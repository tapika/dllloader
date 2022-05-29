#include "dllloader.h"
#include <iostream>         //ifstream
#include <fstream>
#include <filesystem>       //weakly_canonical, path

using namespace std;
using namespace std::filesystem;

int wmain(int argc, wchar_t** argv)
{
    path baseDir = weakly_canonical(path(argv[0])).parent_path();

    ifstream is;
    is.exceptions(is.exceptions() | std::ios::failbit);   //throw exception on failure
    string testDllFile;

    try {
        is.open(baseDir / L"test_dll.dll", ios::binary);
        is.seekg(0, ios::end);
        int size = is.tellg();
        is.seekg(0, ios::beg);
        testDllFile.resize(size);
        is.read(&testDllFile[0], size);
        is.close();
    }
    catch (ios_base::failure& e)
    {
        std::cerr << e.what() << '\n';
    }

    DllManager dllm;
    path dllFilePath = temp_directory_path() / L"test_dll.dll";

    dllm.SetDllFile(dllFilePath.c_str(), &testDllFile[0], testDllFile.size());

    if( !dllm.EnableDllRedirection() )
    {
        wprintf(L"%s\n", dllm.GetLastError().c_str());
        return -2;
    }

    // Same as LoadLibrary, only with error handling
    HMODULE dll = dllm.LoadLibrary(dllFilePath.c_str());
    if(dll == 0)
    {
        wprintf(L"%s\n", dllm.GetLastError().c_str());
        return -2;
    }

    void(*helloworld)();
    *((FARPROC*)&helloworld) = GetProcAddress(dll, "HelloWorld");

    if(!dllm.WinApiCall(helloworld != nullptr))
    {
        wprintf(L"%s\n", dllm.GetLastError().c_str());
    }

    printf("- before helloworld()\n");
    helloworld();
    printf("- after helloworld()\n");


    FreeLibrary(dll);

    return 0;
}

