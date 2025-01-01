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

Allocator createPageAllocator()
{
    Allocator allocator = {};
    allocator.allocateFunction = +[](Allocator* context, u64 size) -> void*
    {
        return VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    };
    allocator.freeFunction = +[](Allocator* context, void* data, u64 size)
    {
        VirtualFree(data, 0, MEM_RELEASE);
    };
    return allocator;
}

struct ReadFileResult
{
    u8* data;
    i64 size;
    i64 error;
    wchar_t const* errorText;
};

ReadFileResult readEntireFile(wchar_t const* filename)
{
    ReadFileResult result = {};

    // Open file
    HANDLE file = CreateFileW(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (file == INVALID_HANDLE_VALUE)
    {
        result.errorText = L"CreateFileW failed";
        result.error = GetLastError();
        return result;
    }
    defer{ CloseHandle(file); };

    // Get file size
    LARGE_INTEGER fileSize = {};
    if (!GetFileSizeEx(file, &fileSize))
    {
        result.errorText = L"GetFileSizeEx failed";
        result.error = GetLastError();
        return result;
    }

    // Read file contents
    i64 bufferSize = fileSize.QuadPart;
    u8* buffer = (u8*)VirtualAlloc(NULL, bufferSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    u8* writeCursor = buffer;
    i64 bytesRemaining = bufferSize;
    while (bytesRemaining > 0)
    {
        DWORD bytesToRead = bytesRemaining > MAXDWORD ? MAXDWORD : (DWORD)bytesRemaining;
        DWORD bytesRead = 0;
        if (!ReadFile(file, writeCursor, bytesToRead, &bytesRead, NULL))
        {
            result.errorText = L"ReadFile failed";
            result.error = GetLastError();
            return result;
        }
        writeCursor += bytesRead;
        bytesRemaining -= bytesRead;
    }

    // Successfully read entire file contents
    result.data = buffer;
    result.size = fileSize.QuadPart;
    return result;
}

void freeReadFileResult(ReadFileResult* result)
{
    if (result->data)
    {
        // Memory size must be 0 if MEM_RELEASE is specified
        // This frees the entire allocation made by VirtualAlloc
        VirtualFree(result->data, 0, MEM_RELEASE);
    }
}
