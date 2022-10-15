#include "dllloader.h"
#include <iostream>         //ifstream
#include <conio.h>
#include <fstream>
#include <filesystem>       //weakly_canonical, path

using namespace std;
using namespace std::filesystem;


bool LoadFile(path file, string& fileContents)
{
    ifstream is;
    is.exceptions(is.exceptions() | std::ios::failbit);   //throw exception on failure

    try {
        is.open(file, ios::binary);
        is.seekg(0, ios::end);
        int size = is.tellg();
        is.seekg(0, ios::beg);
        fileContents.resize(size);
        is.read(&fileContents[0], size);
        is.close();
    }
    catch (ios_base::failure& e)
    {
        std::cerr << e.what() << '\n';
        return false;
    }

    return true;
}


int wmain(int argc, wchar_t** argv)
{
    path baseDir = weakly_canonical(path(argv[0])).parent_path();

    string testDllFile;
    string testPdbFile;
    path pdbFilePath = baseDir / L"test_managed_dll_new.pdb";       // redirected debug symbols path
    path pdbOrigFilePath = baseDir / L"test_managed_dll.pdb";       // official debug symbols path

    if (!LoadFile(baseDir / L"test_managed_dll.dll", testDllFile) )
    {
        return -2;
    }

    LoadFile(pdbOrigFilePath, testPdbFile);

    //printf(".dll loaded, any key...");
    //_getch();

    DllManager dllm;
    path dllFilePath = baseDir / L"test_managed_dll_new.dll";       // redirected assembly path
    path dllOrigFilePath = baseDir / L"test_managed_dll.dll";       // official assembly path

    dllm.SetDllFile(dllOrigFilePath.c_str(), dllFilePath.c_str(), &testDllFile[0], testDllFile.size());
    if (testPdbFile.length())
    {
        dllm.SetDllFile(pdbOrigFilePath.c_str(), pdbFilePath.c_str(), &testPdbFile[0], testPdbFile.size());
    }

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
    _getch();

    return 0;
}

