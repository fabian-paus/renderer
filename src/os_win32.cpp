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
    {
        // Paint the window's client area. 
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(window, &ps);

        // do drawing to 'dc' here -- or don't

        EndPaint(window, &ps);
    }
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

typedef BOOL WINAPI wglChoosePixelFormatARBF(HDC hdc,
    const int* piAttribIList,
    const FLOAT* pfAttribFList,
    UINT nMaxFormats,
    int* piFormats,
    UINT* nNumFormats);
static wglChoosePixelFormatARBF* wglChoosePixelFormatARB;

typedef BOOL WINAPI wglSwapIntervalEXTF(int interval);
static wglSwapIntervalEXTF* wglSwapIntervalEXT;

typedef const GLubyte* glGetStringiF(GLenum name, GLuint index);
static glGetStringiF* glGetStringi;

typedef void DEBUGPROCF(GLenum source,
    GLenum type,
    GLuint id,
    GLenum severity,
    GLsizei length,
    const char* message,
    const void* userParam);
typedef DEBUGPROCF* DEBUGPROC;

typedef void glDebugMessageCallbackF(DEBUGPROC callback, const void* userParam);
static glDebugMessageCallbackF* glDebugMessageCallback;

#define WGL_NUMBER_PIXEL_FORMATS_ARB      0x2000
#define WGL_DRAW_TO_WINDOW_ARB            0x2001
#define WGL_DRAW_TO_BITMAP_ARB            0x2002
#define WGL_ACCELERATION_ARB              0x2003
#define WGL_NEED_PALETTE_ARB              0x2004
#define WGL_NEED_SYSTEM_PALETTE_ARB       0x2005
#define WGL_SWAP_LAYER_BUFFERS_ARB        0x2006
#define WGL_SWAP_METHOD_ARB               0x2007
#define WGL_NUMBER_OVERLAYS_ARB           0x2008
#define WGL_NUMBER_UNDERLAYS_ARB          0x2009
#define WGL_TRANSPARENT_ARB               0x200A
#define WGL_TRANSPARENT_RED_VALUE_ARB     0x2037
#define WGL_TRANSPARENT_GREEN_VALUE_ARB   0x2038
#define WGL_TRANSPARENT_BLUE_VALUE_ARB    0x2039
#define WGL_TRANSPARENT_ALPHA_VALUE_ARB   0x203A
#define WGL_TRANSPARENT_INDEX_VALUE_ARB   0x203B
#define WGL_SHARE_DEPTH_ARB               0x200C
#define WGL_SHARE_STENCIL_ARB             0x200D
#define WGL_SHARE_ACCUM_ARB               0x200E
#define WGL_SUPPORT_OPENGL_ARB            0x2010
#define WGL_DOUBLE_BUFFER_ARB             0x2011
#define WGL_PIXEL_TYPE_ARB                0x2013
#define WGL_COLOR_BITS_ARB                0x2014
#define WGL_DEPTH_BITS_ARB                0x2022
#define WGL_TYPE_RGBA_ARB                 0x202B

#define WGL_CONTEXT_MAJOR_VERSION_ARB     0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB     0x2092
#define WGL_CONTEXT_FLAGS_ARB             0x2094
#define WGL_CONTEXT_DEBUG_BIT_ARB         0x00000001
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB  0x00000001
#define WGL_CONTEXT_PROFILE_MASK_ARB      0x9126

#define GL_NUM_EXTENSIONS 33309

#define GL_MAJOR_VERSION 0x821B
#define GL_MINOR_VERSION 0x821C

#define GL_DEBUG_OUTPUT 0x92E0
#define GL_DEBUG_OUTPUT_SYNCHRONOUS       0x8242

void glMsgCallback(GLenum source,
    GLenum type,
    GLuint id,
    GLenum severity,
    GLsizei length,
    const char* message,
    const void* userParam)
{
    OutputDebugStringW(L"GL DEBUG: ");
    OutputDebugStringA(message);
    OutputDebugStringW(L"\n");
}

#define Assert(condition, message) if (!condition) { OutputDebugStringA(message); OutputDebugStringW(L"\n"); ExitProcess(-1); }

static void initOpenGL() {
    // to get WGL functions we need valid GL context, so create dummy window for dummy GL context
    HWND dummy = CreateWindowExW(
        0, L"STATIC", L"DummyWindow", WS_OVERLAPPED,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        NULL, NULL, NULL, NULL);
    Assert(dummy, "Failed to create dummy window");

    HDC dc = GetDC(dummy);
    Assert(dc, "Failed to get device context for dummy window");

    PIXELFORMATDESCRIPTOR desc = {};
    desc.nSize = sizeof(desc);
    desc.nVersion = 1;
    desc.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    desc.iPixelType = PFD_TYPE_RGBA;
    desc.cColorBits = 24;


    int format = ChoosePixelFormat(dc, &desc);
    if (!format)
    {
        Assert(false, "Cannot choose OpenGL pixel format for dummy window!");
    }

    int ok = DescribePixelFormat(dc, format, sizeof(desc), &desc);
    Assert(ok, "Failed to describe OpenGL pixel format");

    // reason to create dummy window is that SetPixelFormat can be called only once for the window
    if (!SetPixelFormat(dc, format, &desc))
    {
        Assert(false, "Cannot set OpenGL pixel format for dummy window!");
    }

    HGLRC rc = wglCreateContext(dc);
    Assert(rc, "Failed to create OpenGL context for dummy window");

    ok = wglMakeCurrent(dc, rc);

    glGetStringi = (glGetStringiF*)wglGetProcAddress("glGetStringi");
    wglCreateContextAttribsARB = (wglCreateContextAttribsARBF*)wglGetProcAddress("wglCreateContextAttribsARB");
    glDebugMessageCallback = (glDebugMessageCallbackF*)wglGetProcAddress("glDebugMessageCallback");
    wglChoosePixelFormatARB = (wglChoosePixelFormatARBF*)wglGetProcAddress("wglChoosePixelFormatARB");
    wglSwapIntervalEXT = (wglSwapIntervalEXTF*)wglGetProcAddress("wglSwapIntervalEXT");

    wglMakeCurrent(dc, nullptr);
    wglDeleteContext(rc);
    ReleaseDC(dummy, dc);
    DestroyWindow(dummy);
}

static HGLRC setupOpenGL(HDC hdc) {
    int attrib[] =
    {
        WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
        WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
        WGL_DOUBLE_BUFFER_ARB,  GL_TRUE,
        WGL_PIXEL_TYPE_ARB,     WGL_TYPE_RGBA_ARB,
        WGL_COLOR_BITS_ARB,     24,
        WGL_DEPTH_BITS_ARB,     24,

        // uncomment for sRGB framebuffer, from WGL_ARB_framebuffer_sRGB extension
        // https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_framebuffer_sRGB.txt
        //WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB, GL_TRUE,

        // uncomment for multisampled framebuffer, from WGL_ARB_multisample extension
        // https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_multisample.txt
        //WGL_SAMPLE_BUFFERS_ARB, 1,
        //WGL_SAMPLES_ARB,        4, // 4x MSAA

        0,
    };

    int formatIndex;
    UINT formats;
    if (!wglChoosePixelFormatARB(hdc, attrib, NULL, 1, &formatIndex, &formats) || formats == 0)
    {
        Assert(false, "OpenGL does not support required pixel format!");
    }

    PIXELFORMATDESCRIPTOR format = { };
    DescribePixelFormat(hdc, formatIndex, sizeof(format), &format);

    BOOL setResult = SetPixelFormat(hdc, formatIndex, &format);
    if (!setResult) {
        OutputDebugStringW(L"Could not set pixel format\n");
        ExitProcess(-1);
    }

    //// Lookup extensions
    //GLint numExtensions = 0;
    //glGetIntegerv(GL_NUM_EXTENSIONS, &numExtensions);
    //OutputDebugStringW(L"OpenGL extensions:\n");
    //for (int i = 0; i < numExtensions; ++i) {
    //    const char* extension = (const char*)glGetStringi(GL_EXTENSIONS, i);
    //    OutputDebugStringW(L" - ");
    //    OutputDebugStringA(extension);
    //    OutputDebugStringW(L"\n");
    //}

    int attribs[] =
    {
        WGL_CONTEXT_MAJOR_VERSION_ARB, 4,
        WGL_CONTEXT_MINOR_VERSION_ARB, 6,
        WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
        WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_DEBUG_BIT_ARB,
        0
    };

    HGLRC glContext = wglCreateContextAttribsARB(hdc, 0, attribs);
    if (!glContext) {
        OutputDebugStringW(L"Could not create real OpenGL context\n");
        ExitProcess(-1);
    }

    wglMakeCurrent(hdc, glContext);

    glDebugMessageCallback(glMsgCallback, nullptr);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);

    int versionMajor = 0; 
    int versionMinor = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &versionMajor);
    glGetIntegerv(GL_MINOR_VERSION, &versionMinor);


    OutputDebugStringW(L"OpenGL version: ");
    const char version[] = { '0' + versionMajor, '.', '0' + versionMinor, '\n', '\0'};
    OutputDebugStringA(version);

    return glContext;
}

void checkErrorGL() {
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        OutputDebugStringW(L"GL: Some error occured\n");
    }

}


extern "C" int WINAPI WinMainCRTStartup(void)
{
    initOpenGL();

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

    int windowWidth = 1024;
    int windowHeight = 768;
    HWND window = CreateWindowExW(
        0,                              // Optional window styles.
        windowClassName,                     // Window class
        L"Window",    // Window text
        WS_OVERLAPPEDWINDOW,            // Window style

        // Size and position
        CW_USEDEFAULT, CW_USEDEFAULT, windowWidth, windowHeight,

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
    HGLRC glContext = setupOpenGL(windowDC);

    wglSwapIntervalEXT(0);

    ShowWindow(window, SW_SHOW);

    // enable alpha blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // disble depth testing
    glDisable(GL_DEPTH_TEST);

    // disable culling
    glDisable(GL_CULL_FACE);

    g_running = true;
    while (g_running)
    {
        g_userInput.mouseButtonClicked = 0;

        MSG msg = {};
        while (PeekMessageW(&msg, window, 0, 0, PM_REMOVE))
        {
            //if (msg.message == WM_PAINT) {
            //    break;
            //}
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        if (g_userInput.isDown(MouseButton::Left))
        {
            OutputDebugStringW(L"Left button clicked\n");
        }
        
        RECT rect;
        GetClientRect(window, &rect);
        int currentWidth = rect.right - rect.left;
        int currentHeight = rect.bottom - rect.top;

        glViewport(0, 0, currentWidth, currentHeight);
        glClearColor(1.0f, 0.0f, 1.0f, 1.0f);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        BOOL swapResult = SwapBuffers(windowDC);
        if (!swapResult) {
            OutputDebugStringW(L"Failed to swap buffers\n");
            DWORD error = GetLastError();
        }
        /*glFinish();*/

        OutputDebugStringW(L"Frame rendered\n");
    }


    model.free(&arenaAllocator);

    // We have to manually exit the process since we do not use the C Standard Library
    ExitProcess(0);
    return 0;
}
