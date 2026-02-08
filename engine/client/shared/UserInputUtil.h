#pragma once

#include "SDL3/SDL.h"
#include "util/KeyCode.h"

#include "v8datamodel/InputObject.h"
#include "g3d/Vector2.h"

using namespace RBX;

class UserInputUtil
{
private:
	static void wrapMouseBorder(const G3D::Vector2& delta,
		G3D::Vector2& wrapMouseDelta,
		G3D::Vector2& wrapMousePosition,
		const G3D::Vector2& windowSize,
		const int borderWidth,
		const float creepFactor);

public:
	typedef unsigned char DiKeys[256];

	static const float HybridSensitivity;
	static const float MouseTug;

	static RBX::ModCode				createModCode();
	static RBX::InputObject::UserInputState msgToEventState(SDL_Event* event);
	static RBX::InputObject::UserInputType	msgToEventType(SDL_Event* event);

	// KeyCode conversion functions
	static SDL_Scancode getSDLScancodeFromRBXKeyCode(RBX::KeyCode keyCode);
	static RBX::KeyCode getRBXKeyCodeFromSDLScancode(SDL_Scancode scanCode);

	// ModCode conversion functions
	static SDL_Keymod getSDLModCodeFromRBXModCode(RBX::ModCode modCode);
	static RBX::ModCode getRBXModCodeFromSDLMod(SDL_Keymod sdlMod);

	static G3D::Vector2	sdlEventToVector2(SDL_Event* event);
	static bool	isCtrlDown(RBX::ModCode modCode);

	static void	wrapMouseNone(const G3D::Vector2& delta,
		G3D::Vector2& wrapMouseDelta,
		G3D::Vector2& wrapMousePosition);

	static void	wrapMouseBorderLock(const G3D::Vector2& delta,
		G3D::Vector2& wrapMouseDelta,
		G3D::Vector2& wrapMousePosition,
		const G3D::Vector2& windowSize);
	static void	wrapMouseCenter(const G3D::Vector2& delta,
		G3D::Vector2& wrapMouseDelta,
		G3D::Vector2& wrapMousePosition);
};
