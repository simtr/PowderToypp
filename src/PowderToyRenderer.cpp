#if defined(RENDERER)

#include <time.h>
#include <iostream>
#include <sstream>
#include <string>
#include <fstream>
#include <vector>

#include "Config.h"
#include "Format.h"
#include "interface/Engine.h"
#include "graphics/Graphics.h"
#include "graphics/Renderer.h"

#include "client/GameSave.h"
#include "simulation/Simulation.h"


void EngineProcess() {}

void readFile(std::string filename, std::vector<char> & storage)
{
	std::ifstream fileStream;
	fileStream.open(std::string(filename).c_str(), std::ios::binary);
	if(fileStream.is_open())
	{
		fileStream.seekg(0, std::ios::end);
		size_t fileSize = fileStream.tellg();
		fileStream.seekg(0);

		unsigned char * tempData = new unsigned char[fileSize];
		fileStream.read((char *)tempData, fileSize);
		fileStream.close();

		std::vector<unsigned char> fileData;
		storage.clear();
		storage.insert(storage.end(), tempData, tempData+fileSize);
		delete[] tempData;
	}
}

void writeFile(std::string filename, std::vector<char> & fileData)
{
	std::ofstream fileStream;
	fileStream.open(std::string(filename).c_str(), std::ios::binary);
	if(fileStream.is_open())
	{
		fileStream.write(&fileData[0], fileData.size());
		fileStream.close();
	}
}

int main(int argc, char *argv[])
{	
	ui::Engine * engine;
	std::string outputPrefix, inputFilename;
	std::vector<char> inputFile;
	std::string ppmFilename, ptiFilename, ptiSmallFilename, pngFilename, pngSmallFilename;
	std::vector<char> ppmFile, ptiFile, ptiSmallFile, pngFile, pngSmallFile;

	inputFilename = std::string(argv[1]);
	outputPrefix = std::string(argv[2]);

	ppmFilename = outputPrefix+".ppm";
	ptiFilename = outputPrefix+".pti";
	ptiSmallFilename = outputPrefix+"-small.pti";
	pngFilename = outputPrefix+".png";
	pngSmallFilename = outputPrefix+"-small.png";

	readFile(inputFilename, inputFile);

	ui::Engine::Ref().g = new Graphics();
	
	engine = &ui::Engine::Ref();
	engine->Begin(XRES+BARSIZE, YRES+MENUSIZE);

	GameSave * gameSave = new GameSave(inputFile);

	Simulation * sim = new Simulation();
	Renderer * ren = new Renderer(ui::Engine::Ref().g, sim);

	sim->Load(gameSave);


	//Render save
	ren->decorations_enable = true;
	ren->blackDecorations = true;

	int frame = 15;
	while(frame)
	{
		frame--;
		ren->render_parts();
		ren->render_fire();
		ren->clearScreen(1.0f);
	}

	ren->RenderBegin();
	ren->RenderEnd();

	VideoBuffer buf = ren->DumpFrame();
	//ppmFile = format::VideoBufferToPPM(screenBuffer);
	VideoBuffer small = new VideoBuffer(buf.Width/3,buf.Height/3);
	ptiFile = format::VideoBufferToPTI(buf);
	pngFile = format::VideoBufferToPNG(buf);
	int i,j,x,y,k;
	for(i=0;i<buf.Width;i++)
		for(j=0;j<buf.Height;j++)
			buf.Buffer[i+j*buf.Width]=(buf.Buffer[i+j*buf.Width]&0xFCFCFCFC)>>2;		
	for(i=0;i<small.Width;i++)
		for(j=0;j<small.Height;j++){
			small.Buffer[i+j*small.Width]=buf.Buffer[(i*3+1)+(j*3)*buf.Width]+buf.Buffer[(i*3)+(j*3+1)*buf.Width]+buf.Buffer[(i*3+1)+(j*3+2)*buf.Width]+buf.Buffer[(i*3+2)+(j*3+1)*buf.Width];
		}
	ptiSmallFile = format::VideoBufferToPTI(small);
	pngSmallFile = format::VideoBufferToPNG(small);
	//writeFile(ppmFilename, ppmFile);
	writeFile(ptiFilename, ptiFile);
	writeFile(ptiSmallFilename, ptiSmallFile);
	writeFile(pngFilename, pngFile);
	writeFile(pngSmallFilename, pngSmallFile);
}

#endif
