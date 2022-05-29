#include <stdio.h>

extern "C" __declspec(dllexport) void HelloWorld()
{
    printf("Hello world !\r\n");
}

