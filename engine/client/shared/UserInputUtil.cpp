/**
 * UserInputUtil.cpp
 * Copyright (c) 2013 Nicolas Industries All Rights Reserved.
 */

#include "util/KeyCode.h"
#include "SDL3/SDL.h"

#include "stdafx.h"
#include "UserInputUtil.h"
#include "util/Rect.h"
#include "util/standardout.h"
#include "v8datamodel/GameBasicSettings.h"

using namespace RBX;

using RBX::Vector2;
using RBX::Rect;

const float UserInputUtil::HybridSensitivity = 15.0f;
const float UserInputUtil::MouseTug = 20.0f;

bool UserInputUtil::isCtrlDown(RBX::ModCode modCode)
{
	return (modCode == RBX::KC_KMOD_LCTRL || modCode == RBX::KC_KMOD_RCTRL);
}

void UserInputUtil::wrapMouseBorder(const Vector2& delta,
	Vector2& wrapMouseDelta,
	Vector2& wrapMousePosition,
	const Vector2& windowSize,
	const int borderWidth,
	const float creepFactor)

{
	Vector2 halfSize = windowSize * 0.5;
	Rect inner = Rect(-halfSize, halfSize).inset(borderWidth);	// in Wrap Coordinates

	Vector2 newPositionUnclamped = wrapMousePosition + delta;
	Vector2 newPositionClamped = inner.clamp(newPositionUnclamped);

	Vector2 positiveDistanceInBorder = newPositionUnclamped - newPositionClamped;

	wrapMousePosition = newPositionClamped;
	wrapMouseDelta += positiveDistanceInBorder;
}

void UserInputUtil::wrapMouseBorderLock(const Vector2& delta,
	Vector2& wrapMouseDelta,
	Vector2& wrapMousePosition,
	const Vector2& windowSize)
{
	wrapMouseBorder(
		delta,
		wrapMouseDelta,
		wrapMousePosition,
		windowSize,
		6,
		0.0f);
}

void UserInputUtil::wrapMouseNone(const Vector2& delta,
	Vector2& wrapMouseDelta,
	Vector2& wrapMousePosition)
{
	wrapMouseDelta = Vector2::zero();
	wrapMousePosition += delta;
}

void UserInputUtil::wrapMouseCenter(const Vector2& delta,
	Vector2& wrapMouseDelta,
	Vector2& wrapMousePosition)
{
	wrapMouseDelta += delta * GameBasicSettings::singleton().getCameraSensitivity();
}

G3D::Vector2 UserInputUtil::sdlEventToVector2(SDL_Event* event)
{
	switch (event->type) {
	case SDL_EVENT_MOUSE_MOTION:
		return G3D::Vector2(event->motion.xrel, event->motion.yrel);
	case SDL_EVENT_MOUSE_BUTTON_DOWN:
	case SDL_EVENT_MOUSE_BUTTON_UP:
		return G3D::Vector2(event->button.x, event->button.y);
	default:
		return G3D::Vector2(0, 0);
	}
}


RBX::InputObject::UserInputState UserInputUtil::msgToEventState(SDL_Event* event)
{
	switch (event->type) {
	case SDL_EVENT_MOUSE_MOTION:
		return RBX::InputObject::INPUT_STATE_CHANGE;
	case SDL_EVENT_MOUSE_BUTTON_DOWN:
		return RBX::InputObject::INPUT_STATE_BEGIN;
	case SDL_EVENT_MOUSE_BUTTON_UP:
		return RBX::InputObject::INPUT_STATE_END;
	default:
		return RBX::InputObject::INPUT_STATE_NONE;
	}
}

RBX::InputObject::UserInputType UserInputUtil::msgToEventType(SDL_Event* event)
{
	switch (event->type) {
	case SDL_EVENT_MOUSE_MOTION:
		return RBX::InputObject::TYPE_MOUSEMOVEMENT;
	case SDL_EVENT_MOUSE_BUTTON_DOWN:
	case SDL_EVENT_MOUSE_BUTTON_UP:
		switch (event->button.button) {
		case SDL_BUTTON_LEFT: return RBX::InputObject::TYPE_MOUSEBUTTON1;
		case SDL_BUTTON_RIGHT: return RBX::InputObject::TYPE_MOUSEBUTTON2;
		default: return RBX::InputObject::TYPE_NONE;
		}
	default:
		return RBX::InputObject::TYPE_NONE;
	}
}

RBX::ModCode UserInputUtil::createModCode()
{
	RBX::ModCode modCode = RBX::KC_KMOD_NONE;
	const SDL_Keymod sdlMod = SDL_GetModState();

	if (sdlMod & SDL_KMOD_LSHIFT) modCode = static_cast<RBX::ModCode>(modCode | RBX::KC_KMOD_LSHIFT);
	if (sdlMod & SDL_KMOD_RSHIFT) modCode = static_cast<RBX::ModCode>(modCode | RBX::KC_KMOD_RSHIFT);
	if (sdlMod & SDL_KMOD_LCTRL)  modCode = static_cast<RBX::ModCode>(modCode | RBX::KC_KMOD_LCTRL);
	if (sdlMod & SDL_KMOD_RCTRL)  modCode = static_cast<RBX::ModCode>(modCode | RBX::KC_KMOD_RCTRL);
	if (sdlMod & SDL_KMOD_LALT)   modCode = static_cast<RBX::ModCode>(modCode | RBX::KC_KMOD_LALT);
	if (sdlMod & SDL_KMOD_RALT)   modCode = static_cast<RBX::ModCode>(modCode | RBX::KC_KMOD_RALT);
	if (sdlMod & SDL_KMOD_LGUI)   modCode = static_cast<RBX::ModCode>(modCode | RBX::KC_KMOD_LMETA);
	if (sdlMod & SDL_KMOD_RGUI)   modCode = static_cast<RBX::ModCode>(modCode | RBX::KC_KMOD_RMETA);
	if (sdlMod & SDL_KMOD_CAPS)   modCode = static_cast<RBX::ModCode>(modCode | RBX::KC_KMOD_CAPS);
	if (sdlMod & SDL_KMOD_NUM)    modCode = static_cast<RBX::ModCode>(modCode | RBX::KC_KMOD_NUM);
	if (sdlMod & SDL_KMOD_SCROLL) modCode = static_cast<RBX::ModCode>(modCode | RBX::KC_KMOD_RESERVED);

	return modCode;
}

SDL_Scancode UserInputUtil::getSDLScancodeFromRBXKeyCode(RBX::KeyCode keyCode) {
    switch (keyCode) {
        // Letters
        case RBX::KC_A: return SDL_SCANCODE_A;
        case RBX::KC_B: return SDL_SCANCODE_B;
        case RBX::KC_C: return SDL_SCANCODE_C;
        case RBX::KC_D: return SDL_SCANCODE_D;
        case RBX::KC_E: return SDL_SCANCODE_E;
        case RBX::KC_F: return SDL_SCANCODE_F;
        case RBX::KC_G: return SDL_SCANCODE_G;
        case RBX::KC_H: return SDL_SCANCODE_H;
        case RBX::KC_I: return SDL_SCANCODE_I;
        case RBX::KC_J: return SDL_SCANCODE_J;
        case RBX::KC_K: return SDL_SCANCODE_K;
        case RBX::KC_L: return SDL_SCANCODE_L;
        case RBX::KC_M: return SDL_SCANCODE_M;
        case RBX::KC_N: return SDL_SCANCODE_N;
        case RBX::KC_O: return SDL_SCANCODE_O;
        case RBX::KC_P: return SDL_SCANCODE_P;
        case RBX::KC_Q: return SDL_SCANCODE_Q;
        case RBX::KC_R: return SDL_SCANCODE_R;
        case RBX::KC_S: return SDL_SCANCODE_S;
        case RBX::KC_T: return SDL_SCANCODE_T;
        case RBX::KC_U: return SDL_SCANCODE_U;
        case RBX::KC_V: return SDL_SCANCODE_V;
        case RBX::KC_W: return SDL_SCANCODE_W;
        case RBX::KC_X: return SDL_SCANCODE_X;
        case RBX::KC_Y: return SDL_SCANCODE_Y;
        case RBX::KC_Z: return SDL_SCANCODE_Z;

        // Numbers
        case RBX::KC_0: return SDL_SCANCODE_0;
        case RBX::KC_1: return SDL_SCANCODE_1;
        case RBX::KC_2: return SDL_SCANCODE_2;
        case RBX::KC_3: return SDL_SCANCODE_3;
        case RBX::KC_4: return SDL_SCANCODE_4;
        case RBX::KC_5: return SDL_SCANCODE_5;
        case RBX::KC_6: return SDL_SCANCODE_6;
        case RBX::KC_7: return SDL_SCANCODE_7;
        case RBX::KC_8: return SDL_SCANCODE_8;
        case RBX::KC_9: return SDL_SCANCODE_9;

        // Symbols (Mapping to physical locations)
        case RBX::KC_LEFTBRACKET:  return SDL_SCANCODE_LEFTBRACKET;
        case RBX::KC_RIGHTBRACKET: return SDL_SCANCODE_RIGHTBRACKET;
        case RBX::KC_BACKSLASH:    return SDL_SCANCODE_BACKSLASH;
        case RBX::KC_SEMICOLON:    return SDL_SCANCODE_SEMICOLON;
        case RBX::KC_QUOTE:        return SDL_SCANCODE_APOSTROPHE;
        case RBX::KC_BACKQUOTE:    return SDL_SCANCODE_GRAVE;
        case RBX::KC_COMMA:        return SDL_SCANCODE_COMMA;
        case RBX::KC_PERIOD:       return SDL_SCANCODE_PERIOD;
        case RBX::KC_SLASH:        return SDL_SCANCODE_SLASH;
        case RBX::KC_MINUS:        return SDL_SCANCODE_MINUS;
        case RBX::KC_EQUALS:       return SDL_SCANCODE_EQUALS;

        // Numpad
        case RBX::KC_KP0:          return SDL_SCANCODE_KP_0;
        case RBX::KC_KP1:          return SDL_SCANCODE_KP_1;
        case RBX::KC_KP2:          return SDL_SCANCODE_KP_2;
        case RBX::KC_KP3:          return SDL_SCANCODE_KP_3;
        case RBX::KC_KP4:          return SDL_SCANCODE_KP_4;
        case RBX::KC_KP5:          return SDL_SCANCODE_KP_5;
        case RBX::KC_KP6:          return SDL_SCANCODE_KP_6;
        case RBX::KC_KP7:          return SDL_SCANCODE_KP_7;
        case RBX::KC_KP8:          return SDL_SCANCODE_KP_8;
        case RBX::KC_KP9:          return SDL_SCANCODE_KP_9;
        case RBX::KC_KP_PERIOD:    return SDL_SCANCODE_KP_PERIOD;
        case RBX::KC_KP_DIVIDE:    return SDL_SCANCODE_KP_DIVIDE;
        case RBX::KC_KP_MULTIPLY:  return SDL_SCANCODE_KP_MULTIPLY;
        case RBX::KC_KP_MINUS:     return SDL_SCANCODE_KP_MINUS;
        case RBX::KC_KP_PLUS:      return SDL_SCANCODE_KP_PLUS;
        case RBX::KC_KP_ENTER:     return SDL_SCANCODE_KP_ENTER;
        case RBX::KC_KP_EQUALS:    return SDL_SCANCODE_KP_EQUALS;

        // Arrows & Navigation
        case RBX::KC_UP:       return SDL_SCANCODE_UP;
        case RBX::KC_DOWN:     return SDL_SCANCODE_DOWN;
        case RBX::KC_LEFT:     return SDL_SCANCODE_LEFT;
        case RBX::KC_RIGHT:    return SDL_SCANCODE_RIGHT;
        case RBX::KC_INSERT:   return SDL_SCANCODE_INSERT;
        case RBX::KC_HOME:     return SDL_SCANCODE_HOME;
        case RBX::KC_END:      return SDL_SCANCODE_END;
        case RBX::KC_PAGEUP:   return SDL_SCANCODE_PAGEUP;
        case RBX::KC_PAGEDOWN: return SDL_SCANCODE_PAGEDOWN;
        case RBX::KC_DELETE:   return SDL_SCANCODE_DELETE;

        // Control Keys
        case RBX::KC_RETURN:    return SDL_SCANCODE_RETURN;
        case RBX::KC_ESCAPE:    return SDL_SCANCODE_ESCAPE;
        case RBX::KC_BACKSPACE: return SDL_SCANCODE_BACKSPACE;
        case RBX::KC_TAB:       return SDL_SCANCODE_TAB;
        case RBX::KC_SPACE:     return SDL_SCANCODE_SPACE;

        // Function Keys
        case RBX::KC_F1:  return SDL_SCANCODE_F1;
        case RBX::KC_F2:  return SDL_SCANCODE_F2;
        case RBX::KC_F3:  return SDL_SCANCODE_F3;
        case RBX::KC_F4:  return SDL_SCANCODE_F4;
        case RBX::KC_F5:  return SDL_SCANCODE_F5;
        case RBX::KC_F6:  return SDL_SCANCODE_F6;
        case RBX::KC_F7:  return SDL_SCANCODE_F7;
        case RBX::KC_F8:  return SDL_SCANCODE_F8;
        case RBX::KC_F9:  return SDL_SCANCODE_F9;
        case RBX::KC_F10: return SDL_SCANCODE_F10;
        case RBX::KC_F11: return SDL_SCANCODE_F11;
        case RBX::KC_F12: return SDL_SCANCODE_F12;

        // Modifiers
        case RBX::KC_LSHIFT:   return SDL_SCANCODE_LSHIFT;
        case RBX::KC_RSHIFT:   return SDL_SCANCODE_RSHIFT;
        case RBX::KC_LCTRL:    return SDL_SCANCODE_LCTRL;
        case RBX::KC_RCTRL:    return SDL_SCANCODE_RCTRL;
        case RBX::KC_LALT:     return SDL_SCANCODE_LALT;
        case RBX::KC_RALT:     return SDL_SCANCODE_RALT;
        case RBX::KC_LSUPER:   return SDL_SCANCODE_LGUI;
        case RBX::KC_RSUPER:   return SDL_SCANCODE_RGUI;
        case RBX::KC_NUMLOCK:  return SDL_SCANCODE_NUMLOCKCLEAR;
        case RBX::KC_CAPSLOCK: return SDL_SCANCODE_CAPSLOCK;
        case RBX::KC_SCROLLOCK:return SDL_SCANCODE_SCROLLLOCK;

        default: return SDL_SCANCODE_UNKNOWN;
    }
}

// Convert SDL_Keycode to KeyCode
RBX::KeyCode UserInputUtil::getRBXKeyCodeFromSDLScancode(SDL_Scancode scancode) {
    switch (scancode) {
        case SDL_SCANCODE_UNKNOWN:      return RBX::KC_UNKNOWN;

        // Letters (Physical positions)
        case SDL_SCANCODE_A: return RBX::KC_A;
        case SDL_SCANCODE_B: return RBX::KC_B;
        case SDL_SCANCODE_C: return RBX::KC_C;
        case SDL_SCANCODE_D: return RBX::KC_D;
        case SDL_SCANCODE_E: return RBX::KC_E;
        case SDL_SCANCODE_F: return RBX::KC_F;
        case SDL_SCANCODE_G: return RBX::KC_G;
        case SDL_SCANCODE_H: return RBX::KC_H;
        case SDL_SCANCODE_I: return RBX::KC_I;
        case SDL_SCANCODE_J: return RBX::KC_J;
        case SDL_SCANCODE_K: return RBX::KC_K;
        case SDL_SCANCODE_L: return RBX::KC_L;
        case SDL_SCANCODE_M: return RBX::KC_M;
        case SDL_SCANCODE_N: return RBX::KC_N;
        case SDL_SCANCODE_O: return RBX::KC_O;
        case SDL_SCANCODE_P: return RBX::KC_P;
        case SDL_SCANCODE_Q: return RBX::KC_Q;
        case SDL_SCANCODE_R: return RBX::KC_R;
        case SDL_SCANCODE_S: return RBX::KC_S;
        case SDL_SCANCODE_T: return RBX::KC_T;
        case SDL_SCANCODE_U: return RBX::KC_U;
        case SDL_SCANCODE_V: return RBX::KC_V;
        case SDL_SCANCODE_W: return RBX::KC_W;
        case SDL_SCANCODE_X: return RBX::KC_X;
        case SDL_SCANCODE_Y: return RBX::KC_Y;
        case SDL_SCANCODE_Z: return RBX::KC_Z;

        // Numbers (Top Row)
        case SDL_SCANCODE_1: return RBX::KC_1;
        case SDL_SCANCODE_2: return RBX::KC_2;
        case SDL_SCANCODE_3: return RBX::KC_3;
        case SDL_SCANCODE_4: return RBX::KC_4;
        case SDL_SCANCODE_5: return RBX::KC_5;
        case SDL_SCANCODE_6: return RBX::KC_6;
        case SDL_SCANCODE_7: return RBX::KC_7;
        case SDL_SCANCODE_8: return RBX::KC_8;
        case SDL_SCANCODE_9: return RBX::KC_9;
        case SDL_SCANCODE_0: return RBX::KC_0;

        // Special Characters & Symbols
        case SDL_SCANCODE_RETURN:       return RBX::KC_RETURN;
        case SDL_SCANCODE_ESCAPE:       return RBX::KC_ESCAPE;
        case SDL_SCANCODE_BACKSPACE:    return RBX::KC_BACKSPACE;
        case SDL_SCANCODE_TAB:          return RBX::KC_TAB;
        case SDL_SCANCODE_SPACE:        return RBX::KC_SPACE;
        case SDL_SCANCODE_MINUS:        return RBX::KC_MINUS;
        case SDL_SCANCODE_EQUALS:       return RBX::KC_EQUALS;
        case SDL_SCANCODE_LEFTBRACKET:  return RBX::KC_LEFTBRACKET;  // Physically 'ú' on Czech
        case SDL_SCANCODE_RIGHTBRACKET: return RBX::KC_RIGHTBRACKET; // Physically ')' on Czech
        case SDL_SCANCODE_BACKSLASH:    return RBX::KC_BACKSLASH;
        case SDL_SCANCODE_SEMICOLON:    return RBX::KC_SEMICOLON;
        case SDL_SCANCODE_APOSTROPHE:   return RBX::KC_QUOTE;
        case SDL_SCANCODE_GRAVE:        return RBX::KC_BACKQUOTE;
        case SDL_SCANCODE_COMMA:        return RBX::KC_COMMA;
        case SDL_SCANCODE_PERIOD:       return RBX::KC_PERIOD;
        case SDL_SCANCODE_SLASH:        return RBX::KC_SLASH;

        // Numeric Keypad
        case SDL_SCANCODE_KP_1:        return RBX::KC_KP1;
        case SDL_SCANCODE_KP_2:        return RBX::KC_KP2;
        case SDL_SCANCODE_KP_3:        return RBX::KC_KP3;
        case SDL_SCANCODE_KP_4:        return RBX::KC_KP4;
        case SDL_SCANCODE_KP_5:        return RBX::KC_KP5;
        case SDL_SCANCODE_KP_6:        return RBX::KC_KP6;
        case SDL_SCANCODE_KP_7:        return RBX::KC_KP7;
        case SDL_SCANCODE_KP_8:        return RBX::KC_KP8;
        case SDL_SCANCODE_KP_9:        return RBX::KC_KP9;
        case SDL_SCANCODE_KP_0:        return RBX::KC_KP0;
        case SDL_SCANCODE_KP_PERIOD:   return RBX::KC_KP_PERIOD;
        case SDL_SCANCODE_KP_DIVIDE:   return RBX::KC_KP_DIVIDE;
        case SDL_SCANCODE_KP_MULTIPLY: return RBX::KC_KP_MULTIPLY;
        case SDL_SCANCODE_KP_MINUS:    return RBX::KC_KP_MINUS;
        case SDL_SCANCODE_KP_PLUS:     return RBX::KC_KP_PLUS;
        case SDL_SCANCODE_KP_ENTER:    return RBX::KC_KP_ENTER;
        case SDL_SCANCODE_KP_EQUALS:   return RBX::KC_KP_EQUALS;

        // Navigation & Control
        case SDL_SCANCODE_UP:       return RBX::KC_UP;
        case SDL_SCANCODE_DOWN:     return RBX::KC_DOWN;
        case SDL_SCANCODE_LEFT:     return RBX::KC_LEFT;
        case SDL_SCANCODE_RIGHT:    return RBX::KC_RIGHT;
        case SDL_SCANCODE_INSERT:   return RBX::KC_INSERT;
        case SDL_SCANCODE_HOME:     return RBX::KC_HOME;
        case SDL_SCANCODE_END:      return RBX::KC_END;
        case SDL_SCANCODE_PAGEUP:   return RBX::KC_PAGEUP;
        case SDL_SCANCODE_PAGEDOWN: return RBX::KC_PAGEDOWN;
        case SDL_SCANCODE_DELETE:   return RBX::KC_DELETE;

        // Function Keys
        case SDL_SCANCODE_F1:  return RBX::KC_F1;
        case SDL_SCANCODE_F2:  return RBX::KC_F2;
        case SDL_SCANCODE_F3:  return RBX::KC_F3;
        case SDL_SCANCODE_F4:  return RBX::KC_F4;
        case SDL_SCANCODE_F5:  return RBX::KC_F5;
        case SDL_SCANCODE_F6:  return RBX::KC_F6;
        case SDL_SCANCODE_F7:  return RBX::KC_F7;
        case SDL_SCANCODE_F8:  return RBX::KC_F8;
        case SDL_SCANCODE_F9:  return RBX::KC_F9;
        case SDL_SCANCODE_F10: return RBX::KC_F10;
        case SDL_SCANCODE_F11: return RBX::KC_F11;
        case SDL_SCANCODE_F12: return RBX::KC_F12;
        case SDL_SCANCODE_F13: return RBX::KC_F13;
        case SDL_SCANCODE_F14: return RBX::KC_F14;
        case SDL_SCANCODE_F15: return RBX::KC_F15;

        // Modifiers
        case SDL_SCANCODE_NUMLOCKCLEAR: return RBX::KC_NUMLOCK;
        case SDL_SCANCODE_CAPSLOCK:     return RBX::KC_CAPSLOCK;
        case SDL_SCANCODE_SCROLLLOCK:   return RBX::KC_SCROLLOCK;
        case SDL_SCANCODE_RSHIFT:       return RBX::KC_RSHIFT;
        case SDL_SCANCODE_LSHIFT:       return RBX::KC_LSHIFT;
        case SDL_SCANCODE_RCTRL:        return RBX::KC_RCTRL;
        case SDL_SCANCODE_LCTRL:        return RBX::KC_LCTRL;
        case SDL_SCANCODE_RALT:         return RBX::KC_RALT;
        case SDL_SCANCODE_LALT:         return RBX::KC_LALT;
        case SDL_SCANCODE_RGUI:         return RBX::KC_RSUPER;
        case SDL_SCANCODE_LGUI:         return RBX::KC_LSUPER;
        case SDL_SCANCODE_MODE:         return RBX::KC_MODE;
        case SDL_SCANCODE_APPLICATION:  return RBX::KC_MENU;

        // Misc
        case SDL_SCANCODE_HELP:        return RBX::KC_HELP;
        case SDL_SCANCODE_PRINTSCREEN: return RBX::KC_PRINT;
        case SDL_SCANCODE_SYSREQ:      return RBX::KC_SYSREQ;
        case SDL_SCANCODE_PAUSE:       return RBX::KC_PAUSE;
        case SDL_SCANCODE_POWER:       return RBX::KC_POWER;
        case SDL_SCANCODE_UNDO:        return RBX::KC_UNDO;

        default: return RBX::KC_UNKNOWN;
    }
}


// Convert ModCode to SDL_Keymod
SDL_Keymod UserInputUtil::getSDLModCodeFromRBXModCode(RBX::ModCode modCode) {
	switch (modCode) {
	case RBX::KC_KMOD_NONE: return SDL_KMOD_NONE;
	case RBX::KC_KMOD_LSHIFT: return SDL_KMOD_LSHIFT;
	case RBX::KC_KMOD_RSHIFT: return SDL_KMOD_RSHIFT;
	case RBX::KC_KMOD_LCTRL: return SDL_KMOD_LCTRL;
	case RBX::KC_KMOD_RCTRL: return SDL_KMOD_RCTRL;
	case RBX::KC_KMOD_LALT: return SDL_KMOD_LALT;
	case RBX::KC_KMOD_RALT: return SDL_KMOD_RALT;
	case RBX::KC_KMOD_LMETA: return SDL_KMOD_LGUI;
	case RBX::KC_KMOD_RMETA: return SDL_KMOD_RGUI;
	case RBX::KC_KMOD_NUM: return SDL_KMOD_NUM;
	case RBX::KC_KMOD_CAPS: return SDL_KMOD_CAPS;
	case RBX::KC_KMOD_MODE: return SDL_KMOD_MODE;
	case RBX::KC_KMOD_RESERVED: return SDL_KMOD_SCROLL;
	default: return SDL_KMOD_NONE; // Handle unknown mod codes
	}
}

// Convert SDL_Keymod to ModCode
RBX::ModCode UserInputUtil::getRBXModCodeFromSDLMod(SDL_Keymod sdlMod) {
	RBX::ModCode modCode = RBX::KC_KMOD_NONE;

	if (sdlMod & SDL_KMOD_LSHIFT) modCode = static_cast<RBX::ModCode>(modCode | RBX::KC_KMOD_LSHIFT);
	if (sdlMod & SDL_KMOD_RSHIFT) modCode = static_cast<RBX::ModCode>(modCode | RBX::KC_KMOD_RSHIFT);
	if (sdlMod & SDL_KMOD_LCTRL)  modCode = static_cast<RBX::ModCode>(modCode | RBX::KC_KMOD_LCTRL);
	if (sdlMod & SDL_KMOD_RCTRL)  modCode = static_cast<RBX::ModCode>(modCode | RBX::KC_KMOD_RCTRL);
	if (sdlMod & SDL_KMOD_LALT)   modCode = static_cast<RBX::ModCode>(modCode | RBX::KC_KMOD_LALT);
	if (sdlMod & SDL_KMOD_RALT)   modCode = static_cast<RBX::ModCode>(modCode | RBX::KC_KMOD_RALT);
	if (sdlMod & SDL_KMOD_LGUI)   modCode = static_cast<RBX::ModCode>(modCode | RBX::KC_KMOD_LMETA);
	if (sdlMod & SDL_KMOD_RGUI)   modCode = static_cast<RBX::ModCode>(modCode | RBX::KC_KMOD_RMETA);
	if (sdlMod & SDL_KMOD_CAPS)   modCode = static_cast<RBX::ModCode>(modCode | RBX::KC_KMOD_CAPS);
	if (sdlMod & SDL_KMOD_NUM)    modCode = static_cast<RBX::ModCode>(modCode | RBX::KC_KMOD_NUM);
	if (sdlMod & SDL_KMOD_SCROLL) modCode = static_cast<RBX::ModCode>(modCode | RBX::KC_KMOD_RESERVED);

	return modCode;
}
