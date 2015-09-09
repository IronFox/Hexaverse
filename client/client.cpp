// client.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "client.h"

#define MAX_LOADSTRING 100

int main( int argc, const char* argv[]);

bool ConfigureClient(const wchar_t*commandLine);

int APIENTRY _tWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPTSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	if (!ConfigureClient(lpCmdLine))
		return -1;

	return (int) main(0,nullptr);
}

