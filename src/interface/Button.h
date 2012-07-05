/*
 * Button.h
 *
 *  Created on: Jan 8, 2012
 *      Author: Simon
 */

#ifndef BUTTON_H_
#define BUTTON_H_

#include <string>
#include "Misc.h"
#include "Component.h"
#include "Colour.h"

namespace ui
{
class Button;
class ButtonAction
{
public:
	virtual void ActionCallback(ui::Button * sender) {}
	virtual void MouseEnterCallback(ui::Button * sender) {}
	virtual ~ButtonAction() {}
};

#if defined(WIN32) && !defined(__GNUC__)
class __declspec(dllexport) Button : public Component
#else
class Button : public Component
#endif
{
public:
	Button(Point position = Point(0, 0), Point size = Point(0, 0), std::string buttonText = "");
	virtual ~Button();

	bool Toggleable;
	bool Enabled;

	virtual void OnMouseClick(int x, int y, unsigned int button);
	virtual void OnMouseUp(int x, int y, unsigned int button);
	//virtual void OnMouseUp(int x, int y, unsigned int button);

	virtual void OnMouseEnter(int x, int y);
	virtual void OnMouseLeave(int x, int y);

	virtual void Draw(const Point& screenPos);

	virtual void TextPosition();
	inline bool GetState() { return state; }
	virtual void DoAction(); //action of button what ever it may be
	void SetTogglable(bool isTogglable);
	bool GetTogglable();
	inline bool GetToggleState();
	inline void SetToggleState(bool state);
	void SetActionCallback(ButtonAction * action);
	ButtonAction * GetActionCallback() { return actionCallback; }
	void SetText(std::string buttonText);
	void SetIcon(Icon icon);
protected:

	std::string buttonDisplayText;
	std::string ButtonText;

	bool isButtonDown, state, isMouseInside, isTogglable, toggle;
	ButtonAction * actionCallback;

};
}
#endif /* BUTTON_H_ */
