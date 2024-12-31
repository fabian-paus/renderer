// renderer-app.cpp : Defines the entry point for the application.
//

#include "fp_core.h"
#include "fp_allocator.h"
#include "fp_obj.h"

#include <Windows.h>
#include <windowsx.h>
#include <gl/GL.h>

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

enum class MouseButton
{
    None   = 0,
    Left   = 1,
    Right  = 2,
    Middle = 3,
};

struct UserInput
{
    u16 mouseButtonState;
    u16 mouseButtonClicked;
    i16 mouseX;
    i16 mouseY;

    bool isDown(MouseButton button)
    {
        return mouseButtonState & ((u16)1 << (u16)button);
    }

    bool isUp(MouseButton button)
    {
        return (mouseButtonState & ((u16)1 << (u16)button)) == 0;
    }
};

UserInput g_userInput = {};
bool g_running = false;


// This variable is expected by the compiler/linker if floats/doubles are used
extern "C" int _fltused = 0;


// Window procedure handles messages send from the OS
// We want to collect mouse and keyboard input events, so that they are available 
// in an OS independent manner
LRESULT CALLBACK MainWndProc(
    HWND window,        // handle to window
    UINT message,        // message identifier
    WPARAM wParam,    // first message parameter
    LPARAM lParam)    // second message parameter
{

    switch (message)
    {
    case WM_CREATE:
        // Initialize the window. 
        return 0;

    case WM_PAINT:
        // Paint the window's client area. 
        return 0;

    case WM_SIZE:
        // Set the size and position of the window. 
        return 0;

    case WM_DESTROY:
        // Clean up window-specific data objects. 
        PostQuitMessage(0);
        g_running = false;
        return 0;

    case WM_CLOSE:
        DestroyWindow(window);
        return 0;

    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
    {
        u16 bitmask = 0;
        if (wParam & MK_LBUTTON)
        {
            bitmask = (u16)1 << (u16)MouseButton::Left;
        }
        else if (wParam & MK_RBUTTON)
        {
            bitmask = (u16)1 << (u16)MouseButton::Right;
        }
        else if (wParam & MK_MBUTTON)
        {
            bitmask = (u16)1 << (u16)MouseButton::Middle;
        }

        g_userInput.mouseButtonState |= bitmask;

        // Should we store the mouse cursor position?
    } break;

    case WM_LBUTTONUP:
    {
        u16 bitmask = (u16)1 << (u16)MouseButton::Left;
        g_userInput.mouseButtonClicked |= bitmask;
        g_userInput.mouseButtonState &= ~bitmask;
    } break;

    case WM_RBUTTONUP:
    {
        u16 bitmask = (u16)1 << (u16)MouseButton::Right;
        g_userInput.mouseButtonClicked |= bitmask;
        g_userInput.mouseButtonState &= ~bitmask;
    } break;

    case WM_MBUTTONUP:
    {
        u16 bitmask = (u16)1 << (u16)MouseButton::Middle;
        g_userInput.mouseButtonClicked |= bitmask;
        g_userInput.mouseButtonState &= ~bitmask;
    } break;

    case WM_MOUSEMOVE:

        g_userInput.mouseX = GET_X_LPARAM(lParam);
        g_userInput.mouseY = GET_Y_LPARAM(lParam);

        break;

        // 
        // Process other messages. 
        // 

    default:
        return DefWindowProc(window, message, wParam, lParam);
    }
    return 0;
}

// OpenGL on Windows (WGL)

typedef HGLRC WINAPI wglCreateContextAttribsARBF(HDC hDC, HGLRC hShareContext, const int* attribList);
static wglCreateContextAttribsARBF* wglCreateContextAttribsARB;

typedef BOOL WINAPI wglGetPixelFormatAttribIvArbF(HDC hdc,
    int iPixelFormat,
    int iLayerPlane,
    UINT nAttributes,
    const int* piAttributes,
    int* piValues);
static wglGetPixelFormatAttribIvArbF* wglGetPixelFormatAttribIvARB;

typedef BOOL WINAPI wglGetPixelFormatAttribFvArbF(HDC hdc,
    int iPixelFormat,
    int iLayerPlane,
    UINT nAttributes,
    const int* piAttributes,
    FLOAT* pfValues);
static wglGetPixelFormatAttribFvArbF* wglGetPixelFormatAttribFvARB;

typedef BOOL WINAPI wglChoosePixelFormatArbF(HDC hdc,
    const int* piAttribIList,
    const FLOAT* pfAttribFList,
    UINT nMaxFormats,
    int* piFormats,
    UINT* nNumFormats);
static wglChoosePixelFormatArbF* wglChoosePixelFormatARB;

typedef const GLubyte* glGetStringiF(GLenum name, GLuint index);
static glGetStringiF* glGetStringi;

#define WGL_CONTEXT_MAJOR_VERSION_ARB     0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB     0x2092
#define WGL_CONTEXT_FLAGS_ARB             0x2094
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB  0x00000001
#define WGL_CONTEXT_PROFILE_MASK_ARB      0x9126

#define GL_NUM_EXTENSIONS 33309

#define GL_MAJOR_VERSION 0x821B
#define GL_MINOR_VERSION 0x821C

static void setupOpenGL(HDC hdc) {
    PIXELFORMATDESCRIPTOR format = {};
    format.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    format.iPixelType = PFD_TYPE_RGBA;
    format.cColorBits = 32;
    format.cDepthBits = 24;
    format.cStencilBits = 8;
    format.iLayerType = PFD_MAIN_PLANE;

    int formatIndex = ChoosePixelFormat(hdc, &format);
    if (formatIndex == 0) {
        OutputDebugStringW(L"Could not choose a fitting pixel format\n");
        ExitProcess(-1);
    }

    BOOL setResult = SetPixelFormat(hdc, formatIndex, &format);
    if (!setResult) {
        OutputDebugStringW(L"Could not set pixel format\n");
        ExitProcess(-1);
    }

    HGLRC tempContext = wglCreateContext(hdc);
    if (!tempContext) {
        OutputDebugStringW(L"Could not create OpenGL context\n");
        ExitProcess(-1);
    }

    wglMakeCurrent(hdc, tempContext);

    glGetStringi = (glGetStringiF*)wglGetProcAddress("glGetStringi");
    wglCreateContextAttribsARB = (wglCreateContextAttribsARBF*)wglGetProcAddress("wglCreateContextAttribsARB");

    // Lookup extensions
    GLint numExtensions = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &numExtensions);
    OutputDebugStringW(L"OpenGL extensions:\n");
    for (int i = 0; i < numExtensions; ++i) {
        const char* extension = (const char*)glGetStringi(GL_EXTENSIONS, i);
        OutputDebugStringW(L" - ");
        OutputDebugStringA(extension);
        OutputDebugStringW(L"\n");
    }

    int attribs[] =
    {
        WGL_CONTEXT_MAJOR_VERSION_ARB, 4,
        WGL_CONTEXT_MINOR_VERSION_ARB, 6,
        WGL_CONTEXT_FLAGS_ARB, 0,
        0
    };

    HGLRC glContext = wglCreateContextAttribsARB(hdc, 0, attribs);
    if (!glContext) {
        OutputDebugStringW(L"Could not create real OpenGL context\n");
        ExitProcess(-1);
    }

    wglMakeCurrent(hdc, glContext);
    wglDeleteContext(tempContext);

    int versionMajor = 0; 
    int versionMinor = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &versionMajor);
    glGetIntegerv(GL_MINOR_VERSION, &versionMinor);


    OutputDebugStringW(L"OpenGL version: ");
    const char version[] = { '0' + versionMajor, '.', '0' + versionMinor, '\n', '\0'};
    OutputDebugStringA(version);
    //wglGetProcAddress()
}


extern "C" int WINAPI WinMainCRTStartup(void)
{
    Allocator pageAllocator = createPageAllocator();
    u64 arenaSize = 16 * KB;
    ArenaWithFallbackAllocator arenaAllocator = createArenaWithFallbackAllocator(&pageAllocator, arenaSize);

    wchar_t const* filename = L"data/Deer.obj";

    ReadFileResult fileResult = readEntireFile(filename);
    defer{ freeReadFileResult(&fileResult); };

    if (fileResult.error)
    {
        OutputDebugStringW(L"Could not read from file due to the following error:\n");
        OutputDebugStringW(fileResult.errorText);
        OutputDebugStringW(L"\n");
        // TODO: Add error code and format the message according to the error code
        return 1;
    }

    OutputDebugStringW(L"Read file content successfully!\n");

    ObjModel model = parseObjModel(fileResult.data, fileResult.size, &arenaAllocator);


    // Create a window
    wchar_t const* windowClassName = L"FP_WindowClass";
    WNDCLASSEXW windowClass = {};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_OWNDC;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.lpszClassName = windowClassName;
    windowClass.lpfnWndProc = &MainWndProc;
    RegisterClassExW(&windowClass);

    HWND window = CreateWindowExW(
        0,                              // Optional window styles.
        windowClassName,                     // Window class
        L"Window",    // Window text
        WS_OVERLAPPEDWINDOW,            // Window style

        // Size and position
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,

        NULL,       // Parent window    
        NULL,       // Menu
        windowClass.hInstance,  // Instance handle
        NULL        // Additional application data
    );

    if (window == nullptr)
    {
        // TODO: Add error message if window cannot be created
        return 1;
    }

    HDC windowDC = GetDC(window);
    setupOpenGL(windowDC);

    ShowWindow(window, SW_SHOW);

    g_running = true;
    while (g_running)
    {
        g_userInput.mouseButtonClicked = 0;

        MSG msg = {};
        while (PeekMessageW(&msg, window, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        if (g_userInput.isDown(MouseButton::Left))
        {
            OutputDebugStringW(L"Left button clicked\n");
        }
    }


    model.free(&arenaAllocator);

    // We have to manually exit the process since we do not use the C Standard Library
    ExitProcess(0);
    return 0;
}
