#include "stdafx.h"
#include <vector>

#include "v8datamodel/TextBox.h"
#include "v8datamodel/UserInputService.h"
#include "v8datamodel/ScrollingFrame.h"
#include "v8datamodel/GuiService.h"
#include "util/UserInputBase.h"
#include "util/RunStateOwner.h"
#include "GfxBase/Typesetter.h"
#include "humanoid/Humanoid.h"
#include "network/Players.h"
#include "v8datamodel/Workspace.h"

LOGGROUP(Graphics)

DYNAMIC_FASTFLAGVARIABLE(DisplayTextBoxTextWhileTypingMobile, false)
DYNAMIC_FASTFLAGVARIABLE(PasteWithCapsLockOn, false)
DYNAMIC_FASTFLAGVARIABLE(TextBoxIsFocusedEnabled, false)

#define CURSOR_BLINK_RATE_SECONDS 0.3f

//todo: when SDL is on, remove all repeat key junk

namespace RBX
{

    REFLECTION_BEGIN();
	const char* const sTextBox = "TextBox";
	static const Reflection::PropDescriptor<TextBox, bool> prop_MultiLine("MultiLine", category_Data, &TextBox::getMultiLine, &TextBox::setMultiLine);
	static const Reflection::PropDescriptor<TextBox, bool> prop_ClearTextOnFocus("ClearTextOnFocus", category_Data, &TextBox::getClearTextOnFocus, &TextBox::setClearTextOnFocus);

	static const Reflection::BoundFuncDesc<TextBox, void()> func_CaptureFocus(&TextBox::captureFocus, "CaptureFocus", Security::None);
	static const Reflection::BoundFuncDesc<TextBox, void()> func_ReleaseFocus(&TextBox::releaseFocusLua, "ReleaseFocus", Security::None);
	static const Reflection::BoundFuncDesc<TextBox, bool()> func_GetFocus(&TextBox::getFocused, "IsFocused", Security::None);

	static const Reflection::EventDesc<TextBox, void(bool, const shared_ptr<Instance>)> event_FocusLost(&TextBox::focusLostSignal, "FocusLost", "enterPressed", "inputThatCausedFocusLoss");
	static const Reflection::EventDesc<TextBox, void()> event_FocusGained(&TextBox::focusGainedSignal, "Focused");

	IMPLEMENT_GUI_TEXT_MIXIN(TextBox);
    REFLECTION_END();

	TextBox::TextBox()
    : DescribedCreatable<TextBox,GuiObject,sTextBox>("TextBox", true)
    , GuiTextMixin("TextBox",BrickColor::brickBlack().color3())
    , iAmFocus(false)
    , showingCursor(false)
    , multiLine(false)
    , shouldCaptureFocus(false)
    , cursorPos(0)
    , clearTextOnFocus(true)
	, shouldFocusFromInput(true)
    , selectionActive(false)
    , selectionStart(0)
    , selectionEnd(0)
    , isMouseSelecting(false)
    , selectionRepeatActive(false)
	{
		selectable = true;
		setGuiQueue(GUIQUEUE_TEXT);
	}
	void TextBox::setMultiLine(bool value)
	{
		if(value != multiLine)
		{
			multiLine = value;
			raisePropertyChanged(prop_MultiLine);
		}
	}
	void TextBox::setClearTextOnFocus(bool value)
	{
		if(value != clearTextOnFocus)
		{
			clearTextOnFocus = value;
			raisePropertyChanged(prop_ClearTextOnFocus);
		}
	}
	void TextBox::onServiceProvider(ServiceProvider* oldProvider, ServiceProvider* newProvider)
	{
        if(oldProvider)
		{
			bufferedText = "";
            textBoxFinishedEditingConnection.disconnect();
		}

		Super::onServiceProvider(oldProvider, newProvider);
		onServiceProviderHeartbeatInstance(oldProvider, newProvider);

        if(newProvider)
		{
			bufferedText = getText();

            if( RBX::UserInputService* userInputService = RBX::ServiceProvider::create<RBX::UserInputService>(newProvider) )
                textBoxFinishedEditingConnection = userInputService->textBoxFinishedEditing.connect(bind(&TextBox::externalReleaseFocus,this,_1,_2, _3));
		}
	}

	void TextBox::onAncestorChanged(const AncestorChanged& event)
	{
		Super::onAncestorChanged(event);
		releaseFocus(false, shared_ptr<InputObject>(), event.oldParent );
	}

    GuiResponse TextBox::process(const shared_ptr<InputObject>& event)
    {
		if( this->isDescendantOf( ServiceProvider::find<Workspace>(this) ) ) // textbox on SurfaceGUIs under workspace won't accept user input
			return GuiResponse();

		GuiResponse answer = Super::process(event);

        if (event->getUserInputType() == InputObject::TYPE_FOCUS &&
            event->getUserInputState() == InputObject::INPUT_STATE_END)
        {
            releaseFocus(false, event);
        }

        return answer;
    }

	GuiResponse TextBox::preProcessMouseEvent(const shared_ptr<InputObject>& event)
	{
		bool mouseOver = mouseIsOver(event->get2DPosition());
		
		if (event->isLeftMouseDownEvent())
		{
			if (mouseOver)
			{
				if (!iAmFocus)
				{
					if (shouldFocusFromInput)
						gainFocus(event);
				}
				else
				{
					// Already focused, handle click for caret positioning/selection start
					// even if overshadowed (since preProcess is called for the focus target).
					cursorPos = getCursorPos(event->get2DPosition());
					selectionStart = cursorPos;
					selectionEnd = cursorPos;
					selectionActive = true;
					isMouseSelecting = true;
					setText(bufferedText);
				}
			}
			else if (iAmFocus)
			{
				releaseFocus(false, event);
				return GuiResponse::notSunk();
			}
		}

		// Update selection while dragging the mouse
		if (event->getUserInputType() == InputObject::TYPE_MOUSEMOVEMENT && isMouseSelecting)
		{
			// update selection end to current mouse position
			int pos = getCursorPos(event->get2DPosition());
			selectionEnd = pos;
			cursorPos = pos;
			selectionActive = (selectionStart != selectionEnd);
			setText(bufferedText);
			FASTLOGFORMATTED(FLog::Graphics, "TextBox::preProcessMouseEvent MOUSE_MOVE mouse=(%f,%f) pos=%d sel=(%d,%d)",
							 (double)event->get2DPosition().x, (double)event->get2DPosition().y, pos, selectionStart, selectionEnd);
			// sink movement while selecting
			// Note: We return sunk here so that we don't pass this event down to other elements
			// (like the ScrollingFrame underneath) while we are actively selecting text.
			return GuiResponse::sunkWithTarget(this);
		}

		// Finish selection on mouse up
		if (event->isLeftMouseUpEvent() && isMouseSelecting)
		{
			int pos = getCursorPos(event->get2DPosition());
			selectionEnd = pos;
			cursorPos = pos;
			isMouseSelecting = false;
			FASTLOGFORMATTED(FLog::Graphics, "TextBox::preProcessMouseEvent LEFT_UP mouse=(%f,%f) pos=%d sel=(%d,%d)",
							 (double)event->get2DPosition().x, (double)event->get2DPosition().y, pos, selectionStart, selectionEnd);
			selectionActive = (selectionStart != selectionEnd);
			setText(bufferedText);
		}

		GuiResponse answer = Super::processMouseEventInternal(event, iAmFocus);			//We need to call our classParent to deal with the state table

		return answer;
	}

	GuiResponse TextBox::preProcess(const shared_ptr<InputObject>& event)
    {
		if( this->isDescendantOf( ServiceProvider::find<Workspace>(this) ) ) // textbox on SurfaceGUIs under workspace won't accept user input
			return GuiResponse();

		if (event->getUserInputType() == InputObject::TYPE_FOCUS &&
            event->getUserInputState() == InputObject::INPUT_STATE_END)
        {
            releaseFocus(false, event);
        }

		if (event->isMouseEvent())
		{
			return preProcessMouseEvent(event);
		}
		else if (event->isKeyEvent())
		{
			return processKeyEvent(event);
		}
		else if (event->isTextInputEvent())
		{
			return processTextInputEvent(event);
		}
		else if (event->isTouchEvent())
		{
			return processTouchEvent(event);
		}
		else if (event->isGamepadEvent())
		{
			return processGamepadEvent(event);
		}

		return GuiResponse::notSunk();
    }

	GuiResponse TextBox::processMouseEvent(const shared_ptr<InputObject>& event)
	{
		bool mouseOver = mouseIsOver(event->get2DPosition());

		// Start selection on left mouse down (click)
		if (event->isLeftMouseDownEvent() && shouldFocusFromInput && mouseOver)
		{
			if (!iAmFocus)
			{
				gainFocus(event);
			}

			// initialize selection (start and end at clicked position)
			int pos = getCursorPos(event->get2DPosition());
			FASTLOGFORMATTED(FLog::Graphics, "TextBox::processMouseEvent LEFT_DOWN mouse=(%f,%f) pos=%d",
							 (double)event->get2DPosition().x, (double)event->get2DPosition().y, pos);
			selectionStart = pos;
			selectionEnd = pos;
			selectionActive = true;
			isMouseSelecting = true;

			// move caret to clicked spot
			cursorPos = pos;
			setText(bufferedText);
		}
		// If clicked outside while focused, release focus
		else if (event->isLeftMouseDownEvent() && !mouseOver && iAmFocus)
		{
			FASTLOGFORMATTED(FLog::Graphics, "TextBox::processMouseEvent LEFT_DOWN_OUTSIDE mouse=(%f,%f) focusLost",
							 (double)event->get2DPosition().x, (double)event->get2DPosition().y);
			releaseFocus(false, event);
			isMouseSelecting = false;
			return GuiResponse::notSunk();
		}

		GuiResponse answer = Super::processMouseEvent(event);			//We need to call our classParent to deal with the state table

		// used(this) will set a target and sink this event at the PlayerGui processing level - no other GuiObjects will see it.
		return iAmFocus ? GuiResponse::sunkWithTarget(this) : answer;
	}

	GuiResponse TextBox::processTouchEvent(const shared_ptr<InputObject>& event)
	{
		shouldFocusFromInput = true;

		if ( event->isTouchEvent() && mouseIsOver(event->get2DPosition()) && getActive() )
		{
			if( RBX::ScrollingFrame* scrollFrame = findFirstAncestorOfType<RBX::ScrollingFrame>())
			{
				bool processedInput = scrollFrame->processInputFromDescendant(event);
				shouldFocusFromInput = !scrollFrame->isTouchScrolling();

				if (processedInput)
				{
					return GuiResponse::sunk();
				}
			}
		}

		return Super::processTouchEvent(event);
	}

	void TextBox::selectionToggled(const shared_ptr<InputObject>& event)
	{
		iAmFocus ? releaseFocus(false, event) : captureFocus();
	}

	GuiResponse TextBox::processGamepadEvent(const shared_ptr<InputObject>& event)
	{
		// todo: don't hard code selection button? maybe this is ok
		if (event->getKeyCode() == KC_GAMEPAD_BUTTONA && event->getUserInputState() == InputObject::INPUT_STATE_BEGIN)
		{
			if (isSelectedObject())
			{
				selectionToggled(event);
				return GuiResponse::sunk();
			}
		}

		return Super::processGamepadEvent(event);
	}


	int TextBox::getCursorPos(RBX::Vector2 mousePos)
	{
		// Try normal hit test first.
		int pos = getPosInString(mousePos);
		if (pos >= 0)
			return pos;
		
		// If the normal hit-test failed, try fallback positions vertically:
		// center, top (slightly below top edge), and bottom (slightly above bottom edge).
		// This helps when clicks land between lines or due to rounding/scaling.
		Rect2D rect = getRect2D();

		RBX::Vector2 testPos = mousePos;

		// 1) Try center Y of the text rect
		testPos.y = rect.center().y;
		pos = getPosInString(testPos);
		if (pos >= 0)
			return pos;

		// 2) Try near the top of the rect (small offset to avoid being exactly on edge)
		testPos.y = rect.y0() + 1.0f;
		pos = getPosInString(testPos);
		if (pos >= 0)
			return pos;

		// 3) Try near the bottom of the rect
		testPos.y = rect.y1() - 1.0f;
		pos = getPosInString(testPos);
		if (pos >= 0)
			return pos;

		// Final fallback: place caret at beginning or end based on X position
		if (mousePos.x < rect.center().x)
			pos = 0;
		else
			pos = bufferedText.length();

		return pos;
	}

	void TextBox::setBufferedText(std::string value, int newCursorPosition)
	{
		if (DFFlag::DisplayTextBoxTextWhileTypingMobile)
		{
			bufferedText = value;
			cursorPos = newCursorPosition;
			setText(bufferedText);
		}
	}

	void TextBox::gainFocus(const shared_ptr<InputObject>& event)
	{
		iAmFocus = true;
		showingCursor = false;

		if (event && (event->isLeftMouseUpEvent() || event->isLeftMouseDownEvent()))
		{
			cursorPos = getCursorPos(event->get2DPosition());
		}
		else
		{
			cursorPos = bufferedText.length();
		}

		if(clearTextOnFocus)
		{
			bufferedText = "";
			cursorPos = 0;
		}
		else if (event && (event->isLeftMouseUpEvent() || event->isLeftMouseDownEvent()))
		{
			setText(bufferedText);
		}

		lastSwap = Time::nowFast();
		repeatKeyState.state = RepeatKeyState::STATE_NONE;

		ModelInstance* localCharacter = Network::Players::findLocalCharacter(this);
		Humanoid* localHumanoid = Humanoid::modelIsCharacter(localCharacter);
		if (localHumanoid)
			localHumanoid->setTyping(true);

		if(DataModel* dm = DataModel::get(this))
		{
			dm->setGuiTargetInstance(shared_from(this));
			dm->setSuppressNavKeys(true); // suppress the nav keys
		}

		focusGainedSignal();

        if( RBX::UserInputService* userInputService = RBX::ServiceProvider::create<RBX::UserInputService>(this) )
            userInputService->textBoxGainFocus(shared_from(this));
	}

	void TextBox::captureFocus()
	{
		cursorPos = bufferedText.length();
		iAmFocus = true;
		showingCursor = false;

		if(clearTextOnFocus)
		{
			bufferedText = "";
			cursorPos = 0;
		}

		shouldCaptureFocus = true;

		gainFocus(shared_ptr<RBX::InputObject>());
	}


	void TextBox::onPropertyChanged(const Reflection::PropertyDescriptor& descriptor)
	{
		if (descriptor == prop_Text && strcmp(getText().c_str(), bufferedText.c_str()))
		{
            bufferedText = getText();
		}

		Super::onPropertyChanged(descriptor);
	}


    void TextBox::externalReleaseFocus(const char* externalReleaseText, bool enterPressed, const shared_ptr<InputObject>& inputThatCausedFocusLoss)
    {
        if(!iAmFocus)
            return;

        bufferedText = std::string(externalReleaseText);
        setText(bufferedText);

        ModelInstance* localCharacter = Network::Players::findLocalCharacter(this);
    	Humanoid* localHumanoid = Humanoid::modelIsCharacter(localCharacter);
    	if (localHumanoid)
    		localHumanoid->setTyping(false);

        iAmFocus = false;
    	showingCursor = false;
    	repeatKeyState.state = RepeatKeyState::STATE_NONE;

        // Clear any text selection when focus is released
        clearSelection();

        setFocusLost(enterPressed, inputThatCausedFocusLoss);
    }

	void TextBox::releaseFocusLua()
	{
		releaseFocus(false, shared_ptr<InputObject>());
	}

	bool TextBox::getFocused()
	{
		if (!DFFlag::TextBoxIsFocusedEnabled)
		{
			StandardOut::singleton()->printf(MESSAGE_ERROR, "TextBox:IsFocused() is not yet enabled!");
			return false;
		}
		return iAmFocus;
	}

	void TextBox::releaseFocus(bool enterPressed, const shared_ptr<InputObject>& inputThatCausedFocusLoss, Instance* contextLocalCharacter)
	{
		if(!iAmFocus)
			return;

		ModelInstance* localCharacter = Network::Players::findLocalCharacter( contextLocalCharacter ? contextLocalCharacter : this);
		Humanoid* localHumanoid = Humanoid::modelIsCharacter(localCharacter);
		if (localHumanoid)
			localHumanoid->setTyping(false);

		setText(bufferedText);
		iAmFocus = false;
		showingCursor = false;
		repeatKeyState.state = RepeatKeyState::STATE_NONE;

		// Ensure we clear any selection when focus is released
		clearSelection();

		if(DataModel* dm = DataModel::get(this))
			dm->setSuppressNavKeys(false); // give nav back

	    setFocusLost(enterPressed, inputThatCausedFocusLoss);
	}

    void TextBox::setFocusLost(bool enterPressed, const shared_ptr<InputObject>& inputThatCausedFocusLoss)
    {
        focusLostSignal(enterPressed, inputThatCausedFocusLoss);
        if( RBX::UserInputService* userInputService = RBX::ServiceProvider::create<RBX::UserInputService>(this) )
            userInputService->textBoxReleaseFocus(shared_from(this));
    }

    // Selection helper implementations
    bool TextBox::hasSelection() const
    {
        return selectionActive && (selectionStart != selectionEnd);
    }

    int TextBox::getSelectionStart() const
    {
        if (!selectionActive) return 0;
        return (selectionStart < selectionEnd) ? selectionStart : selectionEnd;
    }

    int TextBox::getSelectionEnd() const
    {
        if (!selectionActive) return 0;
        return (selectionStart < selectionEnd) ? selectionEnd : selectionStart;
    }

    std::string TextBox::getSelectedText() const
    {
        if (!selectionActive) return std::string();
        int s = (selectionStart < selectionEnd) ? selectionStart : selectionEnd;
        int e = (selectionStart < selectionEnd) ? selectionEnd : selectionStart;

        if (s < 0) s = 0;
        if (e > static_cast<int>(bufferedText.length())) e = static_cast<int>(bufferedText.length());

        return bufferedText.substr(static_cast<size_t>(s), static_cast<size_t>(e - s));
    }

    void TextBox::setSelection(int start, int end)
    {
        selectionStart = start;
        selectionEnd = end;
        selectionActive = (start != end);
        cursorPos = selectionEnd;
        setText(bufferedText);
    }

    void TextBox::clearSelection()
    {
        selectionActive = false;
        selectionStart = selectionEnd = 0;
    }

    void TextBox::replaceSelection(const std::string& replacement)
    {
        if (!selectionActive) return;

        int s = (selectionStart < selectionEnd) ? selectionStart : selectionEnd;
        int e = (selectionStart < selectionEnd) ? selectionEnd : selectionStart;

        if (s < 0) s = 0;
        if (e > static_cast<int>(bufferedText.length())) e = static_cast<int>(bufferedText.length());

        bufferedText.erase(static_cast<size_t>(s), static_cast<size_t>(e - s));
        bufferedText.insert(static_cast<size_t>(s), replacement);
        cursorPos = s + static_cast<int>(replacement.length());
        clearSelection();
        setText(bufferedText);
    }

	void TextBox::onHeartbeat(const Heartbeat& event)
	{
		if(iAmFocus)
		{
			switch(repeatKeyState.state)
			{
				case RepeatKeyState::STATE_NONE:
					break;
				case RepeatKeyState::STATE_DEPRESSED:
					if(repeatKeyState.stateWallTime + 0.5 < event.wallTime && !UserInputService::IsUsingNewKeyboardEvents())
					{
						//We've waited half a second, start repeating
						doKey(repeatKeyState.keyType, repeatKeyState.character);
						repeatKeyState.state = RepeatKeyState::STATE_REPEATING;
						repeatKeyState.stateWallTime = event.wallTime;
					}
					break;
				case RepeatKeyState::STATE_REPEATING:
					if (!UserInputService::IsUsingNewKeyboardEvents())
					{
						while(repeatKeyState.stateWallTime + (1/20.0) < event.wallTime)
						{
							doKey(repeatKeyState.keyType, repeatKeyState.character);
							repeatKeyState.stateWallTime += 1/20.0;
						}
					}
					break;
			}
		}
	}

	static size_t prevUtf8CharStart(const std::string& s, size_t pos)
	{
		if (s.empty() || pos == 0) return 0;
		size_t i = pos;
		if (i > s.size()) i = s.size();
		if (i > 0) --i;
		while (i > 0 && (static_cast<unsigned char>(s[i]) & 0xC0) == 0x80)
			--i;
		return i;
	}

	static size_t nextUtf8CharEnd(const std::string& s, size_t pos)
	{
		if (pos >= s.size()) return s.size();
		unsigned char c = static_cast<unsigned char>(s[pos]);
		size_t len = 1;
		if ((c & 0x80) == 0) len = 1;
		else if ((c & 0xE0) == 0xC0) len = 2;
		else if ((c & 0xF0) == 0xE0) len = 3;
		else if ((c & 0xF8) == 0xF0) len = 4;
		size_t end = pos + len;
		if (end > s.size()) end = s.size();
		return end;
	}

	void TextBox::doKey(RepeatKeyState::KeyType keyType, char key)
	{
		doKey(keyType, std::string(1, key));
	}
	void TextBox::doKey(RepeatKeyState::KeyType keyType, std::string key)
	{
		std::string bufferedTextBefore = bufferedText;

		switch(keyType)
		{
		case RepeatKeyState::TYPE_BACKSPACE:
			{
				// If there is a selection, delete it first
				if (selectionActive && selectionStart != selectionEnd)
				{
					int s = std::min(selectionStart, selectionEnd);
					int e = std::max(selectionStart, selectionEnd);
					if (s < 0) s = 0;
					if (e > static_cast<int>(bufferedText.length())) e = static_cast<int>(bufferedText.length());
					bufferedText.erase(static_cast<size_t>(s), static_cast<size_t>(e - s));
					cursorPos = s;
					clearSelection();
					break;
				}

				if(!key.empty() && key[0] != 0)
				{
					//Delete word (UTF-8 aware)
					if (!bufferedText.empty() && cursorPos > 0)
					{
						size_t start = static_cast<size_t>(cursorPos);

						// Move past any whitespace backwards
						while (start > 0)
						{
							size_t prev = prevUtf8CharStart(bufferedText, start);
							unsigned char ch = static_cast<unsigned char>(bufferedText[prev]);
							if (Typesetter::isCharWhitespace(static_cast<char>(ch)) || ch == '\n')
							{
								start = prev;
							}
							else
							{
								break;
							}
						}

						// Move until we hit our first whitespace
						while (start > 0)
						{
							size_t prev = prevUtf8CharStart(bufferedText, start);
							unsigned char ch = static_cast<unsigned char>(bufferedText[prev]);
							if (Typesetter::isCharWhitespace(static_cast<char>(ch)))
							{
								break;
							}
							else
							{
								start = prev;
							}
						}

						// Erase range [start, cursorPos)
						bufferedText.erase(start, static_cast<size_t>(cursorPos) - start);
						cursorPos = static_cast<int>(start);
					}
				}
				else if ( (bufferedText.size() > 0) && (cursorPos > 0) && (static_cast<size_t>(cursorPos) <= bufferedText.length()) ) // delete single UTF-8 char
				{
					size_t prev = prevUtf8CharStart(bufferedText, static_cast<size_t>(cursorPos));
					bufferedText.erase(prev, static_cast<size_t>(cursorPos) - prev);
					cursorPos = static_cast<int>(prev);
				}
				break;
			}
		case RepeatKeyState::TYPE_DELETE:
			{
				// If there is a selection, delete it first
				if (selectionActive && selectionStart != selectionEnd)
				{
					int s = std::min(selectionStart, selectionEnd);
					int e = std::max(selectionStart, selectionEnd);
					if (s < 0) s = 0;
					if (e > static_cast<int>(bufferedText.length())) e = static_cast<int>(bufferedText.length());
					bufferedText.erase(static_cast<size_t>(s), static_cast<size_t>(e - s));
					cursorPos = s;
					clearSelection();
					break;
				}

				if ( (bufferedText.size() > 0) && (cursorPos >= 0) && (static_cast<size_t>(cursorPos) < bufferedText.length()) )
				{
					size_t nextEnd = nextUtf8CharEnd(bufferedText, static_cast<size_t>(cursorPos));
					bufferedText.erase(static_cast<size_t>(cursorPos), nextEnd - static_cast<size_t>(cursorPos));
				}
			}
			break;
		case RepeatKeyState::TYPE_CHARACTER:
			{
				// If there is a selection, the typed character replaces it
				if (selectionActive && selectionStart != selectionEnd)
				{
					int s = std::min(selectionStart, selectionEnd);
					int e = std::max(selectionStart, selectionEnd);
					if (s < 0) s = 0;
					if (e > static_cast<int>(bufferedText.length())) e = static_cast<int>(bufferedText.length());
					bufferedText.erase(static_cast<size_t>(s), static_cast<size_t>(e - s));
					cursorPos = s;
					clearSelection();
				}

				// Accept unicode input: either Typesetter supports it, or string contains non-ASCII bytes (UTF-8)
				bool supported = Typesetter::isStringSupported(key);
				if (!supported)
				{
					for (unsigned char c : key)
					{
						if (c & 0x80)
						{
							supported = true;
							break;
						}
					}
				}
				if (supported)
				{
					if( (cursorPos >= 0) && (static_cast<size_t>(cursorPos) <= bufferedText.length()) )
						bufferedText.insert(static_cast<size_t>(cursorPos), key);
					cursorPos += static_cast<int>(key.length());
				}
			}
			break;
		case RepeatKeyState::TYPE_LEFTARROW:
			if(cursorPos > 0)
				cursorPos = static_cast<int>(prevUtf8CharStart(bufferedText, static_cast<size_t>(cursorPos)));
			// If selection-repeat mode is active, update the selection end
			if (selectionRepeatActive)
			{
				selectionEnd = cursorPos;
				selectionActive = (selectionStart != selectionEnd);
			}
			break;
		case RepeatKeyState::TYPE_RIGHTARROW:
			if(static_cast<size_t>(cursorPos) < bufferedText.length())
				cursorPos = static_cast<int>(nextUtf8CharEnd(bufferedText, static_cast<size_t>(cursorPos)));
			// If selection-repeat mode is active, update the selection end
			if (selectionRepeatActive)
			{
				selectionEnd = cursorPos;
				selectionActive = (selectionStart != selectionEnd);
			}
			break;
		case RepeatKeyState::TYPE_PASTE:
			if(repeatKeyState.state == RepeatKeyState::STATE_NONE)
			{
				std::string pasteText = "";
				if(RBX::UserInputService* inputService = RBX::ServiceProvider::find<RBX::UserInputService>(this))
					pasteText = inputService->getPasteText();

				if (pasteText.length() > 0)
				{
					if (selectionActive && selectionStart != selectionEnd)
					{
						int s = std::min(selectionStart, selectionEnd);
						int e = std::max(selectionStart, selectionEnd);
						if (s < 0) s = 0;
						if (e > static_cast<int>(bufferedText.length())) e = static_cast<int>(bufferedText.length());
						bufferedText.erase(static_cast<size_t>(s), static_cast<size_t>(e - s));
						bufferedText.insert(static_cast<size_t>(s), pasteText);
						cursorPos = s + static_cast<int>(pasteText.length());
						clearSelection();
					}
					else if( (cursorPos >= 0) && (static_cast<size_t>(cursorPos) <= bufferedText.length()) )
					{
						bufferedText.insert(static_cast<size_t>(cursorPos),pasteText);
						cursorPos += static_cast<int>(pasteText.length());
					}
				}
			}
			break;
		}

		if (bufferedTextBefore != bufferedText)
			setText(bufferedText);
	}

	void TextBox::keyUp(RepeatKeyState::KeyType keyType, RBX::KeyCode keyCode, char key)
	{
		if( repeatKeyState.state == RepeatKeyState::STATE_DEPRESSED || (repeatKeyState.state == RepeatKeyState::STATE_REPEATING && !UserInputService::IsUsingNewKeyboardEvents()) )
			if(keyType == repeatKeyState.keyType && keyCode == repeatKeyState.keyCode) //if last key pressed was ours, stop it from repeating further
				repeatKeyState.state = RepeatKeyState::STATE_NONE;
	}

	void TextBox::textInput(RepeatKeyState::KeyType keyType, std::string textInput)
	{
		if (iAmFocus)
		{
			doKey(keyType, textInput);
		}
	}

	void TextBox::keyDown(RepeatKeyState::KeyType keyType, RBX::KeyCode keyCode, char key)
	{
		if(repeatKeyState.state == RepeatKeyState::STATE_NONE || repeatKeyState.state == RepeatKeyState::STATE_DEPRESSED)
		{
			doKey(keyType, key);

			repeatKeyState.keyCode = keyCode;
			repeatKeyState.keyType = keyType;
			repeatKeyState.character = key;
			if(keyType != RepeatKeyState::TYPE_PASTE) // we don't repeat paste commands
				repeatKeyState.state = RepeatKeyState::STATE_DEPRESSED;
			if(RunService* runService = ServiceProvider::find<RunService>(this))
				repeatKeyState.stateWallTime = runService->wallTime();
		}
		else
		{
			//We are repeating, so ignore the new key press
		}
	}
	GuiResponse TextBox::processTextInputEvent(const shared_ptr<InputObject>& event)
	{
		RBXASSERT(event->isTextInputEvent());

		textInput(RepeatKeyState::TYPE_CHARACTER, event->getInputText());

		return iAmFocus ? GuiResponse::sunkWithTarget(this) : GuiResponse::notSunk();
	}
	GuiResponse TextBox::processKeyEvent(const shared_ptr<InputObject>& event)
	{
		RBXASSERT(event->isKeyEvent());

		if(shouldCaptureFocus)
		{
			gainFocus(event);
			shouldCaptureFocus = false;
		}

		std::string bufferedTextBefore = bufferedText;
		bool wasFocusAtStartOfProcessKey = iAmFocus;


		GuiResponse answer = Super::processKeyEvent(event);
		if (iAmFocus)
		{
			if (event->isKeyDownEvent())
			{
				if( event->isEscapeKey() )
					releaseFocus(false, event);
				else if(event->isCarriageReturnKey())
				{
					if (isSelectedObject())
						releaseFocus(false, event);
					else if(getMultiLine())
						keyDown(RepeatKeyState::TYPE_CHARACTER, event->getKeyCode(), '\n');
					else
						releaseFocus(true, event);
				}
				else if (event->isLeftArrowKey())
				{
                    // Prefer event-local modifier flags so SHIFT+Left works even if modifier order varies.
                    bool eventShiftDown = event->isKeyPressedWithShiftEvent() || ((event->getModCodes() & (KC_KMOD_LSHIFT | KC_KMOD_RSHIFT)) != 0);

                    if (eventShiftDown)
                    {
                        if (!selectionActive)
                        {
                            selectionStart = cursorPos;
                            selectionEnd = cursorPos;
                            selectionActive = true;
                        }
                        selectionRepeatActive = true;
                    }
                    else
                    {
                        selectionRepeatActive = false;
                        clearSelection();
                    }

                    keyDown(RepeatKeyState::TYPE_LEFTARROW, event->getKeyCode(), 'l');
                }
				else if (event->isRightArrowKey())
				{
                    // Prefer event-local modifier flags so SHIFT+Right works even if modifier order varies.
                    bool eventShiftDown = event->isKeyPressedWithShiftEvent() || ((event->getModCodes() & (KC_KMOD_LSHIFT | KC_KMOD_RSHIFT)) != 0);

                    if (eventShiftDown)
                    {
                        if (!selectionActive)
                        {
                            selectionStart = cursorPos;
                            selectionEnd = cursorPos;
                            selectionActive = true;
                        }
                        selectionRepeatActive = true;
                    }
                    else
                    {
                        selectionRepeatActive = false;
                        clearSelection();
                    }

                    keyDown(RepeatKeyState::TYPE_RIGHTARROW, event->getKeyCode(), 'r');
                }
				else if (event->isClearKey())
					bufferedText = "";
				else if (event->isDeleteKey())
					keyDown(RepeatKeyState::TYPE_DELETE, event->getKeyCode(), '\0');
				else if (event->isBackspaceKey())
                {
                    // Use event-local ctrl detection so Ctrl+Backspace works regardless of modifier order.
                    keyDown(RepeatKeyState::TYPE_BACKSPACE, event->getKeyCode(), event->isCtrlEvent() ? '\1' : '\0');
                }
				else if (event->isTextCharacterKey() || (UserInputService::IsUsingNewKeyboardEvents() && event->getUserInputType() == InputObject::TYPE_KEYBOARD))
				{
                    // Handle copy/cut/paste for both legacy text events (modifiedKey != 0)
                    // and for platforms using the new keyboard events (SDL/Win/Mac).
                    // Note: actual character input on those platforms arrives via TEXTINPUT events,
                    // so only perform TYPE_CHARACTER when we actually have a character (modifiedKey).
                    if (isCopyCommand(event))
                    {
                        std::string toCopy = hasSelection() ? getSelectedText() : bufferedText;
                        // write to OS clipboard
                        if (RBX::UserInputService* inputService = RBX::ServiceProvider::find<RBX::UserInputService>(this))
                            inputService->setClipboardText(toCopy);
                    }
                    else if (isCutCommand(event))
                    {
                        std::string toCopy = hasSelection() ? getSelectedText() : bufferedText;
                        if (RBX::UserInputService* inputService = RBX::ServiceProvider::find<RBX::UserInputService>(this))
                            inputService->setClipboardText(toCopy);
                        if (hasSelection())
                            replaceSelection("");
                    }
                    else if (isPasteCommand(event))
                    {
                        keyDown(RepeatKeyState::TYPE_PASTE, event->getKeyCode(), 'v');
                    }
                    else if (event->isTextCharacterKey())
                    {
                        // Only synthesize character input for events that carry a character.
                        keyDown(RepeatKeyState::TYPE_CHARACTER, event->getKeyCode(), event->modifiedKey);
                    }
				}
			}
			else if(event->isKeyUpEvent()) // key up
			{
				if(event->isCarriageReturnKey())
				{
					if(getMultiLine())
						keyUp(RepeatKeyState::TYPE_CHARACTER, event->getKeyCode(), '\n');
				}
				else if(event->isDeleteKey())
					keyUp(RepeatKeyState::TYPE_DELETE, event->getKeyCode(), '\0');
				else if(event->isBackspaceKey())
					keyUp(RepeatKeyState::TYPE_BACKSPACE, event->getKeyCode(), '\0');
				else if (event->isLeftArrowKey())
				{
					keyUp(RepeatKeyState::TYPE_LEFTARROW, event->getKeyCode(), 'l');
					selectionRepeatActive = false;
				}
				else if (event->isRightArrowKey())
				{
					keyUp(RepeatKeyState::TYPE_RIGHTARROW, event->getKeyCode(), 'r');
					selectionRepeatActive = false;
				}
				else if(event->isTextCharacterKey())
				{
					char found = event->modifiedKey;
					keyUp(RepeatKeyState::TYPE_CHARACTER, event->getKeyCode(), found);
				}
			}
			//Turn off repeating characters if shift is pressed or unpressed
			if((event->getKeyCode() == KC_LSHIFT || event->getKeyCode() == KC_RSHIFT) &&
			   (repeatKeyState.state == RepeatKeyState::STATE_DEPRESSED || (repeatKeyState.state == RepeatKeyState::STATE_REPEATING && !UserInputService::IsUsingNewKeyboardEvents())))
			{
				repeatKeyState.state = RepeatKeyState::STATE_NONE;
				selectionRepeatActive = false;
			}
		}
		else if (!iAmFocus && event->isKeyDownEvent() && isSelectedObject() && event->getKeyCode() == KC_RETURN)
			captureFocus();

		if (bufferedTextBefore != bufferedText)
			setText(bufferedText);

		return wasFocusAtStartOfProcessKey ? GuiResponse::sunkWithTarget(this) : answer;
	}

    // Clipboard operations should be performed via UserInputService::setClipboardText
    // (Use the service from the caller: find<RBX::UserInputService>(this)->setClipboardText(...))

    bool TextBox::isPasteCommand(const shared_ptr<InputObject>& event)
    {
        if(event->getKeyCode() == RBX::KC_V)
        {
            if(RBX::UserInputService* inputService = RBX::ServiceProvider::find<RBX::UserInputService>(this))
            {
                std::vector<RBX::ModCode> modCodes = inputService->getCommandModCodes();
                // Combine legacy 'mod' and new 'modCodes' into a single mask so commands work
                // regardless of which keyboard event model is in use.
                unsigned int eventMods = static_cast<unsigned int>(event->mod) | event->getModCodes();
                for(std::vector<RBX::ModCode>::iterator it = modCodes.begin(); it != modCodes.end(); ++it)
                {
                    if(DFFlag::PasteWithCapsLockOn ? (eventMods & *(it)) : (eventMods == *(it)))
                        return true;
                }
    		}
    	}

        return false;
    }

    bool TextBox::isCopyCommand(const shared_ptr<InputObject>& event)
    {
        if(event->getKeyCode() == RBX::KC_C)
        {
            if(RBX::UserInputService* inputService = RBX::ServiceProvider::find<RBX::UserInputService>(this))
            {
                std::vector<RBX::ModCode> modCodes = inputService->getCommandModCodes();
                // Combine legacy 'mod' and new 'modCodes' into a single mask so commands work
                // regardless of which keyboard event model is in use.
                unsigned int eventMods = static_cast<unsigned int>(event->mod) | event->getModCodes();
                for(std::vector<RBX::ModCode>::iterator it = modCodes.begin(); it != modCodes.end(); ++it)
                {
                    if(DFFlag::PasteWithCapsLockOn ? (eventMods & *(it)) : (eventMods == *(it)))
                        return true;
                }
            }
        }

        return false;
    }

    bool TextBox::isCutCommand(const shared_ptr<InputObject>& event)
    {
        if(event->getKeyCode() == RBX::KC_X)
        {
            if(RBX::UserInputService* inputService = RBX::ServiceProvider::find<RBX::UserInputService>(this))
            {
                std::vector<RBX::ModCode> modCodes = inputService->getCommandModCodes();
                // Combine legacy 'mod' and new 'modCodes' into a single mask so commands work
                // regardless of which keyboard event model is in use.
                unsigned int eventMods = static_cast<unsigned int>(event->mod) | event->getModCodes();
                for(std::vector<RBX::ModCode>::iterator it = modCodes.begin(); it != modCodes.end(); ++it)
                {
                    if(DFFlag::PasteWithCapsLockOn ? (eventMods & *(it)) : (eventMods == *(it)))
                        return true;
                }
            }
        }

        return false;
    }

	std::string TextBox::getTextWithCursor()
	{
		RBXASSERT(cursorPos >= 0);

		// make sure we are in range
		cursorPos = std::min<int>(std::max<int>(0, cursorPos), bufferedText.length());

		std::string textWithCursor = bufferedText;

        textWithCursor.insert(cursorPos,"\1");

		return textWithCursor;
	}
	std::string TextBox::getTextWithBlankCursor()
	{
		RBXASSERT(cursorPos >= 0);

		// make sure we are in range
		cursorPos = std::min<int>(std::max<int>(0, cursorPos), bufferedText.length());

		std::string textWithCursor = bufferedText;

		return textWithCursor;
	}

	void TextBox::render2d(Adorn* adorn)
	{
		if (iAmFocus)
		{
			Time now = Time::nowFast();
			double delta = (now - lastSwap).seconds();
			if (delta > CURSOR_BLINK_RATE_SECONDS)
			{
				showingCursor = !showingCursor;
				lastSwap = now;
			}

			// Draw selection highlight for any text (single-line, multi-line, or wrapped)
			if (hasSelection())
			{
				// Compute layout box and font sizing similar to GuiObject::render2dTextImpl
				Rect2D rect;
				render2dImpl(adorn, getRenderBackgroundColor4(), rect);

				bool autoScale = getTextScale() && adorn->useFontSmoothScalling();
				float _fontSize = 0;
				if (adorn->useFontSmoothScalling())
					_fontSize = getFontSizeScale(getTextScale(), getTextWrap(), getFontSize(), rect);
				else
					_fontSize = getTextScale() ? getScaledFontSize(rect, getText(), getFont(), getTextWrap(), getFontSize()) : getFontSizeScale(getTextScale(), getTextWrap(), getFontSize(), rect);

				Vector2 wh = getTextWrap() ? rect.wh() : Vector2::zero();

				std::string rawText = getText();

				Rect2D clipRect = Rect2D::xyxy(-1, -1, -1, -1);
				if (getClipping())
				{
					GuiObject* clippingObject = firstAncestorClipping();
					if (clippingObject)
						clipRect = clippingObject->getClippedRect().intersect(rect);
					else
						clipRect = rect;
				}

				int s = getSelectionStart();
				int e = getSelectionEnd();
				if (s < 0) s = 0;
				if (e > static_cast<int>(rawText.length())) e = static_cast<int>(rawText.length());

				if (s != e)
				{
					Color4 highlightColor = Color4(0.1f, 0.6f, 1.0f, 0.4f);

					// Construct lines: explicit newlines (multi-line) or greedy wrap if needed
					std::vector<std::string> lines;
					std::vector<int> lineStartIndices;

					if (getMultiLine())
					{
						std::string tmp;
						int currentIndex = 0;
						for (size_t i = 0; i < rawText.size(); ++i)
						{
							if (rawText[i] == '\n')
							{
								lines.push_back(tmp);
								lineStartIndices.push_back(currentIndex);
								currentIndex += static_cast<int>(tmp.size());
								++currentIndex; // account for '\n'
								tmp.clear();
							}
							else
							{
								tmp.push_back(rawText[i]);
							}
						}
						// final line
						lines.push_back(tmp);
						lineStartIndices.push_back(currentIndex);
					}
					else if (getTextWrap() && wh.x > 1.0f)
					{
						int n = static_cast<int>(rawText.size());
						int idx = 0;
						while (idx < n)
						{
							int lineStart = idx;
							int pos = idx;
							int lastBreak = -1;
							int lastGoodPos = idx;

							while (pos <= n)
							{
								std::string substr = rawText.substr(lineStart, pos - lineStart);
								Vector2 substrSize = adorn->drawFont2D(substr, Vector2::zero(), _fontSize, autoScale, Color4(0,0,0,0), Color4(0,0,0,0),
																	TextService::ToTextFont(getFont()), TextService::ToTextXAlign(TextService::XALIGNMENT_LEFT), TextService::ToTextYAlign(TextService::YALIGNMENT_TOP), wh, clipRect);
								if (substrSize.x > wh.x && pos > lineStart)
									break;

								if (pos < n && (rawText[pos] == ' ' || rawText[pos] == '\t'))
									lastBreak = pos;

								lastGoodPos = pos;
								++pos;
							}

							int breakPos = lastGoodPos;
							if (breakPos == lineStart && lineStart < n)
								breakPos = lineStart + 1;
							else if (breakPos > lineStart && breakPos - lineStart > 0)
							{
								// prefer breaking at whitespace
								if (lastBreak > lineStart)
									breakPos = lastBreak;
							}

							std::string line = rawText.substr(lineStart, breakPos - lineStart);
							lines.push_back(line);
							lineStartIndices.push_back(lineStart);
							idx = breakPos;
							// skip initial whitespace on next line
							while (idx < n && (rawText[idx] == ' ' || rawText[idx] == '\t'))
								++idx;
						}
					}
					else
					{
						// single-line (no wrapping/multiline)
						lines.push_back(rawText);
						lineStartIndices.push_back(0);
					}

					// Measure line sizes and compute total height
					std::vector<Vector2> lineSizes;
					float totalHeight = 0.0f;
					for (size_t i = 0; i < lines.size(); ++i)
					{
						Vector2 lineSize = adorn->drawFont2D(lines[i], Vector2::zero(), _fontSize, autoScale, Color4(0,0,0,0), Color4(0,0,0,0),
															 TextService::ToTextFont(getFont()), TextService::ToTextXAlign(TextService::XALIGNMENT_LEFT), TextService::ToTextYAlign(TextService::YALIGNMENT_TOP), wh, clipRect);
						if (lineSize.y == 0.0f)
						{
							// fallback to a single space to get line height
							Vector2 spacer = adorn->drawFont2D(std::string(" "), Vector2::zero(), _fontSize, autoScale, Color4(0,0,0,0), Color4(0,0,0,0),
															   TextService::ToTextFont(getFont()), TextService::ToTextXAlign(TextService::XALIGNMENT_LEFT), TextService::ToTextYAlign(TextService::YALIGNMENT_TOP), wh, clipRect);
							lineSize.y = spacer.y;
						}
						lineSizes.push_back(lineSize);
						totalHeight += lineSize.y;
					}

					// vertical alignment for block
					float baseY;
					switch(getYAlignment())
					{
						case TextService::YALIGNMENT_TOP: baseY = rect.y0(); break;
						case TextService::YALIGNMENT_BOTTOM: baseY = rect.y1() - totalHeight; break;
						case TextService::YALIGNMENT_CENTER: baseY = rect.center().y - totalHeight * 0.5f; break;
						default: baseY = rect.y0(); break;
					}

					// draw selection per-line
					float yOffset = 0.0f;
					for (size_t i = 0; i < lines.size(); ++i)
					{
						const std::string& line = lines[i];
						const Vector2& lineSize = lineSizes[i];

						float baseX;
						switch(getXAlignment())
						{
							case TextService::XALIGNMENT_LEFT: baseX = rect.x0(); break;
							case TextService::XALIGNMENT_RIGHT: baseX = rect.x1() - lineSize.x; break;
							case TextService::XALIGNMENT_CENTER: baseX = rect.center().x - lineSize.x * 0.5f; break;
							default: baseX = rect.x0(); break;
						}

						int lineStart = lineStartIndices[i];
						int lineLen = static_cast<int>(line.length());

						int selStartInLine = std::max(0, std::min(lineLen, s - lineStart));
						int selEndInLine = std::max(0, std::min(lineLen, e - lineStart));

						if (selStartInLine < selEndInLine)
						{
							std::string prefix = line.substr(0, selStartInLine);
							std::string selSub = line.substr(selStartInLine, selEndInLine - selStartInLine);

							Vector2 prefixSize = adorn->drawFont2D(prefix, Vector2::zero(), _fontSize, autoScale, Color4(0,0,0,0), Color4(0,0,0,0),
																 TextService::ToTextFont(getFont()), TextService::ToTextXAlign(TextService::XALIGNMENT_LEFT), TextService::ToTextYAlign(TextService::YALIGNMENT_TOP), wh, clipRect);
							Vector2 selSize = adorn->drawFont2D(selSub, Vector2::zero(), _fontSize, autoScale, Color4(0,0,0,0), Color4(0,0,0,0),
																TextService::ToTextFont(getFont()), TextService::ToTextXAlign(TextService::XALIGNMENT_LEFT), TextService::ToTextYAlign(TextService::YALIGNMENT_TOP), wh, clipRect);

							Rect2D selRect = Rect2D::xywh(baseX + prefixSize.x, baseY + yOffset, selSize.x, lineSize.y);
							if (getClipping())
								selRect = selRect.intersect(clipRect);

							adorn->rect2d(selRect, highlightColor);
						}

						yOffset += lineSize.y;
					}
				}
			}

			// Finally draw the text (cursor will be painted via getTextWithCursor)
			render2dTextImpl(adorn, getRenderBackgroundColor4(), showingCursor ? getTextWithCursor() : getTextWithBlankCursor(), getFont(), getFontSize(), getRenderTextColor4(), getRenderTextStrokeColor4(), getTextWrap(), getTextScale(), getXAlignment(), getYAlignment());
		}
		else
		{
			// Draw selection highlight for any text (single-line, multi-line, or wrapped)
			if (hasSelection())
			{
				// Compute layout box and font sizing similar to GuiObject::render2dTextImpl
				Rect2D rect;
				render2dImpl(adorn, getRenderBackgroundColor4(), rect);

				bool autoScale = getTextScale() && adorn->useFontSmoothScalling();
				float _fontSize = 0;
				if (adorn->useFontSmoothScalling())
					_fontSize = getFontSizeScale(getTextScale(), getTextWrap(), getFontSize(), rect);
				else
					_fontSize = getTextScale() ? getScaledFontSize(rect, getText(), getFont(), getTextWrap(), getFontSize()) : getFontSizeScale(getTextScale(), getTextWrap(), getFontSize(), rect);

				Vector2 wh = getTextWrap() ? rect.wh() : Vector2::zero();

				std::string rawText = getText();

				Rect2D clipRect = Rect2D::xyxy(-1, -1, -1, -1);
				if (getClipping())
				{
					GuiObject* clippingObject = firstAncestorClipping();
					if (clippingObject)
						clipRect = clippingObject->getClippedRect().intersect(rect);
					else
						clipRect = rect;
				}

				int s = getSelectionStart();
				int e = getSelectionEnd();
				if (s < 0) s = 0;
				if (e > static_cast<int>(rawText.length())) e = static_cast<int>(rawText.length());

				if (s != e)
				{
					Color4 highlightColor = Color4(0.1f, 0.6f, 1.0f, 0.4f);

					// Construct lines: explicit newlines (multi-line) or greedy wrap if needed
					std::vector<std::string> lines;
					std::vector<int> lineStartIndices;

					if (getMultiLine())
					{
						std::string tmp;
						int currentIndex = 0;
						for (size_t i = 0; i < rawText.size(); ++i)
						{
							if (rawText[i] == '\n')
							{
								lines.push_back(tmp);
								lineStartIndices.push_back(currentIndex);
								currentIndex += static_cast<int>(tmp.size());
								++currentIndex; // account for '\n'
								tmp.clear();
							}
							else
							{
								tmp.push_back(rawText[i]);
							}
						}
						// final line
						lines.push_back(tmp);
						lineStartIndices.push_back(currentIndex);
					}
					else if (getTextWrap() && wh.x > 1.0f)
					{
						int n = static_cast<int>(rawText.size());
						int idx = 0;
						while (idx < n)
						{
							int lineStart = idx;
							int pos = idx;
							int lastBreak = -1;
							int lastGoodPos = idx;

							while (pos <= n)
							{
								std::string substr = rawText.substr(lineStart, pos - lineStart);
								Vector2 substrSize = adorn->drawFont2D(substr, Vector2::zero(), _fontSize, autoScale, Color4(0,0,0,0), Color4(0,0,0,0),
																	TextService::ToTextFont(getFont()), TextService::ToTextXAlign(TextService::XALIGNMENT_LEFT), TextService::ToTextYAlign(TextService::YALIGNMENT_TOP), wh, clipRect);
								if (substrSize.x > wh.x && pos > lineStart)
									break;

								if (pos < n && (rawText[pos] == ' ' || rawText[pos] == '\t'))
									lastBreak = pos;

								lastGoodPos = pos;
								++pos;
							}

							int breakPos = lastGoodPos;
							if (breakPos == lineStart && lineStart < n)
								breakPos = lineStart + 1;
							else if (breakPos > lineStart && breakPos - lineStart > 0)
							{
								// prefer breaking at whitespace
								if (lastBreak > lineStart)
									breakPos = lastBreak;
							}

							std::string line = rawText.substr(lineStart, breakPos - lineStart);
							lines.push_back(line);
							lineStartIndices.push_back(lineStart);
							idx = breakPos;
							// skip initial whitespace on next line
							while (idx < n && (rawText[idx] == ' ' || rawText[idx] == '\t'))
								++idx;
						}
					}
					else
					{
						// single-line (no wrapping/multiline)
						lines.push_back(rawText);
						lineStartIndices.push_back(0);
					}

					// Measure line sizes and compute total height
					std::vector<Vector2> lineSizes;
					float totalHeight = 0.0f;
					for (size_t i = 0; i < lines.size(); ++i)
					{
						Vector2 lineSize = adorn->drawFont2D(lines[i], Vector2::zero(), _fontSize, autoScale, Color4(0,0,0,0), Color4(0,0,0,0),
															 TextService::ToTextFont(getFont()), TextService::ToTextXAlign(TextService::XALIGNMENT_LEFT), TextService::ToTextYAlign(TextService::YALIGNMENT_TOP), wh, clipRect);
						if (lineSize.y == 0.0f)
						{
							// fallback to a single space to get line height
							Vector2 spacer = adorn->drawFont2D(std::string(" "), Vector2::zero(), _fontSize, autoScale, Color4(0,0,0,0), Color4(0,0,0,0),
															   TextService::ToTextFont(getFont()), TextService::ToTextXAlign(TextService::XALIGNMENT_LEFT), TextService::ToTextYAlign(TextService::YALIGNMENT_TOP), wh, clipRect);
							lineSize.y = spacer.y;
						}
						lineSizes.push_back(lineSize);
						totalHeight += lineSize.y;
					}

					// vertical alignment for block
					float baseY;
					switch(getYAlignment())
					{
						case TextService::YALIGNMENT_TOP: baseY = rect.y0(); break;
						case TextService::YALIGNMENT_BOTTOM: baseY = rect.y1() - totalHeight; break;
						case TextService::YALIGNMENT_CENTER: baseY = rect.center().y - totalHeight * 0.5f; break;
						default: baseY = rect.y0(); break;
					}

					// draw selection per-line
					float yOffset = 0.0f;
					for (size_t i = 0; i < lines.size(); ++i)
					{
						const std::string& line = lines[i];
						const Vector2& lineSize = lineSizes[i];

						float baseX;
						switch(getXAlignment())
						{
							case TextService::XALIGNMENT_LEFT: baseX = rect.x0(); break;
							case TextService::XALIGNMENT_RIGHT: baseX = rect.x1() - lineSize.x; break;
							case TextService::XALIGNMENT_CENTER: baseX = rect.center().x - lineSize.x * 0.5f; break;
							default: baseX = rect.x0(); break;
						}

						int lineStart = lineStartIndices[i];
						int lineLen = static_cast<int>(line.length());

						int selStartInLine = std::max(0, std::min(lineLen, s - lineStart));
						int selEndInLine = std::max(0, std::min(lineLen, e - lineStart));

						if (selStartInLine < selEndInLine)
						{
							std::string prefix = line.substr(0, selStartInLine);
							std::string selSub = line.substr(selStartInLine, selEndInLine - selStartInLine);

							Vector2 prefixSize = adorn->drawFont2D(prefix, Vector2::zero(), _fontSize, autoScale, Color4(0,0,0,0), Color4(0,0,0,0),
																 TextService::ToTextFont(getFont()), TextService::ToTextXAlign(TextService::XALIGNMENT_LEFT), TextService::ToTextYAlign(TextService::YALIGNMENT_TOP), wh, clipRect);
							Vector2 selSize = adorn->drawFont2D(selSub, Vector2::zero(), _fontSize, autoScale, Color4(0,0,0,0), Color4(0,0,0,0),
																TextService::ToTextFont(getFont()), TextService::ToTextXAlign(TextService::XALIGNMENT_LEFT), TextService::ToTextYAlign(TextService::YALIGNMENT_TOP), wh, clipRect);

							Rect2D selRect = Rect2D::xywh(baseX + prefixSize.x, baseY + yOffset, selSize.x, lineSize.y);
							if (getClipping())
								selRect = selRect.intersect(clipRect);

							adorn->rect2d(selRect, highlightColor);
						}

						yOffset += lineSize.y;
					}
				}
			}

			render2dTextImpl(adorn, getRenderBackgroundColor4(), getText(), getFont(), getFontSize(), getRenderTextColor4(), getRenderTextStrokeColor4(), getTextWrap(), getTextScale(), getXAlignment(), getYAlignment());
		}

		renderStudioSelectionBox(adorn);
	}
}
