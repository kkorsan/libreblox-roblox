#include "stdafx.h"
#include "DummyWindow.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdexcept>
#include <cstdlib>

void createDummyWindow(void*& pHwnd, int width, int height)
{
    // Allocate handle structure
    DummyWindowHandle* handle = new DummyWindowHandle();
    handle->display = nullptr;
    handle->window = 0;
    handle->width = width;
    handle->height = height;

    // Check for headless mode
    if (const char* headless = std::getenv("RBX_HEADLESS"))
    {
        pHwnd = handle;
        return;
    }

    // Open the X11 display
    Display* display = XOpenDisplay(nullptr);
    if (!display)
    {
        // Fallback to headless mode if X11 is unavailable
        pHwnd = handle;
        return;
    }

    int screen = DefaultScreen(display);
    Window root = RootWindow(display, screen);

    // Create a minimal window (1x1 by default)
    Window win = XCreateSimpleWindow(display, root, 0, 0, width, height, 0,
                                     WhitePixel(display, screen),
                                     WhitePixel(display, screen));

    // Map the window so it exists in the server and flush the requests
    XMapWindow(display, win);
    XFlush(display);

    // Store platform handles in our opaque handle structure
    handle->display = reinterpret_cast<void*>(display);
    handle->window = static_cast<std::uintptr_t>(win);

    // Return the handle via the out parameter
    pHwnd = handle;
}

void destroyDummyWindow(void*& pHwnd)
{
    DummyWindowHandle* handle = static_cast<DummyWindowHandle*>(pHwnd);
    if (!handle)
        return;

    Display* display = static_cast<Display*>(handle->display);
    Window win = static_cast<Window>(handle->window);

    if (display)
    {
        XDestroyWindow(display, win);
        XFlush(display);
        XCloseDisplay(display);
    }

    delete handle;
    pHwnd = nullptr;
}
