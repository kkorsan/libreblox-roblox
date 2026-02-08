#pragma once

#include <cstdint>

// Handle that holds the minimal window/display information for the
// Linux dummy window implementation. The window is stored as an
// integer type (uintptr_t) so it can be safely round-tripped through
// void* APIs where necessary.
struct DummyWindowHandle
{
	void* display;         // X11 Display* (or equivalent platform display pointer)
	uintptr_t window;      // X11 Window as integer (round-trip via reinterpret_cast)
	int width;
	int height;
};

// Creates a small dummy window and returns a pointer to a
// `DummyWindowHandle` via `pHwnd`. Width and height default to 1x1.
void createDummyWindow(void*& pHwnd, int width = 1, int height = 1);
void destroyDummyWindow(void*& pHwnd);