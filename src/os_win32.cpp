// renderer-app.cpp : Defines the entry point for the application.
//

#define WIN32_LEAN_AND_MEAN 

#include "fp_core.h"
#include "fp_allocator.h"
#include "fp_obj.h"
#include "fp_win32.h"
#include "fp_opengl.h"
#include "fp_math.h"

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

// Sometimes the compiler uses memset although we are compiling without standard libary :(
#pragma function(memset)
void* __cdecl memset(void* destination, int value, size_t size) {
    char* dest = (char*)destination;
    for (int i = 0; i < size; ++i) {
        dest[i] = value;
    }
    return dest;
}

// Window procedure handles messages send from the OS
// We want to collect mouse and keyboard input events, so that they are available 
// in an OS independent manner
LRESULT CALLBACK MainWndProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
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

static const char* VERTEX_SHADER_SIMPLE = 
"#version 330 core\n"
"#line " STR(__LINE__) "\n"
R"(
layout (location = 0) in vec3 pos;

out vec4 vertexColor;

void main()
{
    gl_Position = vec4(pos.x, pos.y, pos.z, 1.0);
    vertexColor = vec4(0.5, 0.0, 0.0, 1.0);
}
)";

static const char* FRAGMENT_SHADER_SIMPLE = 
"#version 330 core\n"
"#line " STR(__LINE__) "\n"
R"(
out vec4 FragColor;

//in vec4 vertexColor;
uniform vec4 uniformColor;

void main()
{
    vec4 color = uniformColor;
    // color.y = sin(color.y) / 2.0f + 0.5f;
    FragColor = color;
} 
)";

static void gl_compileShader(unsigned int shader, const char* source) {
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    int success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512] = {};
        glGetShaderInfoLog(shader, sizeof(infoLog), nullptr, infoLog);
        OutputDebugStringW(L"GL shader compilation errors: \n");
        OutputDebugStringA(infoLog);
        OutputDebugStringW(L"\n");
        DebugBreak();
    }
}

static void gl_linkProgram(unsigned int program) {
    glLinkProgram(program);

    int success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512] = {};
        glGetProgramInfoLog(program, sizeof(infoLog), NULL, infoLog);
        OutputDebugStringW(L"GL shader program link errors: \n");
        OutputDebugStringA(infoLog);
        OutputDebugStringW(L"\n");
        DebugBreak();
    }
}

struct Renderer {
    unsigned int vertexBuffer;

    unsigned int vertexShader;
    unsigned int fragmentShader;
    unsigned int shaderProgram;

    void setup() {
        glGenBuffers(1, &vertexBuffer);

        vertexShader = glCreateShader(GL_VERTEX_SHADER);
        gl_compileShader(vertexShader, VERTEX_SHADER_SIMPLE);

        fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        gl_compileShader(fragmentShader, FRAGMENT_SHADER_SIMPLE);

        shaderProgram = glCreateProgram();
        glAttachShader(shaderProgram, vertexShader);
        glAttachShader(shaderProgram, fragmentShader);
        gl_linkProgram(shaderProgram);
    }
};

static int mainFunction()
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
    glDebugMessageCallback(ourGlErrorCallback, nullptr);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);

    // Disable vsync
    wglSwapIntervalEXT(0);

    Renderer renderer = {};
    renderer.setup();

    float vertices[] = {
        -0.5f, -0.5f, 0.0f,
        0.5f, -0.5f, 0.0f,
        0.0f,  0.5f, 0.0f
    };  

    unsigned int vertexArray = 0;
    glGenVertexArrays(1, &vertexArray);
    glBindVertexArray(vertexArray);

    // TODO: Abstract this away
    glBindBuffer(GL_ARRAY_BUFFER, renderer.vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    int uniformColorIndex = glGetUniformLocation(renderer.shaderProgram, "uniformColor");

    glUseProgram(renderer.shaderProgram);

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

        if (g_userInput.isDown(MouseButton::Left))
        {
            OutputDebugStringW(L"Left button clicked\n");
        }
        
        RECT rect;
        GetClientRect(window, &rect);
        int currentWidth = rect.right - rect.left;
        int currentHeight = rect.bottom - rect.top;

        glViewport(0, 0, currentWidth, currentHeight);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(renderer.shaderProgram);
        float timeInSeconds = 0.001f * ticks;
        float green = sin(2*timeInSeconds) / 2.0f + 0.5f;
        glUniform4f(uniformColorIndex, 0.0, green, 0.0, 1.0);

        glBindVertexArray(vertexArray);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        BOOL swapResult = SwapBuffers(deviceContext);
        if (!swapResult) {
            OutputDebugStringW(L"Failed to swap buffers\n");
        }
    }


    model.free(&arenaAllocator);

    return 0;
}

extern "C" int WINAPI WinMainCRTStartup(void) {
    int exitCode = mainFunction();

    // We have to manually exit the process since we do not use the C Standard Library
    ExitProcess(exitCode);
}
