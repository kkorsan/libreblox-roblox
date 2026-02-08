#include "stdafx.h"
#include "SDLGameController.h"

#include <SDL3/SDL.h>
#ifdef nullptr
#undef nullptr
#endif


#include "v8datamodel/DataModel.h"
#include "v8datamodel/GamepadService.h"
#include "v8datamodel/UserInputService.h"
#include "v8datamodel/ContentProvider.h"

#define MAX_AXIS_VALUE 32767.0f

SDLGameController::SDLGameController(shared_ptr<RBX::DataModel> newDM)
{
	dataModel = newDM;
    initSDL();
}

void SDLGameController::initSDL()
{
	if (!SDL_Init(SDL_INIT_GAMEPAD | SDL_INIT_HAPTIC))
{
		std::string error = SDL_GetError();
		fprintf(stderr,
			"\nUnable to initialize SDL:  %s\n",
			error.c_str()
			);
		return;
	}

	RBX::ContentId gameControllerDb = RBX::ContentId::fromAssets("fonts/gamecontrollerdb.txt");
	std::string filePath = RBX::ContentProvider::findAsset(gameControllerDb);

	if (SDL_AddGamepadMappingsFromFile(filePath.c_str()) == -1)
	{
		std::string error = SDL_GetError();
		RBX::StandardOut::singleton()->printf(RBX::MESSAGE_ERROR, "Unable to add SDL controller mappings because %s", error.c_str());
	}

	if (shared_ptr<RBX::DataModel> sharedDM = dataModel.lock())
	{
		sharedDM->submitTask(boost::bind(&SDLGameController::bindToDataModel, this), RBX::DataModelJob::Write);
	}
}

void SDLGameController::bindToDataModel()
{
	if (RBX::UserInputService* inputService = getUserInputService())
	{
		renderSteppedConnection = inputService->updateInputSignal.connect(boost::bind(&SDLGameController::updateControllers, this));
		getSupportedGamepadKeyCodesConnection = inputService->getSupportedGamepadKeyCodesSignal.connect(boost::bind(&SDLGameController::findAvailableGamepadKeyCodesAndSet, this, _1));
	}

	if (RBX::HapticService* hapticService = getHapticService())
	{
		setEnabledVibrationMotorsConnection = hapticService->setEnabledVibrationMotorsSignal.connect(boost::bind(&SDLGameController::setVibrationMotorsEnabled, this, _1));
		setVibrationMotorConnection = hapticService->setVibrationMotorSignal.connect(boost::bind(&SDLGameController::setVibrationMotor, this, _1, _2, _3));
	}
}

SDLGameController::~SDLGameController()
{
	renderSteppedConnection.disconnect();
	getSupportedGamepadKeyCodesConnection.disconnect();

	setEnabledVibrationMotorsConnection.disconnect();
	setVibrationMotorConnection.disconnect();

	for (boost::unordered_map<int, HapticData>::iterator iter = hapticsFromGamepadId.begin(); iter != hapticsFromGamepadId.end(); ++iter )
	{
		SDL_Haptic* haptic = iter->second.hapticDevice;
		int hapticEffectId = iter->second.hapticEffectId;

		SDL_DestroyHapticEffect(haptic, hapticEffectId);
		SDL_CloseHaptic(haptic);
	}
	hapticsFromGamepadId.clear();

    SDL_Quit();
}

RBX::UserInputService* SDLGameController::getUserInputService()
{
	if (shared_ptr<RBX::DataModel> sharedDM = dataModel.lock())
	{
		if (RBX::UserInputService* inputService = RBX::ServiceProvider::create<RBX::UserInputService>(sharedDM.get()))
		{
			return inputService;
		}
	}

	return NULL;
}

RBX::HapticService* SDLGameController::getHapticService()
{
	if (shared_ptr<RBX::DataModel> sharedDM = dataModel.lock())
	{
		if (RBX::HapticService* hapticService = RBX::ServiceProvider::create<RBX::HapticService>(sharedDM.get()))
		{
			return hapticService;
		}
	}

	return NULL;
}

RBX::GamepadService* SDLGameController::getGamepadService()
{
	if (shared_ptr<RBX::DataModel> sharedDM = dataModel.lock())
	{
		if (RBX::GamepadService* gamepadService = RBX::ServiceProvider::create<RBX::GamepadService>(sharedDM.get()))
		{
			return gamepadService;
		}
	}

	return NULL;
}

SDL_Gamepad* SDLGameController::removeControllerMapping(int joystickId)
{
	SDL_Gamepad* gameController = NULL;
	RBX::UserInputService* inputService = getUserInputService();

	if (joystickIdToGamepadId.find(joystickId) != joystickIdToGamepadId.end())
	{
		int gamepadId = joystickIdToGamepadId[joystickId];
		if (gamepadIdToGameController.find(gamepadId) != gamepadIdToGameController.end())
		{
			gameController = gamepadIdToGameController[gamepadId].second;
			gamepadIdToGameController.erase(gamepadId);

			if (inputService)
			{
                inputService->safeFireGamepadDisconnected(RBX::GamepadService::getGamepadEnumForInt(gamepadId));
			}
		}

		if (hapticsFromGamepadId.find(gamepadId) != hapticsFromGamepadId.end())
		{
			SDL_Haptic* haptic = hapticsFromGamepadId[gamepadId].hapticDevice;
			int hapticEffectId = hapticsFromGamepadId[gamepadId].hapticEffectId;

			SDL_DestroyHapticEffect(haptic, hapticEffectId);
			SDL_CloseHaptic(haptic);

			hapticsFromGamepadId.erase(gamepadId);
		}
	}

	return gameController;
}

void SDLGameController::setupControllerId(int joystickId, int gamepadId, SDL_Gamepad *pad)
{
	gamepadIdToGameController[gamepadId] = std::pair<int,SDL_Gamepad*>(joystickId, pad);
	joystickIdToGamepadId[joystickId] = gamepadId;

	if (RBX::UserInputService* inputService = getUserInputService())
	{
        inputService->safeFireGamepadConnected(RBX::GamepadService::getGamepadEnumForInt(gamepadId));
	}
}

void SDLGameController::addController(int gamepadId)
{
	if ( SDL_IsGamepad(gamepadId) )
	{
		SDL_Gamepad *pad = SDL_OpenGamepad(gamepadId);

		if (pad)
		{
			SDL_Joystick *joy = SDL_GetGamepadJoystick(pad);
			int joystickId = SDL_GetJoystickID(joy);

			setupControllerId(joystickId, gamepadId, pad);
		}
	}
}

void SDLGameController::removeController(int joystickId)
{
	if (SDL_Gamepad *pad = removeControllerMapping(joystickId))
	{
		SDL_CloseGamepad(pad);
	}
}

RBX::Gamepad SDLGameController::getRbxGamepadFromJoystickId(int joystickId)
{
	if (joystickIdToGamepadId.find(joystickId) != joystickIdToGamepadId.end())
	{
		if (RBX::GamepadService* gamepadService = getGamepadService())
		{
			int gamepadId = joystickIdToGamepadId[joystickId];
			return gamepadService->getGamepadState(gamepadId);
		}
	}

	return RBX::Gamepad();
}

RBX::KeyCode getKeyCodeFromSDLAxis(SDL_GamepadAxis sdlAxis, int& axisValueChanged)
{
	switch (sdlAxis)
	{
	case SDL_GAMEPAD_AXIS_LEFTX:
		axisValueChanged = 0;
		return RBX::KC_GAMEPAD_THUMBSTICK1;
	case SDL_GAMEPAD_AXIS_LEFTY:
		axisValueChanged = 1;
		return RBX::KC_GAMEPAD_THUMBSTICK1;
	case SDL_GAMEPAD_AXIS_RIGHTX:
		axisValueChanged = 0;
		return RBX::KC_GAMEPAD_THUMBSTICK2;
	case SDL_GAMEPAD_AXIS_RIGHTY:
		axisValueChanged = 1;
		return RBX::KC_GAMEPAD_THUMBSTICK2;
	case SDL_GAMEPAD_AXIS_LEFT_TRIGGER:
		axisValueChanged = 2;
		return RBX::KC_GAMEPAD_BUTTONL2;
	case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER:
		axisValueChanged = 2;
		return RBX::KC_GAMEPAD_BUTTONR2;
    case SDL_GAMEPAD_AXIS_INVALID:
    default:
        return RBX::KC_UNKNOWN;
	}

	return RBX::KC_UNKNOWN;
}

RBX::KeyCode getKeyCodeFromSDLButton(SDL_GamepadButton sdlButton)
{
	switch (sdlButton)
	{
	case SDL_GAMEPAD_BUTTON_SOUTH:
		return RBX::KC_GAMEPAD_BUTTONA;
	case SDL_GAMEPAD_BUTTON_EAST:
		return RBX::KC_GAMEPAD_BUTTONB;
	case SDL_GAMEPAD_BUTTON_WEST:
		return RBX::KC_GAMEPAD_BUTTONX;
	case SDL_GAMEPAD_BUTTON_NORTH:
		return RBX::KC_GAMEPAD_BUTTONY;

	case SDL_GAMEPAD_BUTTON_START:
		return RBX::KC_GAMEPAD_BUTTONSTART;
	case SDL_GAMEPAD_BUTTON_BACK:
		return RBX::KC_GAMEPAD_BUTTONSELECT;

	case SDL_GAMEPAD_BUTTON_DPAD_UP:
		return RBX::KC_GAMEPAD_DPADUP;
	case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
		return RBX::KC_GAMEPAD_DPADDOWN;
	case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
		return RBX::KC_GAMEPAD_DPADLEFT;
	case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
		return RBX::KC_GAMEPAD_DPADRIGHT;

	case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
		return RBX::KC_GAMEPAD_BUTTONL1;
	case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:
		return RBX::KC_GAMEPAD_BUTTONR1;

	case SDL_GAMEPAD_BUTTON_LEFT_STICK:
		return RBX::KC_GAMEPAD_BUTTONL3;
	case SDL_GAMEPAD_BUTTON_RIGHT_STICK:
		return RBX::KC_GAMEPAD_BUTTONR3;

    case SDL_GAMEPAD_BUTTON_INVALID:
    default:
        return RBX::KC_UNKNOWN;
	}

	return RBX::KC_UNKNOWN;
}

bool SDLGameController::setupHapticsForDevice(int id)
{
	// already set up
	if (hapticsFromGamepadId.find(id) != hapticsFromGamepadId.end())
	{
		return true;
	}

	SDL_Haptic *haptic = NULL;

	// Open the device
	haptic = SDL_OpenHaptic(id);
	if (haptic)
	{
		HapticData hapticData;
		hapticData.hapticDevice = haptic;
		hapticData.hapticEffectId = -1;
		hapticData.currentLeftMotorValue = 0.0f;
		hapticData.currentRightMotorValue = 0.0f;

		hapticsFromGamepadId[id] = hapticData;

		return true;
	}

	return false;
}

void SDLGameController::setVibrationMotorsEnabled(RBX::InputObject::UserInputType gamepadType)
{
	int gamepadId = getGamepadIntForEnum(gamepadType);
	if (!setupHapticsForDevice(gamepadId))
	{
		return;
	}

	SDL_Haptic* haptic = hapticsFromGamepadId[gamepadId].hapticDevice;
	if (haptic)
	{
		if (RBX::HapticService* hapticService = getHapticService())
		{
			hapticService->setEnabledVibrationMotors(gamepadType, RBX::HapticService::MOTOR_LARGE, true);
			hapticService->setEnabledVibrationMotors(gamepadType, RBX::HapticService::MOTOR_SMALL, true);
			hapticService->setEnabledVibrationMotors(gamepadType, RBX::HapticService::MOTOR_LEFTTRIGGER, false);
			hapticService->setEnabledVibrationMotors(gamepadType, RBX::HapticService::MOTOR_RIGHTTRIGGER, false);
		}
	}
}

void SDLGameController::setVibrationMotor(RBX::InputObject::UserInputType gamepadType, RBX::HapticService::VibrationMotor vibrationMotor, shared_ptr<const RBX::Reflection::Tuple> args)
{
	int gamepadId = getGamepadIntForEnum(gamepadType);
	if (!setupHapticsForDevice(gamepadId))
	{
		return;
	}

	float newMotorValue = 0.0f;
	RBX::Reflection::Variant newValue = args->values[0];
	if (newValue.isFloat())
	{
		newMotorValue = newValue.get<float>();
		newMotorValue = G3D::clamp(newMotorValue, 0.0f, 1.0f);
	}
	else // no valid number in first position, lets bail
	{
		RBX::StandardOut::singleton()->printf(RBX::MESSAGE_ERROR, "First value to HapticService:SetMotor is not a valid number (must be a number between 0-1)");
		return;
	}

	boost::unordered_map<int, HapticData>::iterator iter = hapticsFromGamepadId.find(gamepadId);

	// make sure we grab old data so we set the motors that haven't changed value
	float leftMotorValue = iter->second.currentLeftMotorValue;
	float rightMotorValue = iter->second.currentRightMotorValue;

	if (vibrationMotor == RBX::HapticService::MOTOR_LARGE)
	{
		leftMotorValue = newMotorValue;
	}
	else if (vibrationMotor == RBX::HapticService::MOTOR_SMALL)
	{
		rightMotorValue = newMotorValue;
	}

	SDL_Haptic* haptic = iter->second.hapticDevice;
	int oldEffectId = iter->second.hapticEffectId;
	if (oldEffectId >= 0)
	{
		SDL_DestroyHapticEffect(haptic, oldEffectId);
	}

	if (leftMotorValue <= 0.0f && rightMotorValue <= 0.0f)
	{
		HapticData hapticData;
		hapticData.hapticDevice = haptic;
		hapticData.hapticEffectId = -1;
		hapticData.currentLeftMotorValue = 0.0f;
		hapticData.currentRightMotorValue = 0.0f;

		hapticsFromGamepadId[gamepadId] = hapticData;
		return;
	}

	// Create the left/right effect
	SDL_HapticEffect effect;
	memset( &effect, 0, sizeof(SDL_HapticEffect) ); // 0 is safe default
	effect.type = SDL_HAPTIC_LEFTRIGHT;
	effect.leftright.large_magnitude = 65535.0f * leftMotorValue;
	effect.leftright.small_magnitude = 65535.0f * rightMotorValue;
	effect.leftright.length = SDL_HAPTIC_INFINITY;

	// Upload the effect
	int hapticEffectId = SDL_CreateHapticEffect(haptic, &effect);

	HapticData hapticData;
	hapticData.hapticDevice = haptic;
	hapticData.hapticEffectId = hapticEffectId;
	hapticData.currentLeftMotorValue = leftMotorValue;
	hapticData.currentRightMotorValue = rightMotorValue;

	hapticsFromGamepadId[gamepadId] = hapticData;

	if (haptic && hapticEffectId >= 0)
	{
		SDL_RunHapticEffect(haptic, hapticEffectId, SDL_HAPTIC_INFINITY);
	}
}

void SDLGameController::refreshHapticEffects()
{
	for (boost::unordered_map<int, HapticData>::iterator iter = hapticsFromGamepadId.begin(); iter != hapticsFromGamepadId.end(); ++iter )
	{
		SDL_Haptic* haptic = iter->second.hapticDevice;
		int hapticEffectId = iter->second.hapticEffectId;

		if (haptic && hapticEffectId >= 0)
		{
			SDL_RunHapticEffect(haptic, hapticEffectId, SDL_HAPTIC_INFINITY);
		}
	}
}

void SDLGameController::onControllerButton( const SDL_GamepadButtonEvent sdlEvent )
{
	const RBX::KeyCode buttonCode = getKeyCodeFromSDLButton((SDL_GamepadButton) sdlEvent.button);

	if (buttonCode == RBX::KC_UNKNOWN)
	{
		return;
	}

	RBX::Gamepad gamepad = getRbxGamepadFromJoystickId(sdlEvent.which);
	const int buttonState = (sdlEvent.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) ? 1 : 0;
	RBX::InputObject::UserInputState newState = (buttonState == 1) ? RBX::InputObject::INPUT_STATE_BEGIN : RBX::InputObject::INPUT_STATE_END;

	if (newState == gamepad[buttonCode]->getUserInputState())
	{
		return;
	}

	const G3D::Vector3 lastPos = gamepad[buttonCode]->getPosition();

	gamepad[buttonCode]->setPosition(G3D::Vector3(0,0,buttonState));
	gamepad[buttonCode]->setDelta(gamepad[buttonCode]->getPosition() - lastPos);
	gamepad[buttonCode]->setInputState(newState);

	if (RBX::UserInputService* inputService = getUserInputService())
	{
        inputService->dangerousFireInputEvent(gamepad[buttonCode], NULL);
	}
}

void SDLGameController::onControllerAxis( const SDL_GamepadAxisEvent sdlEvent )
{
	int axisValueChanged = -1;
	const RBX::KeyCode axisCode = getKeyCodeFromSDLAxis((SDL_GamepadAxis) sdlEvent.axis, axisValueChanged);

	if (axisCode == RBX::KC_UNKNOWN)
	{
		return;
	}

	float axisValue = sdlEvent.value;
	axisValue /= MAX_AXIS_VALUE;
	axisValue = G3D::clamp(axisValue, -1.0f, 1.0f);

	RBX::Gamepad gamepad = getRbxGamepadFromJoystickId(sdlEvent.which);
	G3D::Vector3 currentPosition = gamepad[axisCode]->getPosition();

	switch (axisValueChanged)
	{
	case 0:
		currentPosition.x = axisValue;
		break;
	case 1:
		currentPosition.y = -axisValue;
		break;
	case 2:
		currentPosition.z = axisValue;
		break;
	default:
		break;
	}

	G3D::Vector3 lastPos = gamepad[axisCode]->getPosition();
	if (lastPos != currentPosition)
	{
		gamepad[axisCode]->setPosition(currentPosition);

		RBX::InputObject::UserInputState currentState = RBX::InputObject::INPUT_STATE_CHANGE;
		if (currentPosition == G3D::Vector3::zero())
		{
			currentState = RBX::InputObject::INPUT_STATE_END;
		}
		else if (currentPosition.z >= 1.0f)
		{
			currentState = RBX::InputObject::INPUT_STATE_BEGIN;
		}

		gamepad[axisCode]->setDelta(currentPosition - lastPos);
		gamepad[axisCode]->setInputState(currentState);

		if (RBX::UserInputService* inputService = getUserInputService())
		{
            inputService->dangerousFireInputEvent(gamepad[axisCode], NULL);
		}
	}
}

void SDLGameController::handleEvent(const SDL_Event& sdlEvent)
{
	switch( sdlEvent.type )
	{
	case SDL_EVENT_GAMEPAD_ADDED:
		addController( sdlEvent.gdevice.which );
		break;

	case SDL_EVENT_GAMEPAD_REMOVED:
		removeController( sdlEvent.gdevice.which );
		break;

	case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
	case SDL_EVENT_GAMEPAD_BUTTON_UP:
		onControllerButton( sdlEvent.gbutton );
		break;

	case SDL_EVENT_GAMEPAD_AXIS_MOTION:
		onControllerAxis( sdlEvent.gaxis );
		break;

	default:
		break;
	}
}

void SDLGameController::updateControllers()
{
	// Polling removed to avoid thread safety issues.
	// Events are now pushed via handleEvent() from the main thread.

	refreshHapticEffects();
}

RBX::KeyCode getKeyCodeFromSDLName(std::string sdlName)
{
	if (sdlName.compare("a") == 0)
		return RBX::KC_GAMEPAD_BUTTONA;
	if (sdlName.compare("b") == 0)
		return RBX::KC_GAMEPAD_BUTTONB;
	if (sdlName.compare("x") == 0)
		return RBX::KC_GAMEPAD_BUTTONX;
	if (sdlName.compare("y") == 0)
		return RBX::KC_GAMEPAD_BUTTONY;

	if (sdlName.compare("back") == 0)
		return RBX::KC_GAMEPAD_BUTTONSELECT;
	if (sdlName.compare("start") == 0)
		return RBX::KC_GAMEPAD_BUTTONSTART;

	if (sdlName.compare("dpdown") == 0)
		return RBX::KC_GAMEPAD_DPADDOWN;
	if (sdlName.compare("dpleft") == 0)
		return RBX::KC_GAMEPAD_DPADLEFT;
	if (sdlName.compare("dpright") == 0)
		return RBX::KC_GAMEPAD_DPADRIGHT;
	if (sdlName.compare("dpup") == 0)
		return RBX::KC_GAMEPAD_DPADUP;

	if (sdlName.compare("leftshoulder") == 0)
		return RBX::KC_GAMEPAD_BUTTONL1;
	if (sdlName.compare("lefttrigger") == 0)
		return RBX::KC_GAMEPAD_BUTTONL2;
	if (sdlName.compare("leftstick") == 0)
		return RBX::KC_GAMEPAD_BUTTONL3;

	if (sdlName.compare("rightshoulder") == 0)
		return RBX::KC_GAMEPAD_BUTTONR1;
	if (sdlName.compare("righttrigger") == 0)
		return RBX::KC_GAMEPAD_BUTTONR2;
	if (sdlName.compare("rightstick") == 0)
		return RBX::KC_GAMEPAD_BUTTONR3;

	if (sdlName.compare("leftx") == 0 ||
		sdlName.compare("lefty") == 0)
		return RBX::KC_GAMEPAD_THUMBSTICK1;

	if (sdlName.compare("rightx") == 0 ||
		sdlName.compare("righty") == 0)
		return RBX::KC_GAMEPAD_THUMBSTICK2;

	return RBX::KC_UNKNOWN;
}

int SDLGameController::getGamepadIntForEnum(RBX::InputObject::UserInputType gamepadType)
{
	switch (gamepadType)
	{
	case RBX::InputObject::TYPE_GAMEPAD1:
		return 0;
	case RBX::InputObject::TYPE_GAMEPAD2:
		return 1;
	case RBX::InputObject::TYPE_GAMEPAD3:
		return 2;
	case RBX::InputObject::TYPE_GAMEPAD4:
		return 3;
	default:
		break;
	}

	return -1;
}

void SDLGameController::findAvailableGamepadKeyCodesAndSet(RBX::InputObject::UserInputType gamepadType)
{
	shared_ptr<const RBX::Reflection::ValueArray> availableGamepadKeyCodes = getAvailableGamepadKeyCodes(gamepadType);
	if (RBX::UserInputService* inputService = getUserInputService())
	{
		inputService->setSupportedGamepadKeyCodes(gamepadType, availableGamepadKeyCodes);
	}
}

shared_ptr<const RBX::Reflection::ValueArray> SDLGameController::getAvailableGamepadKeyCodes(RBX::InputObject::UserInputType gamepadType)
{
	int gamepadId = getGamepadIntForEnum(gamepadType);

	if ( gamepadId < 0 || (gamepadIdToGameController.find(gamepadId) == gamepadIdToGameController.end()) )
	{
		return shared_ptr<const RBX::Reflection::ValueArray>();
	}

	if (SDL_Gamepad* gameController = gamepadIdToGameController[gamepadId].second)
	{
		std::string gameControllerMapping(SDL_GetGamepadMapping(gameController));

		std::istringstream controllerMappingStream(gameControllerMapping);
		std::string mappingItem;

		shared_ptr<RBX::Reflection::ValueArray> supportedGamepadFunctions(new RBX::Reflection::ValueArray());

		int count = 0;
		while(std::getline(controllerMappingStream, mappingItem, ','))
		{
			// first two settings in mapping are hardware id and device name, don't need those
			if (count > 1)
			{
				std::istringstream mappingStream(mappingItem);
				std::string sdlName;
				std::getline(mappingStream, sdlName, ':');

				// platform is always last thing defined in mappings, don't need it so we are done
				if (sdlName.compare("platform") == 0)
				{
					break;
				}

				RBX::KeyCode gamepadCode = getKeyCodeFromSDLName(sdlName);
				if (gamepadCode != RBX::KC_UNKNOWN)
				{
					supportedGamepadFunctions->push_back(gamepadCode);
				}
			}
			count++;
		}

		return supportedGamepadFunctions;
	}

	return shared_ptr<const RBX::Reflection::ValueArray>();
}
