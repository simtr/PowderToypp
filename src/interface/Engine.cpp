#include <iostream>

#include "Config.h"
#include <stack>

#include "Global.h"
#include "interface/Window.h"
#include "interface/Platform.h"
#include "interface/Engine.h"
#include "Graphics.h"

using namespace ui;
using namespace std;

Engine::Engine():
	state_(NULL),
	mousex_(0),
	mousey_(0),
	mousexp_(0),
	mouseyp_(0),
	FpsLimit(60.0f),
	windows(stack<Window*>()),
	lastBuffer(NULL),
	prevBuffers(stack<pixel*>())
{
}

Engine::~Engine()
{
	if(state_ != NULL)
		delete state_;
	//Dispose of any Windows.
	while(!windows.empty())
	{
		delete windows.top();
		windows.pop();
	}
}

void Engine::Begin(int width, int height)
{
	//engine is now ready
	running_ = true;

	width_ = width;
	height_ = height;
}

void Engine::Exit()
{
	running_ = false;
}

void Engine::ShowWindow(Window * window)
{
	if(window->Position.X==-1)
	{
		window->Position.X = (width_-window->Size.X)/2;
	}
	if(window->Position.Y==-1)
	{
		window->Position.Y = (height_-window->Size.Y)/2;
	}
	if(state_)
	{
		if(lastBuffer)
		{
			prevBuffers.push(lastBuffer);
		}
		lastBuffer = (pixel*)malloc((width_ * height_) * PIXELSIZE);
		g->fillrect(0, 0, width_, height_, 0, 0, 0, 100);
		memcpy(lastBuffer, g->vid, (width_ * height_) * PIXELSIZE);

		windows.push(state_);
	}
	state_ = window;

}

void Engine::CloseWindow()
{
	if(!windows.empty())
	{
		if(!prevBuffers.empty())
		{
			lastBuffer = prevBuffers.top();
			prevBuffers.pop();
		}
		else
		{
			free(lastBuffer);
			lastBuffer = NULL;
		}
		state_ = windows.top();
		windows.pop();
	}
	else
	{
		state_ = NULL;
	}
}

/*void Engine::SetState(State * state)
{
	if(state_) //queue if currently in a state
		statequeued_ = state;
	else
	{
		state_ = state;
		if(state_)
			state_->DoInitialized();
	}
}*/

void Engine::SetSize(int width, int height)
{
	width_ = width;
	height_ = height;
}

void Engine::Tick(float dt)
{
	if(state_ != NULL)
		state_->DoTick(dt);

	/*if(statequeued_ != NULL)
	{
		if(state_ != NULL)
		{
			state_->DoExit();
			delete state_;
			state_ = NULL;
		}
		state_ = statequeued_;
		statequeued_ = NULL;

		if(state_ != NULL)
			state_->DoInitialized();
	}*/
}

void Engine::Draw()
{
	if(lastBuffer && !(state_->Position.X == 0 && state_->Position.Y == 0 && state_->Size.X == width_ && state_->Size.Y == height_))
	{
		memcpy(g->vid, lastBuffer, (width_ * height_) * PIXELSIZE);
	}
	else
	{
		g->Clear();
	}
	if(state_)
		state_->DoDraw();
	g->Blit();
}

void Engine::onKeyPress(int key, bool shift, bool ctrl, bool alt)
{
	if(state_)
		state_->DoKeyPress(key, shift, ctrl, alt);
}

void Engine::onKeyRelease(int key, bool shift, bool ctrl, bool alt)
{
	if(state_)
		state_->DoKeyRelease(key, shift, ctrl, alt);
}

void Engine::onMouseClick(int x, int y, unsigned button)
{
	if(state_)
		state_->DoMouseDown(x, y, button);
}

void Engine::onMouseUnclick(int x, int y, unsigned button)
{
	if(state_)
		state_->DoMouseUp(x, y, button);
}

void Engine::onMouseMove(int x, int y)
{
	mousex_ = x;
	mousey_ = y;
	if(state_)
	{
		state_->DoMouseMove(x, y, mousex_ - mousexp_, mousey_ - mouseyp_);
	}
	mousexp_ = x;
	mouseyp_ = y;
}

void Engine::onMouseWheel(int x, int y, int delta)
{
	if(state_)
		state_->DoMouseWheel(x, y, delta);
}

void Engine::onResize(int newWidth, int newHeight)
{
	SetSize(newWidth, newHeight);
}

void Engine::onClose()
{
	if(state_)
		state_->DoExit();
}