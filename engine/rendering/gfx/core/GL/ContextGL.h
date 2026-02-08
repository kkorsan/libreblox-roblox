#pragma once

#include <utility>

namespace RBX
{
namespace Graphics
{

class ContextGL
{
public:
	static ContextGL* create(void* windowHandle, void* display = nullptr);

    virtual ~ContextGL() {}

    virtual void setCurrent() = 0;
    virtual void setVSync(bool enabled) {}
    virtual void swapBuffers() = 0;

    virtual unsigned int getMainFramebufferId() = 0;
    virtual float getMainFramebufferScale() = 0;

    virtual std::pair<unsigned int, unsigned int> updateMainFramebuffer(unsigned int width, unsigned int height) = 0;
};

}
}
