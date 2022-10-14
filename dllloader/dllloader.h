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

    bool IsEnabled() const;

    bool EnableDllRedirection();
    void DisableDllRedirection();

    std::function<std::wstring()> GetLastError;

    // Same as LoadLibrary, except loads .dll into ram avoiding lock to file.
    // returns 0 if failure, DllManager::GetLastError() can be used to retrieve error message.
    HMODULE RamLoadLibrary(const wchar_t* path);

    /// <summary>
    /// Sets dll contents for specific path
    /// </summary>
    /// <param name="origpath">origpath is used only by managed dll - since .dll itself contains assembly name.
    /// origpath will be redirected to path instead. use nullptr for native .dll's
    /// </param>
    /// <param name="path">.dll path to fake</param>
    /// <param name="dll">dll binary itself</param>
    /// <param name="size">dll size</param>
    bool SetDllFile(const wchar_t* origpath,const wchar_t* path, const void* dll, int size);
    
    HMODULE LoadLibrary(const wchar_t* path);

    ATL::CAtlTransactionManager transactionManager;
    std::map<std::wstring, HANDLE, std::less<> > path2handle;
    std::map<std::wstring,std::wstring, std::less<> > path2path;
    std::map<HANDLE, std::wstring> handle2path;

    bool WinApiCall(bool cond);

private:
    bool MhCall(int r);
    bool hooksEnabled;
};



