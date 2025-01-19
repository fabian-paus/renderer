// renderer-app.cpp : Defines the entry point for the application.
//

#define WIN32_LEAN_AND_MEAN 

#include "fp_core.h"
#include "fp_allocator.h"
#include "fp_obj.h"
#include "fp_win32.h"
#include "fp_opengl.h"
#include "fp_math.h"
#include "fp_log.h"
#include "fp_renderer.h"

#include <Windows.h>
#include <gl/GL.h>

Renderer g_renderer;
Log g_log;

static void render(int width, int height) {
    glViewport(0, 0, width, height);

    float transformMatrix[16] = {
        2.0f / width, 0.0f,  0.0f, -1.0f,
        0.0f, 2.0f / height, 0.0f, -1.0f,
        0.0f, 0.0f,                1.0f, 0.0f,
        0.0f, 0.0f,                0.0f, 1.0f,
    };
    glUniformMatrix4fv(g_renderer.projectionLocation, 1, GL_TRUE, transformMatrix);

    glViewport(0, 0, width, height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(g_renderer.shaderProgram);
    //float timeInSeconds = 0.001f * ticks;
    //float green = sin(2 * timeInSeconds) / 2.0f + 0.5f;
    //glUniform4f(uniformColorIndex, 0.0, green, 0.0, 1.0);

    g_renderer.render();

    BOOL swapResult = SwapBuffers(g_renderer.deviceContext);
    if (!swapResult) {
        OutputDebugStringW(L"Failed to swap buffers\n");
    }
}

static void flushLog() {
    LogEntry* entry = g_log.beginFlush();
    while (entry != nullptr) {

        entry->message[entry->length] = '\n';
        entry->message[entry->length + 1] = '\0';

        OutputDebugStringA(entry->message);

        entry = next(entry);
    }
    g_log.endFlush();
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

    bool wasClicked(MouseButton button)
    {
        return mouseButtonClicked & ((u16)1 << (u16)button);
    }
};

UserInput g_userInput = {};
bool g_running = false;


// This variable is expected by the linker if floats or doubles are used
extern "C" int _fltused = 0;

// Sometimes the compiler uses memset although we are compiling without standard libary :(
#pragma function(memset)
void* __cdecl memset(void* destination, int value, size_t size) {
    // You should probably look for a more optimized version of memset
    char* dest = (char*)destination;
    for (int i = 0; i < size; ++i) {
        dest[i] = value;
    }
    return dest;
}

// Window procedure handles messages send from the OS
// We want to collect mouse and keyboard input events, so that they are available 
// in an OS independent manner
static LRESULT CALLBACK MainWndProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
        return 0;

    case WM_PAINT:
    {
        // If we do not handle the the WM_PAINT message correctly, i.e use BeginPaint / EndPaint,
        // then PeekMessage will always receive a new WM_PAINT message and we never get to render.
        // I seem to remember that this was not the case in the past :thinking_emoji:
        // If we do not handle the message at all, that we render again.
        // Presumably, because DefWindowProc handles this case correctly.
        return DefWindowProcW(window, message, wParam, lParam);
    }

    case WM_SIZE:
        {
            // We have to render the image here to get smooth resizing behaviour
            int width = LOWORD(lParam);
            int height = LOWORD(lParam);

            render(width, height);
        }
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



void ourGlErrorCallback(GLenum source,
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
    if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) {
        // Do not break on debug notifications
        return;
    }
    DebugBreak();
}

static void fillCommands(RenderCommandBuffer* commands) {
    RenderCommandRectangle rect = {};
    rect.width = 80.0f;
    rect.height = 80.0f;

    float size = 80.0f;
    Color colors[] = { RED, GREEN, BLUE };
    for (int y = 0; y < 3; ++y) {
        rect.y = 100.0f * y;
        rect.color = colors[y];
        for (int x = 0; x < 4; ++x) {
            rect.x = 100.0f * x;
            commands->push(&rect);
        }
    }
}

static int mainFunction()
{
    char buffer[512] = {};

    // TODO: Integrate this into the logging system
    print(buffer, 640, "x", 480, " pixels");

    gl_initialize();

    Allocator pageAllocator = createPageAllocator();
    u64 arenaSize = 16 * KB;
    ArenaWithFallbackAllocator arenaAllocator = createArenaWithFallbackAllocator(&pageAllocator, arenaSize);

#if 0
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
    defer{ model.free(&arenaAllocator); }
#endif 


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
    glDebugMessageCallback(ourGlErrorCallback, nullptr);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);

    // Disable vsync
    wglSwapIntervalEXT(0);

    int renderMemorySize = 1 * MB;
    void* renderMemory = pageAllocator.allocate(renderMemorySize);
    defer{ pageAllocator.free(renderMemory, renderMemorySize); };

    g_renderer.setup(deviceContext, renderMemory, renderMemorySize);

    // TODO: Do only one allocation and partition the memory
    int logMemorySize = 4 * KB;
    void* logMemory = pageAllocator.allocate(logMemorySize);
    defer{ pageAllocator.free(logMemory, logMemorySize); };

    g_log = createLog(logMemory, logMemorySize);

    
    // TODO: Move this into the renderer
    int vertexSize = sizeof(ColoredVertex);
    // The binding index connects the attribute location (here 0) with a specific buffer
    // They do not have to be the same
    int positionBindingIndex = 12; 
    glVertexArrayVertexBuffer(g_renderer.vertexArray, positionBindingIndex, g_renderer.vertexBuffer, 0, vertexSize);
    int positionIndex = 0;
    glVertexArrayAttribFormat(g_renderer.vertexArray, positionIndex, 3, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribBinding(g_renderer.vertexArray, positionIndex, positionBindingIndex);

    int colorBindingIndex = 13;
    glVertexArrayVertexBuffer(g_renderer.vertexArray, colorBindingIndex, g_renderer.vertexBuffer, 3 * sizeof(float), vertexSize);
    int colorIndex = 1;
    glVertexArrayAttribFormat(g_renderer.vertexArray, positionIndex, 3, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribBinding(g_renderer.vertexArray, colorIndex, colorBindingIndex);

    glEnableVertexAttribArray(positionIndex);
    glEnableVertexAttribArray(colorIndex);

    glUseProgram(g_renderer.shaderProgram);

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
        ULONGLONG ticks = GetTickCount64();

        g_userInput.mouseButtonClicked = 0;

        MSG msg = {};
        while (PeekMessageW(&msg, window, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (!g_running) {
            break;
        }

        if (g_userInput.wasClicked(MouseButton::Left))
        {
            char message[] = "Left button clicked";
            g_log.add(message, sizeof(message));
            //OutputDebugStringW(L"Left button clicked\n");
        }

        g_renderer.beginFrame();

        fillCommands(&g_renderer.commands);
        
        RECT rect;
        GetClientRect(window, &rect);
        int renderWidth = rect.right - rect.left;
        int renderHeight = rect.bottom - rect.top;

        render(renderWidth, renderHeight);

        g_renderer.endFrame();

        flushLog();
    }

    return 0;
}

extern "C" int WINAPI WinMainCRTStartup(void) {
    int exitCode = mainFunction();

    // We have to manually exit the process since we do not use the C Standard Library
    ExitProcess(exitCode);
}
