// KeyCode.cpp
#include "stdafx.h"
#include "util/KeyCode.h"
#include "reflection/EnumConverter.h"

namespace RBX {
	namespace Reflection {

		template<>
		EnumDesc<KeyCode>::EnumDesc() : EnumDescriptor("KeyCode")
		{
			// only adding the roblox keycodes, not the sdl mappings.
			addPair(KeyCode::KC_UNKNOWN, "Unknown");
			addPair(KeyCode::KC_BACKSPACE, "Backspace");
			addPair(KeyCode::KC_TAB, "Tab");
			addPair(KeyCode::KC_CLEAR, "Clear");
			addPair(KeyCode::KC_RETURN, "Return");
			addPair(KeyCode::KC_PAUSE, "Pause");
			addPair(KeyCode::KC_ESCAPE, "Escape");
			addPair(KeyCode::KC_SPACE, "Space");
			addPair(KeyCode::KC_QUOTEDBL, "QuotedDouble");
			addPair(KeyCode::KC_HASH, "Hash");
			addPair(KeyCode::KC_DOLLAR, "Dollar");
			addPair(KeyCode::KC_PERCENT, "Percent");
			addPair(KeyCode::KC_AMPERSAND, "Ampersand");
			addPair(KeyCode::KC_QUOTE, "Quote");
			addPair(KeyCode::KC_LEFTPAREN, "LeftParenthesis");
			addPair(KeyCode::KC_RIGHTPAREN, "RightParenthesis");
			addPair(KeyCode::KC_ASTERISK, "Asterisk");
			addPair(KeyCode::KC_PLUS, "Plus");
			addPair(KeyCode::KC_COMMA, "Comma");
			addPair(KeyCode::KC_MINUS, "Minus");
			addPair(KeyCode::KC_PERIOD, "Period");
			addPair(KeyCode::KC_SLASH, "Slash");

			addPair(KeyCode::KC_0, "Zero");
			addPair(KeyCode::KC_1, "One");
			addPair(KeyCode::KC_2, "Two");
			addPair(KeyCode::KC_3, "Three");
			addPair(KeyCode::KC_4, "Four");
			addPair(KeyCode::KC_5, "Five");
			addPair(KeyCode::KC_6, "Six");
			addPair(KeyCode::KC_7, "Seven");
			addPair(KeyCode::KC_8, "Eight");
			addPair(KeyCode::KC_9, "Nine");

			addPair(KeyCode::KC_COLON, "Colon");
			addPair(KeyCode::KC_SEMICOLON, "Semicolon");
			addPair(KeyCode::KC_LESS, "LessThan");
			addPair(KeyCode::KC_EQUALS, "Equals");
			addPair(KeyCode::KC_GREATER, "GreaterThan");
			addPair(KeyCode::KC_QUESTION, "Question");
			addPair(KeyCode::KC_AT, "At");
			addPair(KeyCode::KC_LEFTBRACKET, "LeftBracket");
			addPair(KeyCode::KC_BACKSLASH, "BackSlash");
			addPair(KeyCode::KC_RIGHTBRACKET, "RightBracket");
			addPair(KeyCode::KC_CARET, "Caret");
			addPair(KeyCode::KC_UNDERSCORE, "Underscore");
			addPair(KeyCode::KC_BACKQUOTE, "Backquote");

			addPair(KeyCode::KC_A, "A");
			addPair(KeyCode::KC_B, "B");
			addPair(KeyCode::KC_C, "C");
			addPair(KeyCode::KC_D, "D");
			addPair(KeyCode::KC_E, "E");
			addPair(KeyCode::KC_F, "F");
			addPair(KeyCode::KC_G, "G");
			addPair(KeyCode::KC_H, "H");
			addPair(KeyCode::KC_I, "I");
			addPair(KeyCode::KC_J, "J");
			addPair(KeyCode::KC_K, "K");
			addPair(KeyCode::KC_L, "L");
			addPair(KeyCode::KC_M, "M");
			addPair(KeyCode::KC_N, "N");
			addPair(KeyCode::KC_O, "O");
			addPair(KeyCode::KC_P, "P");
			addPair(KeyCode::KC_Q, "Q");
			addPair(KeyCode::KC_R, "R");
			addPair(KeyCode::KC_S, "S");
			addPair(KeyCode::KC_T, "T");
			addPair(KeyCode::KC_U, "U");
			addPair(KeyCode::KC_V, "V");
			addPair(KeyCode::KC_W, "W");
			addPair(KeyCode::KC_X, "X");
			addPair(KeyCode::KC_Y, "Y");
			addPair(KeyCode::KC_Z, "Z");

			addPair(KeyCode::KC_LEFTCURLY, "LeftCurly");
			addPair(KeyCode::KC_PIPE, "Pipe");
			addPair(KeyCode::KC_RIGHTCURLY, "RightCurly");
			addPair(KeyCode::KC_TILDE, "Tilde");
			addPair(KeyCode::KC_DELETE, "Delete");

			addPair(KeyCode::KC_KP0, "KeypadZero");
			addPair(KeyCode::KC_KP1, "KeypadOne");
			addPair(KeyCode::KC_KP2, "KeypadTwo");
			addPair(KeyCode::KC_KP3, "KeypadThree");
			addPair(KeyCode::KC_KP4, "KeypadFour");
			addPair(KeyCode::KC_KP5, "KeypadFive");
			addPair(KeyCode::KC_KP6, "KeypadSix");
			addPair(KeyCode::KC_KP7, "KeypadSeven");
			addPair(KeyCode::KC_KP8, "KeypadEight");
			addPair(KeyCode::KC_KP9, "KeypadNine");
			addPair(KeyCode::KC_KP_PERIOD, "KeypadPeriod");
			addPair(KeyCode::KC_KP_DIVIDE, "KeypadDivide");
			addPair(KeyCode::KC_KP_MULTIPLY, "KeypadMultiply");
			addPair(KeyCode::KC_KP_MINUS, "KeypadMinus");
			addPair(KeyCode::KC_KP_PLUS, "KeypadPlus");
			addPair(KeyCode::KC_KP_ENTER, "KeypadEnter");
			addPair(KeyCode::KC_KP_EQUALS, "KeypadEquals");

			addPair(KeyCode::KC_UP, "Up");
			addPair(KeyCode::KC_DOWN, "Down");
			addPair(KeyCode::KC_RIGHT, "Right");
			addPair(KeyCode::KC_LEFT, "Left");
			addPair(KeyCode::KC_INSERT, "Insert");
			addPair(KeyCode::KC_HOME, "Home");
			addPair(KeyCode::KC_END, "End");
			addPair(KeyCode::KC_PAGEUP, "PageUp");
			addPair(KeyCode::KC_PAGEDOWN, "PageDown");

			addPair(KeyCode::KC_LSHIFT, "LeftShift");
			addPair(KeyCode::KC_RSHIFT, "RightShift");
			addPair(KeyCode::KC_LMETA, "LeftMeta");
			addPair(KeyCode::KC_RMETA, "RightMeta");
			addPair(KeyCode::KC_LALT, "LeftAlt");
			addPair(KeyCode::KC_RALT, "RightAlt");
			addPair(KeyCode::KC_LCTRL, "LeftControl");
			addPair(KeyCode::KC_RCTRL, "RightControl");
			addPair(KeyCode::KC_CAPSLOCK, "CapsLock");
			addPair(KeyCode::KC_NUMLOCK, "NumLock");
			addPair(KeyCode::KC_SCROLLOCK, "ScrollLock");

			addPair(KeyCode::KC_LSUPER, "LeftSuper");
			addPair(KeyCode::KC_RSUPER, "RightSuper");
			addPair(KeyCode::KC_MODE, "Mode");
			addPair(KeyCode::KC_COMPOSE, "Compose");

			addPair(KeyCode::KC_HELP, "Help");
			addPair(KeyCode::KC_PRINT, "Print");
			addPair(KeyCode::KC_SYSREQ, "SysReq");
			addPair(KeyCode::KC_BREAK, "Break");
			addPair(KeyCode::KC_MENU, "Menu");
			addPair(KeyCode::KC_POWER, "Power");
			addPair(KeyCode::KC_EURO, "Euro");
			addPair(KeyCode::KC_UNDO, "Undo");

			addPair(KeyCode::KC_F1, "F1");
			addPair(KeyCode::KC_F2, "F2");
			addPair(KeyCode::KC_F3, "F3");
			addPair(KeyCode::KC_F4, "F4");
			addPair(KeyCode::KC_F5, "F5");
			addPair(KeyCode::KC_F6, "F6");
			addPair(KeyCode::KC_F7, "F7");
			addPair(KeyCode::KC_F8, "F8");
			addPair(KeyCode::KC_F9, "F9");
			addPair(KeyCode::KC_F10, "F10");
			addPair(KeyCode::KC_F11, "F11");
			addPair(KeyCode::KC_F12, "F12");
			addPair(KeyCode::KC_F13, "F13");
			addPair(KeyCode::KC_F14, "F14");
			addPair(KeyCode::KC_F15, "F15");

			const std::string worldString("World");
			for (int i = KeyCode::KC_WORLD_0; i <= KeyCode::KC_WORLD_95; ++i)
			{
				int num = i - KeyCode::KC_WORLD_0;

				std::stringstream ss;
				ss << worldString;
				ss << num;

				addPair((KeyCode)i, ss.str().c_str());
			}

			addPair(KeyCode::KC_GAMEPAD_BUTTONX, "ButtonX");
			addPair(KeyCode::KC_GAMEPAD_BUTTONY, "ButtonY");
			addPair(KeyCode::KC_GAMEPAD_BUTTONA, "ButtonA");
			addPair(KeyCode::KC_GAMEPAD_BUTTONB, "ButtonB");
			addPair(KeyCode::KC_GAMEPAD_BUTTONR1, "ButtonR1");
			addPair(KeyCode::KC_GAMEPAD_BUTTONL1, "ButtonL1");
			addPair(KeyCode::KC_GAMEPAD_BUTTONR2, "ButtonR2");
			addPair(KeyCode::KC_GAMEPAD_BUTTONL2, "ButtonL2");
			addPair(KeyCode::KC_GAMEPAD_BUTTONR3, "ButtonR3");
			addPair(KeyCode::KC_GAMEPAD_BUTTONL3, "ButtonL3");
			addPair(KeyCode::KC_GAMEPAD_BUTTONSTART, "ButtonStart");
			addPair(KeyCode::KC_GAMEPAD_BUTTONSELECT, "ButtonSelect");
			addPair(KeyCode::KC_GAMEPAD_DPADLEFT, "DPadLeft");
			addPair(KeyCode::KC_GAMEPAD_DPADRIGHT, "DPadRight");
			addPair(KeyCode::KC_GAMEPAD_DPADUP, "DPadUp");
			addPair(KeyCode::KC_GAMEPAD_DPADDOWN, "DPadDown");
			addPair(KeyCode::KC_GAMEPAD_THUMBSTICK1, "Thumbstick1");
			addPair(KeyCode::KC_GAMEPAD_THUMBSTICK2, "Thumbstick2");
		}

		template<>
		KeyCode& Variant::convert<KeyCode>(void)
		{
			return genericConvert<KeyCode>();
		}

	} // namespace Reflection

	template<>
	bool StringConverter<KeyCode>::convertToValue(const std::string& text, KeyCode& value)
	{
		return Reflection::EnumDesc<KeyCode>::singleton().convertToValue(text.c_str(), value);
	}
} // namespace RBX
