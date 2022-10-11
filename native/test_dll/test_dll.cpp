#include <Windows.h>
#include <stdio.h>

extern "C" __declspec(dllexport) void HelloWorld()
{
    printf("Hello world !\r\n");
}

BOOL WINAPI DllMain( HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved)
{
    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
        printf("DLL_PROCESS_ATTACH\n");
        break;

    case DLL_THREAD_ATTACH:
        printf("DLL_THREAD_DETACH\n");
        break;

    case DLL_THREAD_DETACH:
        printf("DLL_THREAD_DETACH\n");
        break;

    case DLL_PROCESS_DETACH:
        printf("DLL_PROCESS_DETACH\n");
        break;
    }
    return TRUE;
}

