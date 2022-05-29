#pragma once
#include "atlcore.h"                    //AtlLoadSystemLibraryUsingFullPath
#include "atltransactionmanager.h"
#include <string>
#include <functional>
#include <map>
#include <string>


#ifdef DLLLOADER_EXPORT
    #define DLLLOADER_ENTRY __declspec(dllexport)
#else
    #define DLLLOADER_ENTRY __declspec(dllimport)
#endif

class DLLLOADER_ENTRY DllManager
{
public:
    DllManager();
    ~DllManager();

    bool EnableDllRedirection();
    void DisableDllRedirection();

    std::function<std::wstring()> GetLastError;

    /// Sets dll contents for specific path
    bool SetDllFile(const wchar_t* path, const void* dll, int size);
    
    HMODULE LoadLibrary(const wchar_t* path);

    ATL::CAtlTransactionManager transactionManager;
    std::map<std::wstring, HANDLE, std::less<> > path2handle;
    std::map<HANDLE, std::wstring> handle2path;

    bool WinApiCall(bool cond);

private:
    bool MhCall(int r);
};



