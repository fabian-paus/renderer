#pragma once

#include <Windows.h>

void win32_printLastError(const char* context) {
	DWORD error = GetLastError();
	if (error != 0) {
		const wchar_t* buffer = 0;
		DWORD count = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
			nullptr, error, 0, (wchar_t*)&buffer, 0, nullptr);

		OutputDebugStringW(L"Win32 error: ");
		OutputDebugStringA(context);
		OutputDebugStringW(L"\n");

		OutputDebugStringW(buffer);
		OutputDebugStringW(L"\n");
	}
	DebugBreak();
}

#define win32_handleError(condition, context) if (condition) { \
	win32_printLastError(context); \
	ExitProcess(0); \
}
