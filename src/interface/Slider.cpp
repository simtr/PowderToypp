/*
 * Slider.cpp
 *
 *  Created on: Mar 3, 2012
 *      Author: Simon
 */

#include <iostream>
#include "Slider.h"
#include "Colour.h"

namespace ui {

Slider::Slider(Point position, Point size, int steps):
		Component(position, size),
		sliderSteps(steps),
		sliderPosition(0),
		isMouseDown(false),
		bgGradient(NULL),
		col1(0, 0, 0, 0),
		col2(0, 0, 0, 0)
{
	// TODO Auto-generated constructor stub

}

void Slider::updatePosition(int position)
{
	if(position < 3)
		position = 3;
	if(position > Size.X-3)
		position = Size.X-3;

	float fPosition = position-3;
	float fSize = Size.X-6;
	float fSteps = sliderSteps;

	float fSliderPosition = (fPosition/fSize)*sliderSteps;//position;//((x-3)/(Size.X-6))*sliderSteps;

	int newSliderPosition = fSliderPosition;

	if(newSliderPosition == sliderPosition)
		return;

	sliderPosition = newSliderPosition;

	if(actionCallback)
	{
		actionCallback->ValueChangedCallback(this);
	}
}

void Slider::OnMouseMoved(int x, int y, int dx, int dy)
{
	if(isMouseDown)
	{
		updatePosition(x);
	}
}

void Slider::OnMouseClick(int x, int y, unsigned button)
{
	isMouseDown = true;
	updatePosition(x);
}

void Slider::OnMouseUp(int x, int y, unsigned button)
{
	if(isMouseDown)
	{
		isMouseDown = false;
	}
}

int Slider::GetValue()
{
	return sliderPosition;
}

void Slider::SetColour(Colour col1, Colour col2)
{
	pixel pix[2] = {PIXRGB(col1.Red, col1.Green, col1.Blue), PIXRGB(col2.Red, col2.Green, col2.Blue)};
	float fl[2] = {0.0f, 1.0f};
	if(bgGradient)
		free(bgGradient);
	this->col1 = col1;
	this->col2 = col2;
	bgGradient = (unsigned char*)Graphics::GenerateGradient(pix, fl, 2, Size.X-6);
}

void Slider::SetValue(int value)
{
	if(value < 0)
		value = 0;
	if(value > sliderSteps)
		value = sliderSteps;
	sliderPosition = value;
}

void Slider::Draw(const Point& screenPos)
{
	Graphics * g = Engine::Ref().g;
	//g->drawrect(screenPos.X, screenPos.Y, Size.X, Size.Y, 255, 255, 255, 255);

	if(bgGradient)
	{
#ifndef OGLI
		for (int j = 3; j < Size.Y-6; j++)
				for (int i = 3; i < Size.X-6; i++)
					g->blendpixel(screenPos.X+i+2, screenPos.Y+j+2, bgGradient[(i-3)*3], bgGradient[(i-3)*3+1], bgGradient[(i-3)*3+2], 255);
#else
		g->gradientrect(screenPos.X+5, screenPos.Y+5, Size.X-10, Size.Y-10, col1.Red, col1.Green, col1.Blue, col1.Alpha, col2.Red, col2.Green, col2.Blue, col2.Alpha);
#endif
	}

	g->drawrect(screenPos.X+3, screenPos.Y+3, Size.X-6, Size.Y-6, 255, 255, 255, 255);

	float fPosition = sliderPosition;
	float fSize = Size.X-6;
	float fSteps = sliderSteps;

	float fSliderX = (fSize/fSteps)*fPosition;//sliderPosition;//((Size.X-6)/sliderSteps)*sliderPosition;
	int sliderX = fSliderX;
	sliderX += 3;

	g->fillrect(screenPos.X+sliderX-2, screenPos.Y+1, 4, Size.Y-2, 20, 20, 20, 255);
	g->drawrect(screenPos.X+sliderX-2, screenPos.Y+1, 4, Size.Y-2, 200, 200, 200, 255);
}

Slider::~Slider()
{
}

} /* namespace ui */
