#if defined(RBX_RCC_SECURITY) || defined(RBX_TEST_BUILD)
#include "ContextGL.h"

#include "HeadersGL.h"

#include "rbx/Debug.h"

#include <X11/Xlib.h>
#include <GL/glx.h>
#include <SDL3/SDL.h>
#ifdef nullptr
#undef nullptr
#endif


#include <EGL/egl.h>
#include <EGL/eglext.h>

#ifndef EGL_PLATFORM_SURFACELESS_MESA
#define EGL_PLATFORM_SURFACELESS_MESA 0x31DD
#endif

FASTFLAG(DebugGraphicsGL)
LOGGROUP(Graphics)

namespace RBX
{
namespace Graphics
{

class ContextGLX11: public ContextGL
{
public:
    ContextGLX11(void* windowHandle, void* displayHandle)
        : display(nullptr)
        , window(static_cast<Window>(reinterpret_cast<uintptr_t>(windowHandle)))
        , glxContext(nullptr)
        , eglDisplay(EGL_NO_DISPLAY)
        , eglContext(EGL_NO_CONTEXT)
        , eglSurface(EGL_NO_SURFACE)
        , isEGL(false)
        , mainFramebufferId(0)
        , mainFramebufferScale(1.0f)
    {
        FASTLOG1(FLog::Graphics, "ContextGLX11: Creating context for window handle %p", windowHandle);

        // Check for headless mode (windowHandle == 0 from DummyWindow or environment check)
        bool headless = (window == 0);

        if (!headless)
        {
            if (displayHandle)
            {
                display = static_cast<Display*>(displayHandle);
                FASTLOG(FLog::Graphics, "ContextGLX11: Using provided X11 display");
            }
            else
            {
                display = XOpenDisplay(nullptr);
                if (!display)
                {
                    FASTLOG(FLog::Graphics, "ContextGLX11: Failed to open X11 display, falling back to EGL headless");
                    headless = true;
                }
                else
                {
                    FASTLOG(FLog::Graphics, "ContextGLX11: Opened X11 display successfully");
                }
            }
        }

        if (headless)
        {
            initEGL();
            return;
        }

        // --- Existing GLX Initialization ---

        // Check if GLX is available
        int glxMajor, glxMinor;
        if (!glXQueryVersion(display, &glxMajor, &glxMinor))
        {
            fprintf(stderr, "ContextGLX11: GLX not available\n");
            FASTLOG(FLog::Graphics, "ContextGLX11: GLX not available");
            if (!displayHandle) XCloseDisplay(display);
            throw RBX::runtime_error("GLX not available");
        }
        fprintf(stderr, "ContextGLX11: GLX version: %d.%d\n", glxMajor, glxMinor);

        FASTLOG2(FLog::Graphics, "ContextGLX11: GLX version: %d.%d", glxMajor, glxMinor);

        // Get the visual from the existing window
        XWindowAttributes windowAttributes;
        if (!XGetWindowAttributes(display, window, &windowAttributes))
        {
            FASTLOG1(FLog::Graphics, "ContextGLX11: Failed to get window attributes for window %lu", window);
            if (!displayHandle) XCloseDisplay(display);
            throw RBX::runtime_error("Failed to get window attributes");
        }
        FASTLOG1(FLog::Graphics, "ContextGLX11: Got window attributes for window %lu", window);

        XVisualInfo visualInfoTemplate;
        visualInfoTemplate.visualid = XVisualIDFromVisual(windowAttributes.visual);

        int numVisuals;
        XVisualInfo* visualInfo = XGetVisualInfo(display, VisualIDMask, &visualInfoTemplate, &numVisuals);
        if (!visualInfo || numVisuals == 0)
        {
            FASTLOG1(FLog::Graphics, "ContextGLX11: Failed to get visual info from window %lu", window);
            if (!displayHandle) XCloseDisplay(display);
            throw RBX::runtime_error("Failed to get visual info from window");
        }
        FASTLOG2(FLog::Graphics, "ContextGLX11: Got visual info for window %lu, visual ID %lu", window, visualInfoTemplate.visualid);

        // Get FBConfig from visual
        GLXFBConfig fbconfig = nullptr;
        int fbcount;
        GLXFBConfig* fbconfigs = glXGetFBConfigs(display, DefaultScreen(display), &fbcount);
        if (fbconfigs)
        {
            for (int i = 0; i < fbcount; ++i)
            {
                int visualid;
                glXGetFBConfigAttrib(display, fbconfigs[i], GLX_VISUAL_ID, &visualid);
                if (visualid == visualInfo->visualid)
                {
                    fbconfig = fbconfigs[i];
                    break;
                }
            }
            XFree(fbconfigs);
        }

        // Create GLX context
        glxContext = nullptr;
        if (fbconfig)
        {
            // Try to create OpenGL 2.1 compatibility context
            PFNGLXCREATECONTEXTATTRIBSARBPROC glXCreateContextAttribsARB = (PFNGLXCREATECONTEXTATTRIBSARBPROC) glXGetProcAddressARB((const GLubyte*)"glXCreateContextAttribsARB");
            if (glXCreateContextAttribsARB)
            {
                int context_attribs[] = {
                    GLX_CONTEXT_MAJOR_VERSION_ARB, 2,
                    GLX_CONTEXT_MINOR_VERSION_ARB, 1,
                    GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
                    None
                };
                glxContext = glXCreateContextAttribsARB(display, fbconfig, nullptr, GL_TRUE, context_attribs);
                if (glxContext)
                {
                    FASTLOG1(FLog::Graphics, "ContextGLX11: Created OpenGL 2.1 compatibility context for window %lu", window);
                }
            }
        }

        // Fallback to legacy context
        if (!glxContext)
        {
            glxContext = glXCreateContext(display, visualInfo, nullptr, GL_TRUE);
            if (glxContext)
            {
                fprintf(stderr, "ContextGLX11: Created legacy GLX context for window %lu\n", window);
                FASTLOG1(FLog::Graphics, "ContextGLX11: Created legacy GLX context for window %lu", window);
            }
        }

        if (!glxContext)
        {
            fprintf(stderr, "ContextGLX11: Failed to create GLX context for window %lu\n", window);
            FASTLOG1(FLog::Graphics, "ContextGLX11: Failed to create GLX context for window %lu", window);
            XFree(visualInfo);
            if (!displayHandle) XCloseDisplay(display);
            throw RBX::runtime_error("Failed to create GLX context");
        }

        // Make context current
        if (!glXMakeCurrent(display, window, glxContext))
        {
            fprintf(stderr, "ContextGLX11: Failed to make GLX context current for window %lu\n", window);
            FASTLOG1(FLog::Graphics, "ContextGLX11: Failed to make GLX context current for window %lu", window);
            glXDestroyContext(display, glxContext);
            XFree(visualInfo);
            if (!displayHandle) XCloseDisplay(display);
            throw RBX::runtime_error("Failed to make GLX context current");
        }
        fprintf(stderr, "ContextGLX11: Made GLX context current for window %lu\n", window);
        FASTLOG1(FLog::Graphics, "ContextGLX11: Made GLX context current for window %lu", window);

        // Initialize GLEW
        glewInitRBX();
        FASTLOG(FLog::Graphics, "ContextGLX11: Initialized GLEW");

        // Get main framebuffer info
        mainFramebufferId = 0; // Default framebuffer

        // Get window scale (for HiDPI)
        int screen = DefaultScreen(display);
        double xdpi = (DisplayWidth(display, screen) * 25.4) / DisplayWidthMM(display, screen);
        double ydpi = (DisplayHeight(display, screen) * 25.4) / DisplayHeightMM(display, screen);
        mainFramebufferScale = std::max(xdpi, ydpi) / 96.0; // Assume 96 DPI as baseline

        FASTLOG1(FLog::Graphics, "ContextGLX11: Context creation successful for window %lu", window);
        XFree(visualInfo);
    }

    void initEGL()
    {
        FASTLOG(FLog::Graphics, "ContextGLX11: Initializing EGL Headless...");
        isEGL = true;

        // Try getting surfaceless platform display (Mesa specific but standard for headless)
        PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXT = (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
        if (eglGetPlatformDisplayEXT)
        {
             eglDisplay = eglGetPlatformDisplayEXT(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, nullptr);
        }

        // Fallback to default display if platform extension missing or failed
        if (eglDisplay == EGL_NO_DISPLAY)
        {
             eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        }

        if (eglDisplay == EGL_NO_DISPLAY)
        {
            throw RBX::runtime_error("Failed to get EGL display");
        }

        EGLint major, minor;
        if (!eglInitialize(eglDisplay, &major, &minor))
        {
            throw RBX::runtime_error("Failed to initialize EGL");
        }
        FASTLOG2(FLog::Graphics, "ContextGLX11: EGL Initialized version %d.%d", major, minor);

        if (!eglBindAPI(EGL_OPENGL_API))
        {
            throw RBX::runtime_error("Failed to bind OpenGL API");
        }

        EGLint configAttribs[] = {
            EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, // We might use pbuffer if surfaceless not supported, but usually ok
            EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
            EGL_RED_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE, 8,
            EGL_ALPHA_SIZE, 8,
            EGL_DEPTH_SIZE, 24,
            EGL_STENCIL_SIZE, 8,
            EGL_NONE
        };

        EGLConfig config;
        EGLint numConfigs;
        if (!eglChooseConfig(eglDisplay, configAttribs, &config, 1, &numConfigs) || numConfigs == 0)
        {
             // Try without PBUFFER bit (maybe surfaceless only)
             configAttribs[1] = 0; // Remove EGL_PBUFFER_BIT
             if (!eglChooseConfig(eglDisplay, configAttribs, &config, 1, &numConfigs) || numConfigs == 0)
                throw RBX::runtime_error("Failed to choose EGL config");
        }

        eglContext = eglCreateContext(eglDisplay, config, EGL_NO_CONTEXT, nullptr);
        if (eglContext == EGL_NO_CONTEXT)
        {
            throw RBX::runtime_error("Failed to create EGL context");
        }

        // Make current with no surface (surfaceless)
        // If driver doesn't support EGL_KHR_surfaceless_context, this might fail.
        // But we are targeting Mesa/modern Linux.
        if (!eglMakeCurrent(eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, eglContext))
        {
            // Fallback: create a tiny pbuffer if surfaceless fails
            EGLint pbufferAttribs[] = {
                EGL_WIDTH, 1,
                EGL_HEIGHT, 1,
                EGL_NONE
            };
            eglSurface = eglCreatePbufferSurface(eglDisplay, config, pbufferAttribs);
            if (eglSurface == EGL_NO_SURFACE )
                 throw RBX::runtime_error("Failed to create EGL pbuffer surface");

            if (!eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext))
                 throw RBX::runtime_error("Failed to make EGL context current");

            FASTLOG(FLog::Graphics, "ContextGLX11: EGL using Pbuffer Surface");
        }
        else
        {
            FASTLOG(FLog::Graphics, "ContextGLX11: EGL Surfaceless Context Active");
        }

        glewInitRBX();
        FASTLOG(FLog::Graphics, "ContextGLX11: Initialized GLEW (EGL)");
    }

    ~ContextGLX11()
    {
        if (isEGL)
        {
            eglMakeCurrent(eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            if (eglSurface != EGL_NO_SURFACE)
                eglDestroySurface(eglDisplay, eglSurface);
            if (eglContext != EGL_NO_CONTEXT)
                eglDestroyContext(eglDisplay, eglContext);
            if (eglDisplay != EGL_NO_DISPLAY)
                eglTerminate(eglDisplay);
        }
        else
        {
            if (glxContext)
            {
                glXMakeCurrent(display, None, nullptr);
                glXDestroyContext(display, glxContext);
            }
        }
    }

    virtual void setCurrent()
    {
        if (isEGL)
        {
            if (eglGetCurrentContext() != eglContext)
            {
                if (!eglMakeCurrent(eglDisplay, eglSurface != EGL_NO_SURFACE ? eglSurface : EGL_NO_SURFACE, eglSurface != EGL_NO_SURFACE ? eglSurface : EGL_NO_SURFACE, eglContext))
                {
                    RBXASSERT(false && "Failed to make EGL context current");
                }
            }
        }
        else
        {
            if (glXGetCurrentContext() != glxContext)
            {
                if (!glXMakeCurrent(display, window, glxContext))
                {
                    RBXASSERT(false && "Failed to make GLX context current");
                }
            }
        }
    }

    virtual void swapBuffers()
    {
        if (isEGL)
        {
            // Nothing to swap for surfaceless, or swap pbuffer (pointless)
            glFlush();
        }
        else
        {
            RBXASSERT(glXGetCurrentContext() == glxContext);
            glXSwapBuffers(display, window);
        }
    }

    virtual unsigned int getMainFramebufferId()
    {
        return mainFramebufferId;
    }

    virtual float getMainFramebufferScale()
    {
        return mainFramebufferScale;
    }

    virtual std::pair<unsigned int, unsigned int> updateMainFramebuffer(unsigned int width, unsigned int height)
    {
        if (isEGL)
        {
             // For headless, we accept whatever size is requested, as there is no real window constraint
             glViewport(0, 0, width, height);
             return std::make_pair(width, height);
        }

        // Get actual window size from X11
        XWindowAttributes windowAttrs;
        if (XGetWindowAttributes(display, window, &windowAttrs))
        {
            width = windowAttrs.width;
            height = windowAttrs.height;

            // Set the OpenGL viewport to match the window size
            glViewport(0, 0, width, height);
        }

        return std::make_pair(width, height);
    }

private:
    Display* display;
    Window window;
    GLXContext glxContext;

    // EGL members
    EGLDisplay eglDisplay;
    EGLContext eglContext;
    EGLSurface eglSurface;
    bool isEGL;

    unsigned int mainFramebufferId;
    float mainFramebufferScale;
};

ContextGL* ContextGL::create(void* windowHandle, void* display)
{
    // Force X11 for Studio (even on Wayland, assuming XWayland)
    return new ContextGLX11(windowHandle, display);
}

} // namespace Graphics
} // namespace RBX
#endif
