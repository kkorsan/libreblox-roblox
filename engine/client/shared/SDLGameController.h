#pragma once

#include <boost/shared_ptr.hpp>
#include <boost/unordered_map.hpp>
#include <boost/weak_ptr.hpp>

#include <SDL3/SDL.h>
#ifdef nullptr
#undef nullptr
#endif

#ifdef nullptr
#undef nullptr
#endif

#include "util/KeyCode.h"
#include "v8datamodel/InputObject.h"
#include "v8datamodel/HapticService.h"

namespace RBX
{
	class DataModel;
	class UserInputService;

	class GamepadService;

	typedef boost::unordered_map<RBX::KeyCode, boost::shared_ptr<RBX::InputObject> > Gamepad;
	}

	struct HapticData
	{
		int hapticEffectId;
		float currentLeftMotorValue;
		float currentRightMotorValue;
		SDL_Haptic* hapticDevice;
	};

	class SDLGameController
	{
	private:
		boost::weak_ptr<RBX::DataModel> dataModel;
		boost::unordered_map<int, std::pair<int,SDL_Gamepad*> > gamepadIdToGameController;
		boost::unordered_map<int, HapticData> hapticsFromGamepadId;
		boost::unordered_map<int, int> joystickIdToGamepadId;

	rbx::signals::scoped_connection renderSteppedConnection;
	rbx::signals::scoped_connection getSupportedGamepadKeyCodesConnection;

	rbx::signals::scoped_connection setEnabledVibrationMotorsConnection;
	rbx::signals::scoped_connection setVibrationMotorConnection;

	void initSDL();

	RBX::UserInputService* getUserInputService();
	RBX::HapticService* getHapticService();

	RBX::GamepadService* getGamepadService();
	RBX::Gamepad getRbxGamepadFromJoystickId(int joystickId);

	void setupControllerId(int joystickId, int gamepadId, SDL_Gamepad *pad);
	SDL_Gamepad* removeControllerMapping(int joystickId);

	int getGamepadIntForEnum(RBX::InputObject::UserInputType gamepadType);

	void findAvailableGamepadKeyCodesAndSet(RBX::InputObject::UserInputType gamepadType);
	boost::shared_ptr<const RBX::Reflection::ValueArray> getAvailableGamepadKeyCodes(RBX::InputObject::UserInputType gamepadType);

	void bindToDataModel();

	// Haptic Functions
	void refreshHapticEffects();
	bool setupHapticsForDevice(int id);

	void setVibrationMotorsEnabled(RBX::InputObject::UserInputType gamepadType);
	void setVibrationMotor(RBX::InputObject::UserInputType gamepadType, RBX::HapticService::VibrationMotor vibrationMotor, shared_ptr<const RBX::Reflection::Tuple> args);

public:
	SDLGameController(boost::shared_ptr<RBX::DataModel> newDM);
	~SDLGameController();

	void updateControllers();
	void handleEvent(const SDL_Event& event);

	void onControllerAxis( const SDL_GamepadAxisEvent sdlEvent );
	void onControllerButton( const SDL_GamepadButtonEvent sdlEvent );
	void removeController(int joystickId);
	void addController(int gamepadId);
};
