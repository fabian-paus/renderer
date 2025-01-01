// renderer-app.cpp : Defines the entry point for the application.
//

#define WIN32_LEAN_AND_MEAN 

#include "fp_core.h"
#include "fp_allocator.h"
#include "fp_obj.h"
#include "fp_win32.h"
#include "fp_opengl.h"

#include <Windows.h>
#include <gl/GL.h>

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
        // If we do not handle the the WM_PAINT message correctly, i.e use BeginPaint / EndPaint,
        // then PeekMessage will always receive a new WM_PAINT message and we never get to render.
        // I seem to remember that this was not the case in the past :thinking_emoji:
        // If we do not handle the message at all, that we render again.
        // Presumably, because DefWindowProc handles this case correctly.
        //PAINTSTRUCT ps;
        //HDC dc = BeginPaint(window, &ps);
        //EndPaint(window, &ps);
        //return 0;
        return DefWindowProcW(window, message, wParam, lParam);
    }

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

        g_userInput.mouseX = LOWORD(lParam);
        g_userInput.mouseY = HIWORD(lParam);

        break;

        // 
        // Process other messages. 
        // 

    default:
        return DefWindowProc(window, message, wParam, lParam);
    }
    return 0;
}



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


extern "C" int WINAPI WinMainCRTStartup(void)
{
    gl_initialize();

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

    HDC deviceContext = GetDC(window);
    HGLRC glContext = gl_createContext(deviceContext);

    // Enable debugging
    glDebugMessageCallback(glMsgCallback, nullptr);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);

    // Disable vsync
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
        BOOL swapResult = SwapBuffers(deviceContext);
        if (!swapResult) {
            OutputDebugStringW(L"Failed to swap buffers\n");
        }
    }


    model.free(&arenaAllocator);

    // We have to manually exit the process since we do not use the C Standard Library
    ExitProcess(0);
    return 0;
}
