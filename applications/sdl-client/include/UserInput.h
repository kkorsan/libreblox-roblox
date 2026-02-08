#pragma once


#include "SDL3/SDL.h"

#include "stdafx.h"
#include "util/UserInputBase.h"
#include "util/standardout.h"
#include "SDLGameController.h"
#include "util/ContentId.h"
#include <unordered_set>

namespace RBX {

class Game;
class DataModel;
class RunService;
class View;

class UserInput : public UserInputBase
{
	enum MouseButtonType
    {
	    MBUTTON_LEFT	= 0,
	    MBUTTON_RIGHT	= 1,
		MBUTTON_MIDDLE  = 2
    };

    SDL_Cursor* currentSdlCursor = nullptr;
    RBX::ContentId lastCursorId;
    void updateCursorImage();

	rbx::signals::scoped_connection steppedConnection;
	rbx::signals::scoped_connection textBoxGainFocusConnection;
	rbx::signals::scoped_connection textBoxReleaseFocusConnection;
	bool sdlTextInputActive = false;

	// Mouse Stuff
	bool isMouseCaptured;			// poor man's tracker of button state
	Vector2 wrapMousePosition;		// in normalized coordinates (center is 0,0. radius is getWrapRadius)
	bool wrapping;
	bool rightMouseDown;
	bool autoMouseMove;
	bool mouseButtonSwap;			// used for left hand mouse

	bool prevLeftButtonDown = false;
	bool prevRightButtonDown = false;
	bool prevMiddleButtonDown = false;

	// Keyboard Stuff
	int externallyForcedKeyDown;		// + this from external source (like a gui button)
	std::unordered_set<RBX::KeyCode> downKeys;

	SDL_Window* window;

	// InputObject stuff
	boost::unordered_map<RBX::InputObject::UserInputType,shared_ptr<RBX::InputObject> > inputObjectMap;

	static void ProcessSteppedEventsWrapper(UserInput* instance);

	// Gamepad stuff
	shared_ptr<SDLGameController> sdlGameController;

	shared_ptr<RunService> runService;

	View* parentView;

	G3D::Vector2 posToWrapTo;

	G3D::Vector2 previousCursorPosFraction;

	G3D::Vector2 previousMousePos;
	G3D::Vector2 accumulatedMouseDelta;
	G3D::Vector2 lastCursorPos;
	boost::mutex inputMutex;

	bool leftMouseButtonDown;

	////////////////////////////////
	//
	// Events
	void processSteppedEvents();	// called every frame from render thread

	void processMouseEvents(SDL_Event* event);

	void handleMouseButtonEvent(const SDL_MouseButtonEvent* event);
	void handleMouseMotionEvent(const SDL_MouseMotionEvent* event);

	void sendEvent(shared_ptr<InputObject> event);
	void sendMouseEvent(InputObject::UserInputType mouseEventType,
						InputObject::UserInputState mouseEventState,
						const Vector3& position,
						const Vector3& delta);
	void handleMouseButtonStateChange(InputObject::UserInputType type, bool isDown);

	void handleMouseButtons(SDL_MouseButtonFlags mouseState);

	void handleMouseMotion(bool& cursorMoved, G3D::Vector2& wrapMouseDelta, G3D::Vector2& mouseDelta);

	void handleMouseWheel(const SDL_MouseWheelEvent* wheelEvent);

	void handleKeyboardEvent(const SDL_KeyboardEvent* keyEvent);

	// SDL text input events (UTF-8)
	void handleTextInputEvent(const SDL_TextInputEvent* textEvent);
	void handleTextEditingEvent(const SDL_TextEditingEvent* editingEvent);

	// Handlers for TextBox focus events (start/stop SDL text input)
	void onTextBoxFocusGained(shared_ptr<RBX::Instance> textBox);
	void onTextBoxFocusReleased(shared_ptr<RBX::Instance> textBox);

	// Helpers to start/stop SDL text input explicitly
	void startSDLTextInput();
	void stopSDLTextInput();

	void handleWindowEvent(const SDL_WindowEvent* windowEvent);

	void clearAllKeys();

	////////////////////////////////////
	//
	// Keyboard Mouse
	bool isMouseAcquired;			// goes false if data read fails

	void acquireMouseInternal();

	void unAcquireMouse();

	// window stuff
	Vector2int16 getWindowSize() const;
	G3D::Rect2D getWindowRect() const;
	bool isFullScreenMode() const;
	bool movementKeysDown();
	bool keyDownInternal(RBX::KeyCode code) const;

	void doWrapMouse(const G3D::Vector2& delta, G3D::Vector2& wrapMouseDelta);

	G3D::Vector2 getGameCursorPositionInternal();
	G3D::Vector2 getGameCursorPositionExpandedInternal();	// prevent hysteresis
	G3D::Vector2 getSDLCursorPositionInternal();
	Vector2 getCursorPositionInternal();

	void doDiagnostics();

	void postProcessUserInput(bool cursorMoved, RBX::Vector2 wrapMouseDelta, RBX::Vector2 mouseDelta);

	void doWrapHybrid(bool cursorMoved, bool leftMouseUp, G3D::Vector2& wrapMouseDelta, G3D::Vector2& wrapMousePosition, G3D::Vector2& posToWrapTo);

public:
	shared_ptr<RBX::Game> game;
	////////////////////////////////
	//
	// UserInputBase

	/*implement*/ Vector2 getCursorPosition();

	/*implement*/ bool keyDown(RBX::KeyCode code) const;
	/*implement*/ void setKeyState(RBX::KeyCode code, RBX::ModCode modCode, char modifiedKey, bool isDown);

	/*implement*/ void centerCursor() { wrapMousePosition = Vector2::zero(); }
	/*override*/ TextureProxyBaseRef getGameCursor(Adorn* adorn);
	/*override*/ bool isHardwareCursorActive() const { return !isMouseAcquired; }

	void postUserInputMessage(SDL_Event* event);
	// Call this only within a DataModel lock:
	void processUserInputMessage(SDL_Event* event);

	void onWindowFocusLost();

	UserInput(SDL_Window* window, shared_ptr<RBX::Game> game, View* view);
	~UserInput();

	void setGame(shared_ptr<RBX::Game> game);
	void onMouseInside();
	void onMouseLeave();

	bool getIsMouseAcquired() const { return isMouseAcquired; }
	void unAcquireMousePublic() { unAcquireMouse(); }
};

}  // namespace RBX
