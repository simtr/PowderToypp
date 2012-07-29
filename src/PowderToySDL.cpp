#ifdef USE_SDL

#include <map>
#include <string>
#include <time.h>
#include "SDL.h"
#ifdef WIN
#include "SDL_syswm.h"
#endif
#include <iostream>
#include <sstream>
#include <string>
#include "Config.h"
#include "graphics/Graphics.h"
#if defined(LIN)
#include "icon.h"
#endif

#include "interface/Engine.h"
#include "interface/Button.h"
#include "interface/Panel.h"
#include "interface/ControlFactory.h"
#include "interface/Point.h"
#include "interface/Label.h"
#include "simulation/SaveRenderer.h"
#include "client/Client.h"
#include "Misc.h"

#include "game/GameController.h"
#include "game/GameView.h"

#include "dialogues/ErrorMessage.h"

#include "client/HTTP.h"

using namespace std;

#ifdef WIN
extern "C" IMAGE_DOS_HEADER __ImageBase;
#endif

SDL_Surface * sdl_scrn;

#ifdef OGLI
void blit()
{
	SDL_GL_SwapBuffers();
}
#else
void blit(pixel * vid)
{
	if(sdl_scrn)
	{
		pixel * dst;
		pixel * src = vid;
		int j, x = 0, y = 0, w = XRES+BARSIZE, h = YRES+MENUSIZE, pitch = XRES+BARSIZE;
		if (SDL_MUSTLOCK(sdl_scrn))
			if (SDL_LockSurface(sdl_scrn)<0)
				return;
		dst=(pixel *)sdl_scrn->pixels+y*sdl_scrn->pitch/PIXELSIZE+x;
		for (j=0; j<h; j++)
		{
			memcpy(dst, src, w*PIXELSIZE);
			dst+=sdl_scrn->pitch/PIXELSIZE;
			src+=pitch;
		}
		if (SDL_MUSTLOCK(sdl_scrn))
			SDL_UnlockSurface(sdl_scrn);
		SDL_UpdateRect(sdl_scrn,0,0,0,0);
	}
}
#endif

SDL_Surface * SDLOpen()
{
	SDL_Surface * surface;
#if defined(WIN) && defined(WINCONSOLE)
	FILE * console = fopen("CON", "w" );
#endif
	if (SDL_Init(SDL_INIT_VIDEO)<0)
	{
		fprintf(stderr, "Initializing SDL: %s\n", SDL_GetError());
		return 0;
	}
	SDL_EnableUNICODE(1);
#if defined(WIN) && defined(WINCONSOLE)
	//On Windows, SDL redirects stdout to stdout.txt, which can be annoying when debugging, here we redirect back to the console
	if (console)
	{
		freopen("CON", "w", stdout);
		freopen("CON", "w", stderr);
		//fclose(console);
	}
#endif
#ifdef WIN
	SDL_SysWMinfo SysInfo;
	SDL_VERSION(&SysInfo.version);
	if(SDL_GetWMInfo(&SysInfo) <= 0) {
	    printf("%s : %d\n", SDL_GetError(), SysInfo.window);
	    exit(-1);
	}
	HWND WindowHandle = SysInfo.window;
	HICON hIconSmall = (HICON)LoadImage(reinterpret_cast<HMODULE>(&__ImageBase), MAKEINTRESOURCE(101), IMAGE_ICON, 16, 16, LR_SHARED);
	HICON hIconBig = (HICON)LoadImage(reinterpret_cast<HMODULE>(&__ImageBase), MAKEINTRESOURCE(101), IMAGE_ICON, 32, 32, LR_SHARED);
	SendMessage(WindowHandle, WM_SETICON, ICON_SMALL, (LPARAM)hIconSmall);
	SendMessage(WindowHandle, WM_SETICON, ICON_BIG, (LPARAM)hIconBig);
#elif defined(LIN)
	SDL_Surface *icon = SDL_CreateRGBSurfaceFrom(app_icon, 16, 16, 32, 64, 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
	SDL_WM_SetIcon(icon, NULL);
#endif

	SDL_WM_SetCaption("The Powder Toy", "Powder Toy");
	//SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
	atexit(SDL_Quit);
#ifndef OGLI
	surface = SDL_SetVideoMode(XRES + BARSIZE, YRES + MENUSIZE, 32, SDL_SWSURFACE);
#else
	surface = SDL_SetVideoMode((XRES + BARSIZE), (YRES + MENUSIZE), 32, SDL_OPENGL | SDL_RESIZABLE);
#endif

#if defined(OGLI)
	int status = glewInit();
	if(status != GLEW_OK)
	{
		fprintf(stderr, "Initializing Glew: %d\n", status);
		exit(-1);
	}
#endif

return surface;
}

std::map<std::string, std::string> readArguments(int argc, char * argv[])
{
	std::map<std::string, std::string> arguments;

	//Defaults 
	arguments["scale"] = "";
	arguments["proxy"] = "";
	arguments["nohud"] = "false";
	arguments["sound"] = "false";
	arguments["kiosk"] = "false";
	arguments["scripts"] = "false";
	arguments["open"] = "";
	arguments["ddir"] = "";
	arguments["ptsave"] = "";

	for (int i=1; i<argc; i++)
	{
		if (!strncmp(argv[i], "scale:", 6) && argv[i]+6)
		{
			arguments["scale"] = std::string(argv[i]+6);
		}
		else if (!strncmp(argv[i], "proxy:", 6) && argv[i]+6)
		{
			arguments["proxy"] =  std::string(argv[i]+6);
		}
		else if (!strncmp(argv[i], "nohud", 5))
		{
			arguments["nohud"] = "true";
		}
		else if (!strncmp(argv[i], "kiosk", 5))
		{
			arguments["kiosk"] = "true";
		}
		else if (!strncmp(argv[i], "sound", 5))
		{
			arguments["sound"] = "true";
		}
		else if (!strncmp(argv[i], "scripts", 8))
		{
			arguments["scripts"] = "true";
		}
		else if (!strncmp(argv[i], "open", 5) && i+1<argc)
		{
			arguments["open"] = std::string(argv[i+1]);;
			i++;
		}
		else if (!strncmp(argv[i], "ddir", 5) && i+1<argc)
		{
			arguments["ddir"] = std::string(argv[i+1]);
			i++;
		}
		else if (!strncmp(argv[i], "ptsave", 7) && i+1<argc)
		{
			arguments["ptsave"] = std::string(argv[i+1]);
			i++;
			break;
		}
	}
	return arguments;
}

int main(int argc, char * argv[])
{
	int elapsedTime = 0, currentTime = 0, lastTime = 0, currentFrame = 0;
	unsigned int lastTick = 0;
	float fps = 0, delta = 1.0f, inputScale = 1.0f;
	float currentWidth = XRES+BARSIZE, currentHeight = YRES+MENUSIZE;

	std::map<std::string, std::string> arguments = readArguments(argc, argv);

	sdl_scrn = SDLOpen();
#ifdef OGLI
	SDL_GL_SetAttribute (SDL_GL_DOUBLEBUFFER, 1);
	//glScaled(2.0f, 2.0f, 1.0f);

#endif
	ui::Engine::Ref().g = new Graphics();
	//ui::Engine::Ref().g->AttachSDLSurface(SDLOpen());

	ui::Engine * engine = &ui::Engine::Ref();
	engine->Begin(XRES+BARSIZE, YRES+MENUSIZE);

	GameController * gameController = new GameController();
	engine->ShowWindow(gameController->GetView());

	SDL_Event event;
	while(engine->Running())
	{
		event.type = 0;
		while (SDL_PollEvent(&event))
		{
			switch (event.type)
			{
			case SDL_QUIT:
				engine->Exit();
				break;
			case SDL_KEYDOWN:
				engine->onKeyPress(event.key.keysym.sym, event.key.keysym.unicode, event.key.keysym.mod&KEY_MOD_SHIFT, event.key.keysym.mod&KEY_MOD_CONTROL, event.key.keysym.mod&KEY_MOD_ALT);
				break;
			case SDL_KEYUP:
				engine->onKeyRelease(event.key.keysym.sym, event.key.keysym.unicode, event.key.keysym.mod&KEY_MOD_SHIFT, event.key.keysym.mod&KEY_MOD_CONTROL, event.key.keysym.mod&KEY_MOD_ALT);
				break;
			case SDL_MOUSEMOTION:
				engine->onMouseMove(event.motion.x*inputScale, event.motion.y*inputScale);
				break;
			case SDL_MOUSEBUTTONDOWN:
				if(event.button.button == SDL_BUTTON_WHEELUP)
				{
					engine->onMouseWheel(event.motion.x*inputScale, event.motion.y*inputScale, 1);
				}
				else if (event.button.button == SDL_BUTTON_WHEELDOWN)
				{
					engine->onMouseWheel(event.motion.x*inputScale, event.motion.y*inputScale, -1);
				}
				else
				{
					engine->onMouseClick(event.motion.x*inputScale, event.motion.y*inputScale, event.button.button);
				}
				break;
			case SDL_MOUSEBUTTONUP:
				if(event.button.button != SDL_BUTTON_WHEELUP && event.button.button != SDL_BUTTON_WHEELDOWN)
					engine->onMouseUnclick(event.motion.x*inputScale, event.motion.y*inputScale, event.button.button);
				break;
#ifdef OGLI
			case SDL_VIDEORESIZE:
				float ratio = float(XRES+BARSIZE) / float(YRES+MENUSIZE);
				float width = event.resize.w;
				float height = width/ratio;

				sdl_scrn = SDL_SetVideoMode(event.resize.w, height, 32, SDL_OPENGL | SDL_RESIZABLE);

				glViewport(0, 0, width, height);
				engine->g->Reset();
				//glScaled(width/currentWidth, height/currentHeight, 1.0f);

				currentWidth = width;
				currentHeight = height;
				inputScale = float(XRES+BARSIZE)/currentWidth;

				glLineWidth(currentWidth/float(XRES+BARSIZE));
				if(sdl_scrn == NULL)
				{
					std::cerr << "Oh bugger" << std::endl;
				}
				break;
#endif
			}
			event.type = 0; //Clear last event
		}

		engine->Tick();
		engine->Draw();
		
		if(SDL_GetTicks()-lastTick>500)
		{
			//Run client tick every second
			lastTick = SDL_GetTicks();
			Client::Ref().Tick();
		}

#ifdef OGLI
		blit();
#else
		blit(engine->g->vid);
#endif

		currentFrame++;
		currentTime = SDL_GetTicks();
		elapsedTime = currentTime - lastTime;
		if(ui::Engine::Ref().FpsLimit > 2.0f && (currentFrame>2 || elapsedTime > 1000*2/ui::Engine::Ref().FpsLimit) && elapsedTime && currentFrame*1000/elapsedTime > ui::Engine::Ref().FpsLimit)
		{
			while (currentFrame*1000/elapsedTime > ui::Engine::Ref().FpsLimit)
			{
				SDL_Delay(1);
				currentTime = SDL_GetTicks();
				elapsedTime = currentTime-lastTime;
			}
		}
		if(elapsedTime>=1000)
		{
			fps = (((float)currentFrame)/((float)elapsedTime))*1000.0f;
			currentFrame = 0;
			lastTime = currentTime;
			if(ui::Engine::Ref().FpsLimit > 2.0f)
			{
				delta = ui::Engine::Ref().FpsLimit/fps;
			}
			else
			{
				delta = 1.0f;
			}
		}
		engine->SetFps(fps);
	}
	ui::Engine::Ref().CloseWindow();
	delete gameController;
	delete ui::Engine::Ref().g;
	Client::Ref().Shutdown();
	return 0;
}

#endif
