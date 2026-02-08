
#include <SDL3/SDL_video.h>
#ifdef nullptr
#undef nullptr
#endif

#if !defined(RBX_RCC_SECURITY) && !defined(RBX_TEST_BUILD)
#include "ContextGL.h"

#include "HeadersGL.h"

#include "rbx/Debug.h"

#include <SDL3/SDL.h>
#ifdef nullptr
#undef nullptr
#endif

FASTFLAG(DebugGraphicsGL)
LOGGROUP(Graphics)

namespace RBX
{
namespace Graphics
{
class ContextGLSDL: public ContextGL
{
public:
    ContextGLSDL(void* windowHandle)
        : window(static_cast<SDL_Window*>(windowHandle))
        , glContext(nullptr)
    {
        // Set OpenGL attributes
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
        SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

        // Try to create OpenGL 2.1 context
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);

        glContext = SDL_GL_CreateContext(window);
        if (!glContext)
        {
            FASTLOG1(FLog::Graphics, "Failed to create OpenGL 2.1 core context: %s", SDL_GetError());

            // Try compatibility profile
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
            glContext = SDL_GL_CreateContext(window);
            if (!glContext)
            {
                FASTLOG1(FLog::Graphics, "Failed to create OpenGL 2.1 compatibility context: %s", SDL_GetError());

                throw RBX::runtime_error("Error creating OpenGL context: %s", SDL_GetError());
            }
        }

        // Make context current
        if (!SDL_GL_MakeCurrent(window, glContext))
        {
            SDL_GL_DestroyContext(glContext);
            throw RBX::runtime_error("Error making OpenGL context current: %s", SDL_GetError());
        }

        // Initialize GLEW
        glewInitRBX();
    }

    ~ContextGLSDL()
    {
        if (glContext)
        {
            SDL_GL_DestroyContext(glContext);
        }
    }

    virtual void setCurrent()
    {
        if (SDL_GL_GetCurrentContext() != glContext)
        {
            if (!SDL_GL_MakeCurrent(window, glContext))
            {
                RBXASSERT(false && "Failed to make OpenGL context current");
            }
        }
    }

    virtual void setVSync(bool enabled)
    {
        setCurrent();
        bool result = SDL_GL_SetSwapInterval(enabled ? 1 : 0);
        if (!result)
        {
            FASTLOG1(FLog::Graphics, "SDL_GL_SetSwapInterval failed: %s", SDL_GetError());
        }
        else
        {
            FASTLOG1(FLog::Graphics, "SDL_GL_SetSwapInterval success: %d", enabled);
        }
    }

    virtual void swapBuffers()
    {
        RBXASSERT(SDL_GL_GetCurrentContext() == glContext);
        SDL_GL_SwapWindow(window);
    }

    virtual unsigned int getMainFramebufferId()
    {
        return 0; // Default framebuffer
    }

    virtual float getMainFramebufferScale()
    {
        return SDL_GetWindowDisplayScale(window);
    }

    virtual std::pair<unsigned int, unsigned int> updateMainFramebuffer(unsigned int width, unsigned int height)
    {
        int w, h;
        SDL_GetWindowSizeInPixels(window, &w, &h);
        return std::make_pair(static_cast<unsigned int>(w), static_cast<unsigned int>(h));
    }

private:
    SDL_Window* window;
    SDL_GLContext glContext;
};

ContextGL* ContextGL::create(void* windowHandle, void* display)
{
    return new ContextGLSDL(windowHandle);
}

}
}
#endif
