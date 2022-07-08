#include "../utils.h"
#include "../nice_wprintf.h"

int entry(int argc, const wchar_t** argv);

int wmain(int argc, const wchar_t** argv)
{
    const wchar_t* in_argv[] = {L"arroz", L"main.cpp", L"-h"};
    entry(COUNTOF(in_argv), in_argv);

    nice_wprintf(L"olaaa");
}