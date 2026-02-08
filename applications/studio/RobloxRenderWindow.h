/**
 * RobloxRenderWindow.h
 * Copyright (c) 2013 ROBLOX Corp. All Rights Reserved.
 *
 * SDL3-based rendering window for native rendering integration
 */

#pragma once

#include <QObject>
#include <QTimer>
#include <QWidget>
#include <QWindow>
#include <SDL3/SDL.h>
#ifdef nullptr
#undef nullptr
#endif


class RobloxView;
class QOgreWidget;

/**
 * @brief Native rendering window using SDL3 and QWindow for modern Qt integration
 *
 * This class creates an SDL3 window and embeds it into a QWidget using
 * QWindow::fromWinId and QWidget::createWindowContainer.
 * It handles input via SDL3 events and forwards them to RobloxView.
 */
class RobloxRenderWindow : public QObject
{
    Q_OBJECT

public:
    explicit RobloxRenderWindow(QOgreWidget *parentWidget);
    ~RobloxRenderWindow();

    void setRobloxView(RobloxView *rbxView);
    RobloxView* robloxView() const { return m_pRobloxView; }

    void startEventTimer();

    QWidget* containerWidget() const { return m_pContainerWidget; }
    QWindow* wrapperWindow() const { return m_pWrapperWindow; }
    SDL_Window* sdlWindow() const { return m_pSDLWindow; }

private Q_SLOTS:
    void onTimer();

private:
    void processSdlEvents();
    void handleSdlEvent(const SDL_Event& event);

    // Input mapping helpers
    void handleMouseMotion(const SDL_MouseMotionEvent& event);
    void handleMouseButton(const SDL_MouseButtonEvent& event);
    void handleMouseWheel(const SDL_MouseWheelEvent& event);
    void handleKeyboard(const SDL_KeyboardEvent& event);
    void handleWindow(const SDL_WindowEvent& event);

    QOgreWidget* m_pParentWidget;
    RobloxView* m_pRobloxView;
    SDL_Window* m_pSDLWindow;
    QWindow* m_pWrapperWindow;
    QWidget* m_pContainerWidget;
    QTimer* m_pTimer;
};
