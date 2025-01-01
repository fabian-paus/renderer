/******************************************************************************
* OpenGL helper functions
*
* Setup OpenGL on Windows
*
* Author: Fabian Paus
*
******************************************************************************/

#pragma once

#include "fp_win32.h""

#include <Windows.h>
#include <gl/GL.h>

// OpenGL on Windows (WGL)

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



// OpenGL extensions

#define GL_NUM_EXTENSIONS 33309

#define GL_MAJOR_VERSION 0x821B
#define GL_MINOR_VERSION 0x821C

#define GL_DEBUG_OUTPUT 0x92E0
#define GL_DEBUG_OUTPUT_SYNCHRONOUS       0x8242

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




static void gl_initialize() {
    HWND dummyWindow = CreateWindowExW(
        0, L"STATIC", L"DummyWindow", WS_OVERLAPPED,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        NULL, NULL, NULL, NULL);
    win32_handleError(!dummyWindow, "Failed to create dummy window");

    HDC dc = GetDC(dummyWindow);
    win32_handleError(!dc, "Failed to get device context for dummy window");

    PIXELFORMATDESCRIPTOR desc = {};
    desc.nSize = sizeof(desc);
    desc.nVersion = 1;
    desc.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    desc.iPixelType = PFD_TYPE_RGBA;
    desc.cColorBits = 24;


    int format = ChoosePixelFormat(dc, &desc);
    win32_handleError(format == 0, "Failed choose OpenGL pixel format for dummy window")

    int describeOk = DescribePixelFormat(dc, format, sizeof(desc), &desc);
    win32_handleError(!describeOk, "Failed to describe OpenGL pixel format");

    // reason to create dummy window is that SetPixelFormat can be called only once for the window
    BOOL setOk = SetPixelFormat(dc, format, &desc);
    win32_handleError(!setOk, "Failed to set OpenGL pixel format for dummy window")

    HGLRC rc = wglCreateContext(dc);
    win32_handleError(!rc, "Failed to create OpenGL context for dummy window");

    BOOL currentOk = wglMakeCurrent(dc, rc);
    win32_handleError(!currentOk, "Failed to make OpenGL context current");


    glGetStringi = (glGetStringiF*)wglGetProcAddress("glGetStringi");
    wglCreateContextAttribsARB = (wglCreateContextAttribsARBF*)wglGetProcAddress("wglCreateContextAttribsARB");
    glDebugMessageCallback = (glDebugMessageCallbackF*)wglGetProcAddress("glDebugMessageCallback");
    wglChoosePixelFormatARB = (wglChoosePixelFormatARBF*)wglGetProcAddress("wglChoosePixelFormatARB");
    wglSwapIntervalEXT = (wglSwapIntervalEXTF*)wglGetProcAddress("wglSwapIntervalEXT");

    wglMakeCurrent(dc, nullptr);
    wglDeleteContext(rc);
    ReleaseDC(dummyWindow, dc);
    DestroyWindow(dummyWindow);
}

static HGLRC gl_createContext(HDC deviceContext) {
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
    BOOL chooseOk = wglChoosePixelFormatARB(deviceContext, attrib, NULL, 1, &formatIndex, &formats);
    win32_handleError(!chooseOk, "OpenGL does not support required pixel format");

    PIXELFORMATDESCRIPTOR format = { };
    int describeOk = DescribePixelFormat(deviceContext, formatIndex, sizeof(format), &format);
    win32_handleError(!describeOk, "Failed to describe pixel format");

    BOOL setOk = SetPixelFormat(deviceContext, formatIndex, &format);
    win32_handleError(!setOk, "Failed to set pixel format");

    int attribs[] =
    {
        WGL_CONTEXT_MAJOR_VERSION_ARB, 4,
        WGL_CONTEXT_MINOR_VERSION_ARB, 6,
        WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
        WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_DEBUG_BIT_ARB,
        0
    };

    HGLRC glContext = wglCreateContextAttribsARB(deviceContext, 0, attribs);
    win32_handleError(!glContext, "Failed to create OpenGL context");

    BOOL currentOk = wglMakeCurrent(deviceContext, glContext);
    win32_handleError(!currentOk, "Failed to make OpenGL context current");

    int versionMajor = 0;
    int versionMinor = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &versionMajor);
    glGetIntegerv(GL_MINOR_VERSION, &versionMinor);


    OutputDebugStringW(L"OpenGL version: ");
    const char version[] = { '0' + versionMajor, '.', '0' + versionMinor, '\n', '\0' };
    OutputDebugStringA(version);

    return glContext;
}

static void gl_printExtensions() {
    GLint numExtensions = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &numExtensions);
    OutputDebugStringW(L"OpenGL extensions:\n");
    for (int i = 0; i < numExtensions; ++i) {
        const char* extension = (const char*)glGetStringi(GL_EXTENSIONS, i);
        OutputDebugStringW(L" - ");
        OutputDebugStringA(extension);
        OutputDebugStringW(L"\n");
    }
}