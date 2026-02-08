#pragma once

#include "util/KeyCode.h"
#include "util/G3DCore.h"
#include "GfxBase/TextureProxyBase.h"
#include "util/ContentId.h"
#include "util/TextureId.h"
#include "util/Object.h"
#include "GfxBase/Adorn.h"

LOGGROUP(UserInputProfile)

namespace RBX {


	class Adorn;
	class NavKeys;

	// TODO: Rename HardwareDevice or something
	class RBXBaseClass UserInputBase
	{
	private:
		ContentId currentCursorId;
		TextureProxyBaseRef currentCursor;
		TextureProxyBaseRef fallbackCursor;
		rbx::signals::scoped_connection unbindResourceSignal;

        void onUnbindResourceSignal();

	protected:
		virtual Vector2 getCursorPosition() = 0;
		virtual TextureProxyBaseRef getGameCursor(Adorn* adorn);
		TextureProxyBaseRef getCurrentCursor() { return currentCursor; }
		TextureProxyBaseRef getFallbackCursor() { return fallbackCursor; }

	public:
		UserInputBase();
		~UserInputBase() {}

		rbx::signal<void()> cursorIdChangedSignal;

		// This function is purely intended for debugging and diagnostics
		Vector2 getCursorPositionForDebugging()
		{
			return getCursorPosition();
		}

		virtual void removeJobs() {}

		/////////////////////////////////////////////////////////////////////
		// Mouse Wrapping
		//
		virtual void centerCursor() = 0;

		/////////////////////////////////////////////////////////////////////
		// Real-time Key Handling
		//
		virtual bool keyDown(KeyCode code) const = 0;

		void getNavKeys(NavKeys& navKeys,const bool shouldSuppressNavKeys) const;

		bool altKeyDown() const		{return keyDown(KC_RALT) || keyDown(KC_LALT);}
		bool shiftKeyDown() const	{return keyDown(KC_RSHIFT) || keyDown(KC_LSHIFT);}
		bool ctrlKeyDown() const	{return keyDown(KC_RCTRL) || keyDown(KC_LCTRL);}

		// allows Gui Key buttons to "press" keys
        virtual void setKeyState(KeyCode code, ModCode modCode, char modifiedKey, bool isDown) = 0;

		/////////////////////////////////////////////////////////////////////
		// Cursor Handling
		//
		virtual bool isHardwareCursorActive() const { return false; }
		ContentId getCurrentCursorId() { return currentCursorId; }
		virtual bool setCursorId(Adorn *adorn, const TextureId& id);
		virtual void renderGameCursor(Adorn* adorn);
	};
} // namespace
