#include "dllloader.h"
#include <ranges>			//views::keys
#include <filesystem>		//temp_directory_path
#include "MinHook.h"
#define _NTDLL_SELF_		//No need for ntdll.dll linkage
#include "ntddk.h"
#include <atlconv.h>		//CA2W
#include <atlutil.h>		//AtlGetErrorDescription
#include <format>

using namespace ATL;
using namespace std;
using namespace std::filesystem;

static DllManager* g_dllmanager = nullptr;

//-------------------------------------------------------------------------------------------------------------
using NtOpenFile_pfunc = NTSTATUS(NTAPI*)
	(PHANDLE FileHandle, ACCESS_MASK DesiredAccess, POBJECT_ATTRIBUTES ObjectAttributes, 
	 PIO_STATUS_BLOCK IoStatusBlock, ULONG ShareAccess, ULONG OpenOptions);

NtOpenFile_pfunc NtOpenFile_origfunc;

NTSTATUS WINAPI NtOpenFile_detour(PHANDLE FileHandle, ACCESS_MASK DesiredAccess, POBJECT_ATTRIBUTES ObjectAttributes,
	PIO_STATUS_BLOCK IoStatusBlock, ULONG ShareAccess, ULONG OpenOptions)
{
	if(g_dllmanager != nullptr && ObjectAttributes != nullptr)
	{
		wstring_view ntFileName(ObjectAttributes->ObjectName->Buffer, ObjectAttributes->ObjectName->Length / sizeof(wchar_t));
		if(ntFileName.starts_with(L"\\??\\"))
		{
			auto fileName = ntFileName.substr(4);
			auto it = g_dllmanager->path2handle.find(fileName);
			if(it != g_dllmanager->path2handle.end())
			{
				*FileHandle = it->second;
				return 0;
			}
		}
	}
	
	
	NTSTATUS r = NtOpenFile_origfunc(FileHandle, DesiredAccess, ObjectAttributes, IoStatusBlock, ShareAccess, OpenOptions);
	return r;
}

//-------------------------------------------------------------------------------------------------------------

using NtCreateSection_pfunc = NTSTATUS(NTAPI*)
(PHANDLE SectionHandle, ACCESS_MASK DesiredAccess, POBJECT_ATTRIBUTES ObjectAttributes,
	PLARGE_INTEGER MaximumSize, ULONG SectionPageProtection, ULONG AllocationAttributes, HANDLE FileHandle);

NtCreateSection_pfunc NtCreateSection_origfunc;

NTSTATUS NTAPI NtCreateSection_detour(PHANDLE SectionHandle, ACCESS_MASK DesiredAccess, POBJECT_ATTRIBUTES ObjectAttributes,
	PLARGE_INTEGER MaximumSize, ULONG SectionPageProtection, ULONG AllocationAttributes, HANDLE FileHandle)
{
	if (g_dllmanager != nullptr && g_dllmanager->handle2path.contains(FileHandle))
	{
		return NtCreateSection_origfunc(SectionHandle, DesiredAccess, ObjectAttributes, MaximumSize, PAGE_READONLY, AllocationAttributes, FileHandle);
	}

	return NtCreateSection_origfunc(SectionHandle, DesiredAccess, ObjectAttributes, MaximumSize, SectionPageProtection, AllocationAttributes, FileHandle);
}

//-------------------------------------------------------------------------------------------------------------
using NtClose_pfunc = NTSTATUS(NTAPI*) (HANDLE Handle);

NtClose_pfunc NtClose_origfunc;

NTSTATUS NTAPI NtClose_detour(HANDLE Handle)
{
	if (g_dllmanager != nullptr && g_dllmanager->handle2path.contains(Handle))
	{
		return 0;
	}
		
	auto r = NtClose_origfunc(Handle);
	return r;
}

//-------------------------------------------------------------------------------------------------------------


DllManager::DllManager():
    transactionManager(false, true)
{
	GetLastError = []() -> wstring{ return L""; };
	g_dllmanager = this;
}

DllManager::~DllManager()
{
	g_dllmanager = nullptr;

	DisableDllRedirection();

	//TODO: temporary files cleanup
	auto kv = views::keys(handle2path);
	vector<HANDLE> handles{ kv.begin(), kv.end() };

	for(auto h: handles)
		CloseHandle(h);

	handle2path.clear();
	path2handle.clear();

	transactionManager.Rollback();
}

bool DllManager::MhCall(int r)
{
	if (r != MH_OK)
	{
		GetLastError = [r]() -> wstring
		{
			return format(L"MinHook error: {}", CA2W(MH_StatusToString((MH_STATUS)r)).operator LPWSTR());
		};
		return false;
	}

	return true;
}

const wchar_t* ntdll_dll = L"ntdll.dll";

bool DllManager::EnableDllRedirection()
{
	HMODULE ntdll = GetModuleHandle(ntdll_dll);
	if(!MhCall(MH_Initialize()))
	{
		return false;
	}

	bool b = MhCall(MH_CreateHookApi(ntdll_dll, "NtOpenFile", &NtOpenFile_detour, (LPVOID*)&NtOpenFile_origfunc));
	b &= MhCall(MH_CreateHookApi(ntdll_dll, "NtClose", &NtClose_detour, (LPVOID*)&NtClose_origfunc));
	b &= MhCall(MH_CreateHookApi(ntdll_dll, "NtCreateSection", &NtCreateSection_detour, (LPVOID*)&NtCreateSection_origfunc));

	if(b)
	{
		b &= MhCall(MH_EnableHook((LPVOID)GetProcAddress(ntdll, "NtOpenFile")));
		b &= MhCall(MH_EnableHook((LPVOID)GetProcAddress(ntdll, "NtClose")));
		b &= MhCall(MH_EnableHook((LPVOID)GetProcAddress(ntdll, "NtCreateSection")));

		if(!b)
		{
			DisableDllRedirection();
		}
	}

	return b;
}

void DllManager::DisableDllRedirection()
{
	HMODULE ntdll = GetModuleHandle(ntdll_dll);
	MH_DisableHook((LPVOID)GetProcAddress(ntdll, "NtOpenFile"));
	MH_DisableHook((LPVOID)GetProcAddress(ntdll, "NtClose"));
	MH_DisableHook((LPVOID)GetProcAddress(ntdll, "NtCreateSection"));
}


bool DllManager::WinApiCall(bool cond)
{
	if(cond)
	{
		return true;
	}
	
	DWORD errorCode = ::GetLastError();
	if (errorCode != 0)
	{
		GetLastError = [errorCode]() -> wstring
		{
			return AtlGetErrorDescription(HRESULT_FROM_WIN32(errorCode)).GetBuffer();

		};
		return false;
	}

	return true;
}

bool DllManager::SetDllFile(const wchar_t* path, const void* dll, int size)
{
	// Create dummy file, so can be found by LoadLibrary
	if(!exists(path))
	{
		FILE* out = _wfopen(path, L"wb");
		if(out)
		{
			fclose(out);
		}
	}

	HANDLE hFile = transactionManager.CreateFileW(path, GENERIC_WRITE | GENERIC_READ, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
	if(!WinApiCall(hFile != 0))
	{
		return false;	
	}

	DWORD written = 0;
	bool b = WinApiCall(WriteFile(hFile, dll, size, &written, NULL));
	b &= WinApiCall(FlushFileBuffers(hFile));
	b &= WinApiCall(SetFilePointer(hFile, 0, 0, FILE_BEGIN) != INVALID_SET_FILE_POINTER);
	if(b)
	{
		handle2path[hFile] = path;
		path2handle[path] = hFile;
	}
	else
	{
		CloseHandle(hFile);
		transactionManager.DeleteFileW(path);
	}

	return true;
}

HMODULE DllManager::LoadLibrary(const wchar_t* path)
{
	HMODULE h = ::LoadLibrary(path);
	WinApiCall(h != 0);
	return h;
}


