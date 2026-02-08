#include "stdafx.h"
#include "UserInput.h"
#include "UserInputUtil.h"

#include "g3d/GImage.h"
#include "util/G3DCore.h"
#include "util/Rect.h"
#include "util/standardout.h"
#include "util/NavKeys.h"
#include "v8datamodel/Game.h"
#include "v8datamodel/DataModel.h"
#include "v8datamodel/DataModelJob.h"
#include "v8datamodel/GameBasicSettings.h"
#include "v8datamodel/ContentProvider.h"
#include "v8datamodel/Workspace.h"
#include "v8datamodel/UserInputService.h"
#include "View.h"
#include "humanoid/Humanoid.h"
#include "RbxG3D/RbxTime.h"
#include "g3d/System.h"
#include "g3d/Vector2.h"
#include "RenderSettingsItem.h"
#include "FastLog.h"

#include "SDL3/SDL.h"
#ifdef nullptr
#undef nullptr
#endif
#include "util/KeyCode.h"
#include <SDL3/SDL_error.h>
#ifdef nullptr
#undef nullptr
#endif
#include <SDL3/SDL_hints.h>
#ifdef nullptr
#undef nullptr
#endif
#include <SDL3/SDL_mouse.h>
#ifdef nullptr
#undef nullptr
#endif


LOGGROUP(UserInputProfile)
DYNAMIC_FASTFLAGVARIABLE(MouseDeltaWhenNotMouseLocked, false)
DYNAMIC_FASTFLAGVARIABLE(UserInputViewportSizeFixWindows, true)

FASTFLAG(UserAllCamerasInLua)

namespace RBX {

UserInput::UserInput(SDL_Window* window, shared_ptr<RBX::Game> game, View* view)
	: externallyForcedKeyDown(0)
	, isMouseCaptured(false)
	, isMouseAcquired(false)
	, window(window)
	, wrapping(false)
	, posToWrapTo(0, 0)
	, previousMousePos(G3D::Vector2::zero())
	, leftMouseButtonDown(false)
	, rightMouseDown(false)
	, autoMouseMove(true)
	, mouseButtonSwap(false)
	, parentView(view)
	, accumulatedMouseDelta(0, 0)
{
	setGame(game);

	// Initialize cached mouse position
	float x, y;
	SDL_GetMouseState(&x, &y);
	lastCursorPos = G3D::Vector2(x, y);

	// SDL3 automatically detects game controllers
	SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
}

void UserInput::ProcessSteppedEventsWrapper(UserInput* instance) {
	instance->processSteppedEvents();
}

void UserInput::setGame(shared_ptr<RBX::Game> game)
{
	this->game = game;

	runService = shared_from(ServiceProvider::create<RunService>(game->getDataModel().get()));

	shared_ptr<DataModel> dataModel = game->getDataModel();

	sdlGameController = shared_ptr<SDLGameController>(new SDLGameController(dataModel));

	// Listen for TextBox focus events so we can enable SDL text input (UTF-8 / IME) when a text field is focused.
	if (dataModel)
	{
		if (RBX::UserInputService* inputService = RBX::ServiceProvider::find<RBX::UserInputService>(dataModel.get()))
		{
            // Set clipboard callbacks
            inputService->setClipboardCallbacks(
                []() -> std::string {
                    if (SDL_HasClipboardText()) {
                        char* text = SDL_GetClipboardText();
                        std::string result = text ? text : "";
                        if (text) SDL_free(text);
                        return result;
                    }
                    return "";
                },
                [](const std::string& text) {
                    SDL_SetClipboardText(text.c_str());
                }
            );

			textBoxGainFocusConnection = inputService->textBoxGainFocus.connect(boost::bind(&UserInput::onTextBoxFocusGained, this, _1));
			textBoxReleaseFocusConnection = inputService->textBoxReleaseFocus.connect(boost::bind(&UserInput::onTextBoxFocusReleased, this, _1));
		}
	}

	steppedConnection = runService->earlyRenderSignal.connect(
		boost::bind(&UserInput::ProcessSteppedEventsWrapper, this)
	);
}

UserInput::~UserInput()
{
	sdlGameController.reset();
    if (currentSdlCursor) {
        SDL_DestroyCursor(currentSdlCursor);
        currentSdlCursor = nullptr;
    }
}

void UserInput::acquireMouseInternal()
{
	if (isMouseAcquired)
		return;
	isMouseAcquired = true;
	SDL_SetWindowRelativeMouseMode(window, true);
	SDL_HideCursor();
}

void UserInput::unAcquireMouse()
{
	if (!isMouseAcquired)
		return;
	isMouseAcquired = false;
	SDL_SetWindowRelativeMouseMode(window, false);
	SDL_ShowCursor();
}

void UserInput::postUserInputMessage(SDL_Event* event) {
	processUserInputMessage(event);
}

void UserInput::processUserInputMessage(SDL_Event* event) {
	switch (event->type) {
	case SDL_EVENT_MOUSE_WHEEL:
		processMouseEvents(event);
		break;
	case SDL_EVENT_MOUSE_BUTTON_DOWN:
	case SDL_EVENT_MOUSE_BUTTON_UP:
		handleMouseButtonEvent(&event->button);
		break;
	case SDL_EVENT_MOUSE_MOTION:
		handleMouseMotionEvent(&event->motion);
		break;
	case SDL_EVENT_KEY_DOWN:
	case SDL_EVENT_KEY_UP:
		handleKeyboardEvent(&event->key);
		break;
	case SDL_EVENT_TEXT_INPUT:
		handleTextInputEvent(&event->text);
		break;
	case SDL_EVENT_TEXT_EDITING:
		handleTextEditingEvent(&event->edit);
		break;
	case SDL_EVENT_WINDOW_FOCUS_LOST:
	case SDL_EVENT_WINDOW_MINIMIZED:
		clearAllKeys();
		break;
	case SDL_EVENT_WINDOW_MOUSE_ENTER:
		break;
	case SDL_EVENT_WINDOW_MOUSE_LEAVE:
		break;
	case SDL_EVENT_GAMEPAD_ADDED:
	case SDL_EVENT_GAMEPAD_REMOVED:
	case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
	case SDL_EVENT_GAMEPAD_BUTTON_UP:
	case SDL_EVENT_GAMEPAD_AXIS_MOTION:
		if (sdlGameController) sdlGameController->handleEvent(*event);
		break;
	default:
        if (event->type >= SDL_EVENT_WINDOW_FIRST && event->type <= SDL_EVENT_WINDOW_LAST)
    	{
    		handleWindowEvent(&event->window);
    	}
    	break;
	}
}

void UserInput::handleWindowEvent(const SDL_WindowEvent* windowEvent)
{
	if (windowEvent->type == SDL_EVENT_WINDOW_MOUSE_ENTER ||
		windowEvent->type == SDL_EVENT_WINDOW_FOCUS_GAINED)
	{
		float x, y;
		SDL_GetMouseState(&x, &y);
		{
			boost::mutex::scoped_lock lock(inputMutex);
			lastCursorPos = G3D::Vector2(x, y);
		}
	}
}

void UserInput::updateCursorImage()
{
    if (!game || !game->getDataModel()) return;

    // Use DataModel's getRenderMouseCursor to get the correct current cursor ID (including tool/hover icons)
    RBX::ContentId cursorId = game->getDataModel()->getRenderMouseCursor();

    // Default to Roblox arrow if no icon specified (System cursor replacement)
    if (cursorId.isNull()) {
        cursorId = RBX::ContentId::fromAssets("textures/ArrowCursor.png");
    }

    if (cursorId == lastCursorId) return;
    lastCursorId = cursorId;

    // Load cursor
    try {
        RBX::ContentProvider* cp = RBX::ServiceProvider::create<RBX::ContentProvider>(game->getDataModel().get());
        if (!cp) return;

        shared_ptr<const std::string> content;
        shared_ptr<const std::string> content2x;
        bool has2x = false;

        // Try @2x
        std::string url = cursorId.toString();
        size_t extPos = url.find_last_of('.');
        if (extPos != std::string::npos) {
            std::string url2x = url.substr(0, extPos) + "@2x" + url.substr(extPos);
            try {
                content2x = cp->requestContentString(RBX::ContentId(url2x), RBX::ContentProvider::PRIORITY_MFC);
                if (content2x && !content2x->empty()) has2x = true;
            } catch (...) {}
        }

        content = cp->requestContentString(cursorId, RBX::ContentProvider::PRIORITY_MFC);

        if (content && !content->empty()) {
            G3D::GImage imgStandard((const unsigned char*)content->c_str(), content->size(), G3D::GImage::AUTODETECT);
            if (imgStandard.width() > 0 && imgStandard.height() > 0) {
                if (imgStandard.channels() != 4) imgStandard.convertToRGBA();

                // Heuristic: If 64x64 or larger, assume it's meant to be 32x32 logical.
                int logicalW = imgStandard.width();
                int logicalH = imgStandard.height();

                G3D::GImage imgBase = imgStandard;
                if (imgBase.channels() != 4) imgBase.convertToRGBA();

                // Premultiply alpha to fix white outline/fringe on antialiased edges
                {
                    uint8_t* p = (uint8_t*)imgBase.byte();
                    for (int i = 0; i < logicalW * logicalH * 4; i += 4) {
                        uint32_t a = p[i + 3];
                        p[i]     = (uint8_t)((uint32_t)p[i]     * a / 255);
                        p[i + 1] = (uint8_t)((uint32_t)p[i + 1] * a / 255);
                        p[i + 2] = (uint8_t)((uint32_t)p[i + 2] * a / 255);
                    }
                }

                SDL_Surface* surfBaseRaw = SDL_CreateSurfaceFrom(logicalW, logicalH, SDL_PIXELFORMAT_RGBA32, (void*)imgBase.byte(), logicalW * 4);
                SDL_Surface* surfBase = SDL_ConvertSurface(surfBaseRaw, SDL_PIXELFORMAT_ARGB8888);
                SDL_DestroySurface(surfBaseRaw);

                std::vector<SDL_Surface*> alternates;

                if (has2x && surfBase) {
                    G3D::GImage img2xTemp = G3D::GImage((const unsigned char*)content2x->c_str(), content2x->size(), G3D::GImage::AUTODETECT);
                    if (img2xTemp.width() > 0) {
                        if (img2xTemp.channels() != 4) img2xTemp.convertToRGBA();

                        // Premultiply 2x version
                        {
                            uint8_t* p = (uint8_t*)img2xTemp.byte();
                            for (int i = 0; i < img2xTemp.width() * img2xTemp.height() * 4; i += 4) {
                                uint32_t a = p[i + 3];
                                p[i]     = (uint8_t)((uint32_t)p[i]     * a / 255);
                                p[i + 1] = (uint8_t)((uint32_t)p[i + 1] * a / 255);
                                p[i + 2] = (uint8_t)((uint32_t)p[i + 2] * a / 255);
                            }
                        }

                        SDL_Surface* surfHighRaw = SDL_CreateSurfaceFrom(img2xTemp.width(), img2xTemp.height(), SDL_PIXELFORMAT_RGBA32, (void*)img2xTemp.byte(), img2xTemp.width() * 4);
                        SDL_Surface* surfHigh = SDL_ConvertSurface(surfHighRaw, SDL_PIXELFORMAT_ARGB8888);
                        SDL_DestroySurface(surfHighRaw);
                        if (surfHigh) {
                            SDL_AddSurfaceAlternateImage(surfBase, surfHigh);
                            alternates.push_back(surfHigh);
                        }
                    }
                }

                if (surfBase) {
                    int hx = logicalW / 2;
                    int hy = logicalH / 2;

                    if (currentSdlCursor) SDL_DestroyCursor(currentSdlCursor);
                    currentSdlCursor = SDL_CreateColorCursor(surfBase, hx, hy);
                    if (currentSdlCursor) {
                        SDL_SetCursor(currentSdlCursor);
                        if (!isMouseAcquired) SDL_ShowCursor();
                    }

                    // Release our references (SDL_CreateColorCursor consumed data/refs, or manages them)
                    // Alternates are referenced by surfBase.
                    // Destroying surfBase destroys its alternates list refs.
                    // We must release OUR refs to alternates.
                    for (SDL_Surface* s : alternates) SDL_DestroySurface(s);
                    SDL_DestroySurface(surfBase);
                }
            }
        }
    } catch (...) {
        // Fallback to default
        if (currentSdlCursor) {
            SDL_DestroyCursor(currentSdlCursor);
            currentSdlCursor = nullptr;
        }
        SDL_SetCursor(SDL_GetDefaultCursor());
        if (!isMouseAcquired) SDL_ShowCursor();
    }
}

void UserInput::processSteppedEvents() {

    updateCursorImage();

	G3D::Vector2 mouseDelta;
	{
		boost::mutex::scoped_lock lock(inputMutex);
		mouseDelta = accumulatedMouseDelta;
		accumulatedMouseDelta = G3D::Vector2::zero(); // Reset for next frame
	}

	bool cursorMoved = (mouseDelta != G3D::Vector2::zero());
	G3D::Vector2 wrapMouseDelta;

	// Always process movement, even if zero, to satisfy UserInputService expectations
	handleMouseMotion(cursorMoved, wrapMouseDelta, mouseDelta);
	postProcessUserInput(cursorMoved, wrapMouseDelta, mouseDelta);
}


/////////////////////////////////////////////////////////////////////
// Event Handling
//

void UserInput::processMouseEvents(SDL_Event* event)
{
	switch (event->type) {
	case SDL_EVENT_MOUSE_WHEEL:
		handleMouseWheel(&event->wheel);
		break;
	}
}

void UserInput::handleMouseButtonEvent(const SDL_MouseButtonEvent* event)
{
	{
		boost::mutex::scoped_lock lock(inputMutex);
		lastCursorPos = G3D::Vector2(event->x, event->y);
	}

	bool isDown = (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN);
	InputObject::UserInputType type;

	switch (event->button) {
	case SDL_BUTTON_LEFT:
		type = InputObject::TYPE_MOUSEBUTTON1;
		prevLeftButtonDown = isDown;
		leftMouseButtonDown = isDown;
		break;
	case SDL_BUTTON_RIGHT:
		type = InputObject::TYPE_MOUSEBUTTON2;
		prevRightButtonDown = isDown;
		rightMouseDown = isDown;
		break;
	case SDL_BUTTON_MIDDLE:
		type = InputObject::TYPE_MOUSEBUTTON3;
		prevMiddleButtonDown = isDown;
		break;
	default:
		return;
	}

	InputObject::UserInputState state = isDown ?
		InputObject::INPUT_STATE_BEGIN :
		InputObject::INPUT_STATE_END;

	G3D::Vector2 cursorPos = getGameCursorPositionInternal();

	sendMouseEvent(type, state, G3D::Vector3(cursorPos, 0), G3D::Vector3(0, 0, 0));
}

void UserInput::handleMouseMotionEvent(const SDL_MouseMotionEvent* event)
{
	boost::mutex::scoped_lock lock(inputMutex);
	accumulatedMouseDelta += G3D::Vector2(event->xrel, event->yrel);
	lastCursorPos = G3D::Vector2(event->x, event->y);
}

void UserInput::handleMouseButtons(SDL_MouseButtonFlags mouseState) {
	bool currentLeft = (mouseState & SDL_BUTTON_MASK(SDL_BUTTON_LEFT)) != 0;
	bool currentRight = (mouseState & SDL_BUTTON_MASK(SDL_BUTTON_RIGHT)) != 0;
	bool currentMiddle = (mouseState & SDL_BUTTON_MASK(SDL_BUTTON_MIDDLE)) != 0;

	if (currentLeft != prevLeftButtonDown) {
		handleMouseButtonStateChange(InputObject::TYPE_MOUSEBUTTON1, currentLeft);
		prevLeftButtonDown = currentLeft;
		leftMouseButtonDown = currentLeft;
	}
	if (currentRight != prevRightButtonDown) {
		handleMouseButtonStateChange(InputObject::TYPE_MOUSEBUTTON2, currentRight);
		prevRightButtonDown = currentRight;
		rightMouseDown = currentRight;
	}
	if (currentMiddle != prevMiddleButtonDown) {
		handleMouseButtonStateChange(InputObject::TYPE_MOUSEBUTTON3, currentMiddle);
		prevMiddleButtonDown = currentMiddle;
	}
}

void UserInput::handleMouseButtonStateChange(InputObject::UserInputType type, bool isDown)
{
	InputObject::UserInputState state = isDown ?
		InputObject::INPUT_STATE_BEGIN :
		InputObject::INPUT_STATE_END;

	G3D::Vector2 cursorPos = getCursorPositionInternal();

	sendMouseEvent(type, state, G3D::Vector3(cursorPos, 0), G3D::Vector3(0, 0, 0));
}

void UserInput::handleMouseMotion(bool &cursorMoved, Vector2 &wrapMouseDelta, Vector2 &mouseDelta)
{
	// add the mouse delta to the previous cursor position fraction
	previousCursorPosFraction += mouseDelta;

	// convert to integer
	mouseDelta.x = (int)previousCursorPosFraction.x;
	mouseDelta.y = (int)previousCursorPosFraction.y;

	// store the remainder of the delta
	previousCursorPosFraction -= mouseDelta;

	doWrapMouse(mouseDelta, wrapMouseDelta);

	cursorMoved = (mouseDelta.x != 0 || mouseDelta.y != 0);

}

void UserInput::handleMouseWheel(const SDL_MouseWheelEvent* wheelEvent)
{
	// Get scroll deltas from the event structure
	const float deltaY = wheelEvent->y;  // Vertical scroll

	{
		boost::mutex::scoped_lock lock(inputMutex);
		lastCursorPos = G3D::Vector2(wheelEvent->mouse_x, wheelEvent->mouse_y);
	}

	// Get cursor position from the event
	const Vector2 cursorPos = getCursorPositionInternal();

	// Create local Vector3 objects
	RBX::Vector3 position(cursorPos, 0.0f);  // Cursor position
	RBX::Vector3 delta(0.0f, 0.0f, 0.0f);   // Zero delta

	// Handle vertical scroll
	if (deltaY != 0.0f) {
		position.z = deltaY;  // Set scroll delta in Z-axis
		if (DFFlag::MouseDeltaWhenNotMouseLocked) {
			sendMouseEvent(InputObject::TYPE_MOUSEWHEEL,
				InputObject::INPUT_STATE_CHANGE,
				position,
				delta);
		}
		else {
			sendMouseEvent(InputObject::TYPE_MOUSEWHEEL,
				InputObject::INPUT_STATE_CHANGE,
				position,
				delta);
		}
	}
}

void UserInput::handleKeyboardEvent(const SDL_KeyboardEvent* keyEvent)
{
	const bool down = (keyEvent->type == SDL_EVENT_KEY_DOWN);
	const SDL_Scancode sdlScanCode = keyEvent->scancode;

	// Convert SDL key codes to engine's KeyCode and ModCode
	RBX::KeyCode keyCode = UserInputUtil::getRBXKeyCodeFromSDLScancode(sdlScanCode);

	// Force physical Backquote key to always map to RBX::KC_BACKQUOTE for non-US layouts, for backpack consistency
	if (keyEvent->scancode == SDL_SCANCODE_GRAVE) {
		keyCode = RBX::KC_BACKQUOTE;
	}

	RBX::ModCode modCode = UserInputUtil::getRBXModCodeFromSDLMod(keyEvent->mod);

    FASTLOG3(FLog::UserInputProfile, "Key Event: key=%d, down=%d, mod=%x", keyCode, down, modCode);

	// Handle special key combinations
	if (down) {
        FASTLOG2(FLog::UserInputProfile, "Key Down Check: F4=%d, Shift=%d", keyCode == RBX::KC_F4, (modCode & (RBX::KC_KMOD_LSHIFT | RBX::KC_KMOD_RSHIFT)));
		// Handle Alt+F4
		if (keyCode == RBX::KC_F4 && (modCode & (RBX::KC_KMOD_LALT | RBX::KC_KMOD_RALT))) {
			SDL_Event quitEvent{};
			quitEvent.type = SDL_EVENT_QUIT;
			SDL_PushEvent(&quitEvent);
			return;
		}
	}

	// Get text input (if any)
	// Prefer SDL's text input (UTF-8 / IME) when available for proper unicode handling.
	char finalChar = 0;
	// If the engine is using the new keyboard events (SDL / platform text input), or if SDL text input is active,
	// don't synthesize an ASCII char from key/modifiers (that only works reliably for US layouts).
	if (!sdlTextInputActive) {
		finalChar = UserInputService::getModifiedKey(keyCode, modCode);
	}
	if (down) {
		downKeys.insert(keyCode);
	} else {
		downKeys.erase(keyCode);
	}
	// Update input service
	if (RBX::UserInputService* userInputService =
		RBX::ServiceProvider::find<RBX::UserInputService>(game->getDataModel().get()))
	{
		userInputService->setKeyState(keyCode, modCode, finalChar, down);
	}

	// Create input object
	InputObject::UserInputState eventState = down ? InputObject::INPUT_STATE_BEGIN : InputObject::INPUT_STATE_END;

	// Use the SDL scancode provided on the keyboard event directly.
	SDL_Scancode scancode = keyEvent->scancode;

	shared_ptr<RBX::InputObject> keyInput = RBX::Creatable<RBX::Instance>::create<RBX::InputObject>(
	    InputObject::TYPE_KEYBOARD,
	    eventState,
	    keyCode,
	    modCode,
	    scancode,
	    finalChar ? std::string(1, finalChar) : std::string(),
	    game->getDataModel().get()
	);

	sendEvent(keyInput);
}

void UserInput::handleTextInputEvent(const SDL_TextInputEvent* textEvent)
{
	// SDL provides UTF-8 encoded text in textEvent->text. Forward it as a TextInput InputObject
	if (!textEvent || !game)
		return;

	const char* utf8 = textEvent->text;
	if (!utf8 || utf8[0] == '\0')
		return;

	RBX::ModCode modCodes = UserInputUtil::createModCode();

	shared_ptr<RBX::InputObject> textInput = RBX::Creatable<RBX::Instance>::create<RBX::InputObject>(
		InputObject::TYPE_TEXTINPUT,
		InputObject::INPUT_STATE_BEGIN,
		RBX::KC_UNKNOWN,
		static_cast<unsigned int>(modCodes),
		SDL_SCANCODE_UNKNOWN,
		std::string(utf8),
		game->getDataModel().get()
	);
	sendEvent(textInput);
}

void UserInput::handleTextEditingEvent(const SDL_TextEditingEvent* editingEvent)
{
	// IME composition (pre-edit). Forward as a change so GUI can display composition if desired.
	if (!editingEvent || !game)
		return;

	const char* utf8 = editingEvent->text;
	if (!utf8 || utf8[0] == '\0')
		return;

	RBX::ModCode modCodes = UserInputUtil::createModCode();

	shared_ptr<RBX::InputObject> editInput = RBX::Creatable<RBX::Instance>::create<RBX::InputObject>(
		InputObject::TYPE_TEXTINPUT,
		InputObject::INPUT_STATE_CHANGE,
		RBX::KC_UNKNOWN,
		static_cast<unsigned int>(modCodes),
		SDL_SCANCODE_UNKNOWN,
		std::string(utf8),
		game->getDataModel().get()
	);
	sendEvent(editInput);
}

void UserInput::onTextBoxFocusGained(shared_ptr<RBX::Instance> textBox)
{
	// Start SDL text input to receive unicode/IME events when a TextBox gains focus.
	startSDLTextInput();
}

void UserInput::onTextBoxFocusReleased(shared_ptr<RBX::Instance> textBox)
{
	// Stop SDL text input when focus is lost
	stopSDLTextInput();
}

void UserInput::startSDLTextInput()
{
	// SDL manages a per-window text input state. Only start if not already active.
	if (!sdlTextInputActive)
	{
		// SDL_StartTextInput on SDL3 requires the window pointer and returns bool on success.
		if (SDL_StartTextInput(window))
		{
			sdlTextInputActive = true;
		}
	}
}

void UserInput::stopSDLTextInput()
{
	if (sdlTextInputActive)
	{
		if (SDL_StopTextInput(window))
		{
			sdlTextInputActive = false;
		}
	}
}

void UserInput::clearAllKeys()
{
	// Send key up events for all currently pressed keys
	for (const auto& keyCode : downKeys) {
		RBX::ModCode modCode = UserInputUtil::createModCode();
		char finalChar = 0;
		if (!RBX::UserInputService::IsUsingNewKeyboardEvents() && !sdlTextInputActive) {
			finalChar = UserInputService::getModifiedKey(keyCode, modCode);
		}

		if (RBX::UserInputService* userInputService = RBX::ServiceProvider::find<RBX::UserInputService>(game->getDataModel().get())) {
			userInputService->setKeyState(keyCode, modCode, finalChar, false);
		}

		shared_ptr<RBX::InputObject> keyInput = RBX::Creatable<RBX::Instance>::create<RBX::InputObject>(
		    InputObject::TYPE_KEYBOARD,
		    InputObject::INPUT_STATE_END,
		    keyCode,
		    modCode,
		    SDL_SCANCODE_UNKNOWN,
		    std::string(1, finalChar),
		    game->getDataModel().get()
		);
		sendEvent(keyInput);
	}
	downKeys.clear();
}

void UserInput::onWindowFocusLost()
{
	clearAllKeys();
	if (isMouseAcquired) {
		unAcquireMouse();
	}
}

void UserInput::postProcessUserInput(bool cursorMoved, RBX::Vector2 wrapMouseDelta, RBX::Vector2 mouseDelta)
{
	UserInputService* userInputService = ServiceProvider::find<UserInputService>(game->getDataModel().get());

	if (userInputService->getMouseWrapMode() == UserInputService::WRAP_HYBRID && !game->getDataModel()->getMouseOverGui())
	{

	}
	else if (GameBasicSettings::singleton().inMousepanMode())
	{
		if (rightMouseDown && GameBasicSettings::singleton().getCanMousePan())
		{
			userInputService->setMouseWrapMode(UserInputService::WRAP_CENTER);
		}
		else if (!rightMouseDown)
		{
			userInputService->setMouseWrapMode(UserInputService::WRAP_AUTO);
		}
	}

	if (wrapMouseDelta != Vector2::zero())
	{
        if (RBX::Workspace* workspace = game->getDataModel()->getWorkspace())
        {
            if (Camera* camera = workspace->getCamera())
            {
                if (!(FFlag::UserAllCamerasInLua && camera->hasClientPlayer()))
                {
                    if ( camera->getCameraType() != Camera::CUSTOM_CAMERA )
                    {

                        workspace->onWrapMouse(wrapMouseDelta);
                    }
                }
            }
        }

		sendMouseEvent(InputObject::TYPE_MOUSEDELTA, InputObject::INPUT_STATE_CHANGE, Vector3(getCursorPositionInternal(), 0), Vector3(wrapMouseDelta.x, wrapMouseDelta.y, 0));
		sendMouseEvent(InputObject::TYPE_MOUSEMOVEMENT, InputObject::INPUT_STATE_CHANGE, Vector3(getCursorPositionInternal(), 0), Vector3(wrapMouseDelta.x, wrapMouseDelta.y, 0));
	}
	else
	{
		sendMouseEvent(InputObject::TYPE_MOUSEMOVEMENT, InputObject::INPUT_STATE_CHANGE, Vector3(getCursorPositionInternal(), 0), Vector3(mouseDelta.x, mouseDelta.y, 0));

	}
}

void UserInput::sendMouseEvent(InputObject::UserInputType mouseEventType,
							   InputObject::UserInputState mouseEventState,
							   const G3D::Vector3& position,
							   const G3D::Vector3& delta)
{
	FASTLOG1(FLog::UserInputProfile, "Processing mouse event from the queue, event: %u", mouseEventType);

	shared_ptr<RBX::InputObject> mouseEventObject;
	if( inputObjectMap.find(mouseEventType) == inputObjectMap.end() )
	{
		mouseEventObject = RBX::Creatable<RBX::Instance>::create<RBX::InputObject>(mouseEventType, mouseEventState, position, delta, game->getDataModel().get());
		inputObjectMap[mouseEventType] = mouseEventObject;
	}
	else
	{
		mouseEventObject = inputObjectMap[mouseEventType];
		mouseEventObject->setInputState(mouseEventState);
		mouseEventObject->setPosition(position);
		mouseEventObject->setDelta(delta);
		inputObjectMap[mouseEventType] = mouseEventObject;
	}

	sendEvent(mouseEventObject);
}

void UserInput::sendEvent(shared_ptr<InputObject> event)
{
	if( RBX::UserInputService* userInputService = RBX::ServiceProvider::find<RBX::UserInputService>(game->getDataModel().get()) )
		userInputService->fireInputEvent(event, NULL);
}

bool UserInput::isFullScreenMode() const
{
	return parentView->IsFullscreen();
}

G3D::Rect2D UserInput::getWindowRect() const
{
	int w, h;
	SDL_GetWindowSize(window, &w, &h);
	return Rect2D::xywh(0, 0, w, h);
}

G3D::Vector2int16 UserInput::getWindowSize() const
{
	G3D::Rect2D windowRect = getWindowRect();
	return G3D::Vector2int16((G3D::int16) windowRect.width(), (G3D::int16) windowRect.height());
}

TextureProxyBaseRef UserInput::getGameCursor(Adorn* adorn)
{
	return UserInputBase::getGameCursor(adorn);
}

bool UserInput::movementKeysDown()
{
	return (	keyDownInternal(RBX::KC_W)	|| keyDownInternal(RBX::KC_S) || keyDownInternal(RBX::KC_A) || keyDownInternal(RBX::KC_D)
		||	keyDownInternal(RBX::KC_UP)|| keyDownInternal(RBX::KC_DOWN) || keyDownInternal(RBX::KC_LEFT) || keyDownInternal(RBX::KC_RIGHT) );
}

G3D::Vector2 UserInput::getGameCursorPositionInternal()
{
	return getWindowRect().center() + wrapMousePosition;
}

G3D::Vector2 UserInput::getGameCursorPositionExpandedInternal()
{
	return getWindowRect().center() + Math::expandVector2(wrapMousePosition, 10);
}

G3D::Vector2 UserInput::getSDLCursorPositionInternal()
{
	// Get cached cursor position (thread-safe)
	boost::mutex::scoped_lock lock(inputMutex);
	return lastCursorPos;
}

G3D::Vector2 UserInput::getCursorPosition()
{
	return getCursorPositionInternal();
}

G3D::Vector2 UserInput::getCursorPositionInternal()
{
	G3D::Vector2 pos = isMouseAcquired ? getGameCursorPositionInternal() : getSDLCursorPositionInternal();
	return pos;
}

bool UserInput::keyDownInternal(RBX::KeyCode code) const
{
	// Check for externally forced key state
	if (externallyForcedKeyDown && (code == externallyForcedKeyDown)) {
		return true;
	}

	// Check the set of currently pressed keys tracked by events
	return downKeys.find(code) != downKeys.end();
}

bool UserInput::keyDown(RBX::KeyCode code) const
{
	return keyDownInternal(code);
}

void UserInput::setKeyState(RBX::KeyCode code, RBX::ModCode modCode, char modifiedKey, bool isDown)
{
	externallyForcedKeyDown = isDown ? code : 0;
}

void UserInput::doWrapMouse(const G3D::Vector2& delta, G3D::Vector2& wrapMouseDelta)
{
	UserInputService* userInputService = ServiceProvider::find<UserInputService>(game->getDataModel().get());

	switch (userInputService->getMouseWrapMode())
	{
	case UserInputService::WRAP_NONEANDCENTER: // intentional fall thru
		acquireMouseInternal();
		centerCursor();
		break;
	case UserInputService::WRAP_NONE: // intentional fall thru
	case UserInputService::WRAP_CENTER:
		acquireMouseInternal();
        UserInputUtil::wrapMouseCenter(delta,
			wrapMouseDelta,
			this->wrapMousePosition);
		break;
	case UserInputService::WRAP_HYBRID:
	case UserInputService::WRAP_AUTO:
        unAcquireMouse();
		break;
	}

	if (isMouseAcquired) {
		Vector2 pos = getGameCursorPositionInternal();
		SDL_WarpMouseInWindow(window, pos.x, pos.y);
	}
	else {
		G3D::Vector2 pos = getSDLCursorPositionInternal();
		G3D::Rect2D rect = getWindowRect();
		if (rect.contains(pos)) {
			wrapMousePosition = pos - rect.center();
		}
	}
}

void UserInput::doWrapHybrid(bool cursorMoved, bool leftMouseUp, G3D::Vector2& wrapMouseDelta, G3D::Vector2& wrapMousePosition, G3D::Vector2& posToWrapTo)
{
}

void UserInput::doDiagnostics()
{
	// Diagnostics
	static int errorCount = 0;
	static bool haveFailed = false;
	if (isMouseAcquired)
	{
		if (haveFailed)
		{
			if (errorCount>0)
			{
				StandardOut::singleton()->printf(MESSAGE_WARNING, "UserInput::attemptOwnMouse Failed to acquire mouse exclusively %d time(s)", errorCount);
				errorCount = 0;
			}
			haveFailed = false;
		}
	}
	else
	{
		static G3D::RealTime nextMessage = G3D::System::time();
		++errorCount;
		haveFailed = true;
		if (nextMessage <= G3D::System::time())
		{
			StandardOut::singleton()->printf(MESSAGE_WARNING, "UserInput::attemptOwnMouse Failed to acquire mouse exclusively %d time(s)", errorCount);
			errorCount = 0;
			nextMessage = G3D::System::time() + 10.0f;		// wait 10 seconds so as not to slow things down with logging
		}
	}
}

}  // namespace RBX
