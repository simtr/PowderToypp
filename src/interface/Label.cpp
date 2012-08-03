#include <string>
#include "Config.h"
#include "Point.h"
#include "Label.h"
#include "ContextMenu.h"

using namespace ui;

Label::Label(Point position, Point size, std::string labelText):
	Component(position, size),
	text(labelText),
	textColour(255, 255, 255),
	selectionIndex0(-1),
	selectionIndex1(-1),
	selectionXL(-1),
	selectionXH(-1),
	multiline(false),
	selecting(false),
	autoHeight(size.Y==-1?true:false),
	caret(-1)
{
	menu = new ContextMenu(this);
	menu->AddItem(ContextMenuItem("Copy", 0, true));
}

Label::~Label()
{

}

void Label::SetMultiline(bool status)
{
	multiline = status;
	if(status)
	{
		updateMultiline();
		updateSelection();
	}
}

void Label::SetText(std::string text)
{
	this->text = text;
	if(multiline)
	{
		updateMultiline();
		updateSelection();
	}
	TextPosition(text);
}

void Label::updateMultiline()
{
	int lines = 1;
	if(text.length()>0)
	{
		char * rawText = new char[text.length()+1];
		std::copy(text.begin(), text.end(), rawText);
		rawText[text.length()] = 0;

		int currentWidth = 0;
		char * lastSpace = NULL;
		char * currentWord = rawText;
		char * nextSpace;
		while(true)
		{
			nextSpace = strchr(currentWord+1, ' ');
			if(nextSpace)
				nextSpace[0] = 0;
			int width = Graphics::textwidth(currentWord);
			if(width+currentWidth > Size.X-6)
			{
				currentWidth = width;
				if(currentWord!=rawText)
				{
					currentWord[0] = '\n';
					lines++;
				}
			}
			else
				currentWidth += width;
			if(nextSpace)
				nextSpace[0] = ' ';
			if(!currentWord[0] || !currentWord[1] || !(currentWord = strchr(currentWord+1, ' ')))
				break;
		}
		if(autoHeight)
		{
			Size.Y = lines*12;
		}
		textLines = std::string(rawText);
		delete[] rawText;
	}
	else
	{
		if(autoHeight)
		{
			Size.Y = 12;
		}
		textLines = std::string("");
	}
}

std::string Label::GetText()
{
	return this->text;
}

void Label::OnContextMenuAction(int item)
{
	switch(item)
	{
	case 0:
		copySelection();
		break;
	}
}

void Label::OnMouseClick(int x, int y, unsigned button)
{
	if(button == BUTTON_RIGHT)
	{
		if(menu)
			menu->Show(GetParentWindow()->Position + Position + ui::Point(x, y));
	}
	else
	{
		selecting = true;
		if(multiline)
			selectionIndex0 = Graphics::CharIndexAtPosition((char*)textLines.c_str(), x-textPosition.X, y-textPosition.Y);
		else
			selectionIndex0 = Graphics::CharIndexAtPosition((char*)text.c_str(), x-textPosition.X, y-textPosition.Y);
		selectionIndex1 = selectionIndex0;

		updateSelection();
	}
}

void Label::copySelection()
{
	std::string currentText;

	if(multiline)
		currentText = textLines;
	else
		currentText = text;

	if(selectionIndex1 > selectionIndex0) {
		clipboard_push_text((char*)currentText.substr(selectionIndex0, selectionIndex1-selectionIndex0).c_str());
	} else if(selectionIndex0 > selectionIndex1) {
		clipboard_push_text((char*)currentText.substr(selectionIndex1, selectionIndex0-selectionIndex1).c_str());
	} else {
		clipboard_push_text((char*)currentText.c_str());
	}
}

void Label::OnMouseUp(int x, int y, unsigned button)
{
	selecting = false;
}

void Label::OnKeyPress(int key, Uint16 character, bool shift, bool ctrl, bool alt)
{
	if(ctrl && key == 'c')
	{
		copySelection();
	}
}

void Label::OnMouseMoved(int localx, int localy, int dx, int dy)
{
	if(selecting)
	{
		if(multiline)
			selectionIndex1 = Graphics::CharIndexAtPosition((char*)textLines.c_str(), localx-textPosition.X, localy-textPosition.Y);
		else
			selectionIndex1 = Graphics::CharIndexAtPosition((char*)text.c_str(), localx-textPosition.X, localy-textPosition.Y);
		updateSelection();
	}
}

void Label::Tick(float dt)
{
	if(!this->IsFocused() && (selecting || (selectionIndex0 != -1 && selectionIndex1 != -1)))
	{
		ClearSelection();
	}
}

int Label::getLowerSelectionBound()
{
	return (selectionIndex0 > selectionIndex1) ? selectionIndex1 : selectionIndex0;
}

int Label::getHigherSelectionBound()
{
	return (selectionIndex0 > selectionIndex1) ? selectionIndex0 : selectionIndex1;
}

bool Label::HasSelection()
{
	if(selectionIndex0 != -1 && selectionIndex1 != -1 && selectionIndex0 != selectionIndex1)
		return true;
	return false;
}

void Label::ClearSelection()
{
	selecting = false;
	selectionIndex0 = -1;
	selectionIndex1 = -1;
	updateSelection();
}

void Label::updateSelection()
{
	std::string currentText;

	if(selectionIndex0 < 0) selectionIndex0 = 0;
	if(selectionIndex0 > text.length()) selectionIndex0 = text.length();
	if(selectionIndex1 < 0) selectionIndex1 = 0;
	if(selectionIndex1 > text.length()) selectionIndex1 = text.length();

	if(selectionIndex0 == -1 || selectionIndex1 == -1)
	{
		selectionXH = -1;
		selectionXL = -1;

		textFragments = std::string(currentText);
		return;
	}

	if(multiline)
		currentText = textLines;
	else
		currentText = text;

	if(selectionIndex1 > selectionIndex0) {
		selectionLineH = Graphics::PositionAtCharIndex((char*)currentText.c_str(), selectionIndex1, selectionXH, selectionYH);
		selectionLineL = Graphics::PositionAtCharIndex((char*)currentText.c_str(), selectionIndex0, selectionXL, selectionYL);

		textFragments = std::string(currentText);
		textFragments.insert(selectionIndex1, "\x0E");
		textFragments.insert(selectionIndex0, "\x0F\x01\x01\x01");
	} else if(selectionIndex0 > selectionIndex1) {
		selectionLineH = Graphics::PositionAtCharIndex((char*)currentText.c_str(), selectionIndex0, selectionXH, selectionYH);
		selectionLineL = Graphics::PositionAtCharIndex((char*)currentText.c_str(), selectionIndex1, selectionXL, selectionYL);

		textFragments = std::string(currentText);
		textFragments.insert(selectionIndex0, "\x0E");
		textFragments.insert(selectionIndex1, "\x0F\x01\x01\x01");
	} else {
		selectionXH = -1;
		selectionXL = -1;

		textFragments = std::string(currentText);
	}
}

void Label::Draw(const Point& screenPos)
{
	if(!drawn)
	{
		if(multiline)
		{
			TextPosition(textLines);
			updateMultiline();
			updateSelection();
		}
		else
			TextPosition(text);
		drawn = true;
	}
	Graphics * g = Engine::Ref().g;

	if(multiline)
	{
		if(selectionXL != -1 && selectionXH != -1)
		{
			if(selectionLineH - selectionLineL > 0)
			{
				g->fillrect(screenPos.X+textPosition.X+selectionXL, (screenPos.Y+textPosition.Y-1)+selectionYL, textSize.X-(selectionXL), 10, 255, 255, 255, 255);
				for(int i = 1; i < selectionLineH-selectionLineL; i++)
				{
					g->fillrect(screenPos.X+textPosition.X, (screenPos.Y+textPosition.Y-1)+selectionYL+(i*12), textSize.X, 10, 255, 255, 255, 255);
				}
				g->fillrect(screenPos.X+textPosition.X, (screenPos.Y+textPosition.Y-1)+selectionYH, selectionXH, 10, 255, 255, 255, 255);

			} else {
				g->fillrect(screenPos.X+textPosition.X+selectionXL, screenPos.Y+selectionYL+textPosition.Y-1, selectionXH-(selectionXL), 10, 255, 255, 255, 255);
			}
			g->drawtext(screenPos.X+textPosition.X, screenPos.Y+textPosition.Y, textFragments, textColour.Red, textColour.Green, textColour.Blue, 255);
		}
		else
		{
			g->drawtext(screenPos.X+textPosition.X, screenPos.Y+textPosition.Y, textLines, textColour.Red, textColour.Green, textColour.Blue, 255);
		}
	} else {
		if(selectionXL != -1 && selectionXH != -1)
		{
			g->fillrect(screenPos.X+textPosition.X+selectionXL, screenPos.Y+textPosition.Y-1, selectionXH-(selectionXL), 10, 255, 255, 255, 255);
			g->drawtext(screenPos.X+textPosition.X, screenPos.Y+textPosition.Y, textFragments, textColour.Red, textColour.Green, textColour.Blue, 255);
		}
		else
		{
			g->drawtext(screenPos.X+textPosition.X, screenPos.Y+textPosition.Y, text, textColour.Red, textColour.Green, textColour.Blue, 255);
		}
	}
}

