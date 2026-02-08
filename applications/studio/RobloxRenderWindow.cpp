/**
 * RobloxRenderWindow.cpp
 * Copyright (c) 2013 ROBLOX Corp. All Rights Reserved.
 */

#include "stdafx.h"
#include "RobloxRenderWindow.h"
#include "RobloxView.h"
#include "ogrewidget.h"
#include "util/KeyCode.h"

#include <QApplication>
#include <QWindow>
#include <QVBoxLayout>
#include <SDL3/SDL_keycode.h>
#ifdef nullptr
#undef nullptr
#endif

#include <qpa/qplatformnativeinterface.h>

// Helper to convert SDL keys to Roblox keys
static RBX::KeyCode sdlKeyCodeToRbxKeyCode(SDL_Keycode key)
{
    // Basic mapping - expand as needed
    if (key >= SDLK_A && key <= SDLK_Z) return (RBX::KeyCode)(key - SDLK_A + 'a');
    if (key >= SDLK_0 && key <= SDLK_9) return (RBX::KeyCode)(key);

    switch (key) {
        case SDLK_SPACE: return RBX::KC_SPACE;
        case SDLK_BACKSPACE: return RBX::KC_BACKSPACE;
        case SDLK_TAB: return RBX::KC_TAB;
        case SDLK_RETURN: return RBX::KC_RETURN;
        case SDLK_ESCAPE: return RBX::KC_ESCAPE;
        case SDLK_DELETE: return RBX::KC_DELETE;
        case SDLK_INSERT: return RBX::KC_INSERT;
        case SDLK_HOME: return RBX::KC_HOME;
        case SDLK_END: return RBX::KC_END;
        case SDLK_PAGEUP: return RBX::KC_PAGEUP;
        case SDLK_PAGEDOWN: return RBX::KC_PAGEDOWN;
        case SDLK_LEFT: return RBX::KC_LEFT;
        case SDLK_RIGHT: return RBX::KC_RIGHT;
        case SDLK_UP: return RBX::KC_UP;
        case SDLK_DOWN: return RBX::KC_DOWN;
        case SDLK_LSHIFT: return RBX::KC_LSHIFT;
        case SDLK_RSHIFT: return RBX::KC_RSHIFT;
        case SDLK_LCTRL: return RBX::KC_LCTRL;
        case SDLK_RCTRL: return RBX::KC_RCTRL;
        case SDLK_LALT: return RBX::KC_LALT;
        case SDLK_RALT: return RBX::KC_RALT;
        case SDLK_F1: return RBX::KC_F1;
        case SDLK_F2: return RBX::KC_F2;
        case SDLK_F3: return RBX::KC_F3;
        case SDLK_F4: return RBX::KC_F4;
        case SDLK_F5: return RBX::KC_F5;
        case SDLK_F6: return RBX::KC_F6;
        case SDLK_F7: return RBX::KC_F7;
        case SDLK_F8: return RBX::KC_F8;
        case SDLK_F9: return RBX::KC_F9;
        case SDLK_F10: return RBX::KC_F10;
        case SDLK_F11: return RBX::KC_F11;
        case SDLK_F12: return RBX::KC_F12;
        default: return RBX::KC_UNKNOWN;
    }
}

static RBX::ModCode sdlModsToRbxMods(Uint16 mods)
{
    unsigned int rbxMods = 0;
    if (mods & SDL_KMOD_SHIFT) rbxMods |= RBX::KC_KMOD_LSHIFT;
    if (mods & SDL_KMOD_CTRL) rbxMods |= RBX::KC_KMOD_LCTRL;
    if (mods & SDL_KMOD_ALT) rbxMods |= RBX::KC_KMOD_LALT;
    return (RBX::ModCode)rbxMods;
}

RobloxRenderWindow::RobloxRenderWindow(QOgreWidget *parentWidget)
    : QObject(parentWidget)
    , m_pParentWidget(parentWidget)
    , m_pRobloxView(nullptr)
    , m_pSDLWindow(nullptr)
    , m_pWrapperWindow(nullptr)
    , m_pContainerWidget(nullptr)
{
    // Initialize SDL Video
    if (!SDL_WasInit(SDL_INIT_VIDEO)) {
        SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    }

    // Set hints for embedding
    SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");

    // Create SDL Window
    Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN;
    m_pSDLWindow = SDL_CreateWindow("RobloxStudioRender", 800, 600, flags);

    if (m_pSDLWindow) {
        // Get Native Window ID
        void* nativeId = nullptr;
        SDL_PropertiesID props = SDL_GetWindowProperties(m_pSDLWindow);

        // Try X11
        if (SDL_HasProperty(props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER)) {
            nativeId = (void*)SDL_GetNumberProperty(props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
        }
        // Try Wayland (Might need surface, but QWindow::fromWinId is tricky with WL)
        // For now assume X11 or XWayland compatibility layer which SDL provides

        if (nativeId) {
            m_pWrapperWindow = QWindow::fromWinId((WId)nativeId);
            if (m_pWrapperWindow) {
                m_pContainerWidget = QWidget::createWindowContainer(m_pWrapperWindow, m_pParentWidget);
                // Make sure container accepts focus
                m_pContainerWidget->setFocusPolicy(Qt::StrongFocus);

                // Show the SDL window now that it's embedded
                SDL_ShowWindow(m_pSDLWindow);
            }
        } else {
             fprintf(stderr, "RobloxRenderWindow: Failed to get native window handle\n");
        }
    } else {
        fprintf(stderr, "RobloxRenderWindow: Failed to create SDL window: %s\n", SDL_GetError());
    }

    // Timer for event polling and rendering
    m_pTimer = new QTimer(this);
    connect(m_pTimer, &QTimer::timeout, this, &RobloxRenderWindow::onTimer);
    m_pTimer->start(8); // ~120Hz polling for smooth input
}

RobloxRenderWindow::~RobloxRenderWindow()
{
    if (m_pSDLWindow) {
        SDL_DestroyWindow(m_pSDLWindow);
    }
}

void RobloxRenderWindow::setRobloxView(RobloxView *rbxView)
{
    m_pRobloxView = rbxView;
}

void RobloxRenderWindow::onTimer()
{
    processSdlEvents();

    if (m_pRobloxView) {
        m_pRobloxView->requestUpdateView();
    }
}

void RobloxRenderWindow::processSdlEvents()
{
    if (!m_pSDLWindow) return;

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        handleSdlEvent(event);
    }
}

void RobloxRenderWindow::handleSdlEvent(const SDL_Event& event)
{
    // Pass event to RobloxView (for Gamepad, etc.)
    if (m_pRobloxView) {
        m_pRobloxView->handleRawSdlEvent(event);
    }

    // Only process events for our window
    if (event.type >= SDL_EVENT_WINDOW_FIRST && event.type <= SDL_EVENT_WINDOW_LAST) {
        if (event.window.windowID != SDL_GetWindowID(m_pSDLWindow))
            return;
    }
    // For input events, we might want to check focus, but SDL generally sends them to focused window

    switch (event.type) {
        case SDL_EVENT_MOUSE_MOTION:
            handleMouseMotion(event.motion);
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
            handleMouseButton(event.button);
            break;
        case SDL_EVENT_MOUSE_WHEEL:
            handleMouseWheel(event.wheel);
            break;
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
            handleKeyboard(event.key);
            break;
        case SDL_EVENT_WINDOW_RESIZED:
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        case SDL_EVENT_WINDOW_EXPOSED:
            handleWindow(event.window);
            break;
        default:
            break;
    }
}

void RobloxRenderWindow::handleMouseMotion(const SDL_MouseMotionEvent& event)
{
    if (!m_pRobloxView) return;

    // Pass coordinates directly
    m_pRobloxView->handleMouse(
        RBX::InputObject::TYPE_MOUSEMOVEMENT,
        RBX::InputObject::INPUT_STATE_CHANGE,
        (int)event.x, (int)event.y,
        sdlModsToRbxMods(SDL_GetModState())
    );
}

void RobloxRenderWindow::handleMouseButton(const SDL_MouseButtonEvent& event)
{
    if (!m_pRobloxView) return;

    RBX::InputObject::UserInputType rbxType = RBX::InputObject::TYPE_NONE;
    if (event.button == SDL_BUTTON_LEFT) rbxType = RBX::InputObject::TYPE_MOUSEBUTTON1;
    else if (event.button == SDL_BUTTON_RIGHT) rbxType = RBX::InputObject::TYPE_MOUSEBUTTON2;
    else if (event.button == SDL_BUTTON_MIDDLE) rbxType = RBX::InputObject::TYPE_MOUSEBUTTON3;

    if (rbxType != RBX::InputObject::TYPE_NONE) {
        m_pRobloxView->handleMouse(
            rbxType,
            event.down ? RBX::InputObject::INPUT_STATE_BEGIN : RBX::InputObject::INPUT_STATE_END,
            (int)event.x, (int)event.y,
            sdlModsToRbxMods(SDL_GetModState())
        );
    }
}

void RobloxRenderWindow::handleMouseWheel(const SDL_MouseWheelEvent& event)
{
    if (!m_pRobloxView) return;

    float delta = event.y;
    int mouseX = 0, mouseY = 0;

    // Get mouse position for wheel event
    float fx, fy;
    SDL_GetMouseState(&fx, &fy);
    mouseX = (int)fx;
    mouseY = (int)fy;

    m_pRobloxView->handleScrollWheel(delta, mouseX, mouseY);
}

void RobloxRenderWindow::handleKeyboard(const SDL_KeyboardEvent& event)
{
    if (!m_pRobloxView || event.repeat) return;

    m_pRobloxView->handleKey(
        RBX::InputObject::TYPE_KEYBOARD,
        event.down ? RBX::InputObject::INPUT_STATE_BEGIN : RBX::InputObject::INPUT_STATE_END,
        sdlKeyCodeToRbxKeyCode(event.key),
        sdlModsToRbxMods(event.mod),
        0, // char code - hard to get from SDL_KeyboardEvent simply, but usually handled by TextInput
        false
    );
}

void RobloxRenderWindow::handleWindow(const SDL_WindowEvent& event)
{
    if (!m_pRobloxView) return;

    if (event.type == SDL_EVENT_WINDOW_RESIZED || event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
        m_pRobloxView->setBounds(event.data1, event.data2);
    }

    // Propagate focus
    if (event.type == SDL_EVENT_WINDOW_FOCUS_GAINED) {
        m_pRobloxView->handleFocus(true);
    } else if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
        m_pRobloxView->handleFocus(false);
    }
}
