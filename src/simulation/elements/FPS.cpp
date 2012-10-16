#include "simulation/Elements.h"
#include "interface/Engine.h"
//#TPT-Directive ElementClass Element_FPS PT_FPS 166
Element_FPS::Element_FPS()
{
    Identifier = "DEFAULT_PT_FPS";
    Name = "FPS";
    Colour = PIXPACK(0xDD3333);
    MenuVisible = 1;
    MenuSection = SC_POWERED;
    Enabled = 1;
    
    Advection = 0.0f;
    AirDrag = 0.00f * CFDS;
    AirLoss = 0.90f;
    Loss = 0.00f;
    Collision = 0.0f;
    Gravity = 0.0f;
    Diffusion = 0.00f;
    HotAir = 0.000f	* CFDS;
    Falldown = 0;
    
    Flammable = 0;
    Explosive = 0;
    Meltable = 0;
    Hardness = 1;
    
    Weight = 100;
    
    Temperature = 6000.0f		+273.15f;
    HeatConduct = 0;
    Description = "Changes the fpscap to it's temp divided by 100";
    
    State = ST_NONE;
    Properties = TYPE_SOLID;
    
    LowPressure = IPL;
    LowPressureTransition = NT;
    HighPressure = IPH;
    HighPressureTransition = NT;
    LowTemperature = ITL;
    LowTemperatureTransition = NT;
    HighTemperature = ITH;
    HighTemperatureTransition = NT;
    
    Update = &Element_FPS::update;
    Graphics = &Element_FPS::graphics;
}

//#TPT-Directive ElementHeader Element_FPS static int update(UPDATE_FUNC_ARGS)
int Element_FPS::update(UPDATE_FUNC_ARGS)
 {
	int r, rx, ry;
	if(parts[i].life == 0)
		parts[i].tmp = -1;
	if (parts[i].life>0 && parts[i].life!=10)
	{
		parts[i].life--;
		if(parts[i].tmp != -1)
		{
			ui::Engine::Ref().FpsLimit = parts[i].tmp;
			parts[i].tmp = -1;
		}

	}
	if(parts[i].life>=10 && parts[i].tmp == -1)
	{
		parts[i].tmp = ui::Engine::Ref().FpsLimit; 
		ui::Engine::Ref().FpsLimit = 0.01f*(parts[i].temp-273.15);
	}
	if (parts[i].life==10)
	{
		for (rx=-2; rx<3; rx++)
			for (ry=-2; ry<3; ry++)
				if (x+rx>=0 && y+ry>0 && x+rx<XRES && y+ry<YRES && (rx || ry))
				{
					r = pmap[y+ry][x+rx];
					if (!r)
						continue;
					if ((r&0xFF)==PT_FPS)
					{
						if (parts[r>>8].life<10&&parts[r>>8].life>0)
							parts[i].life = 9;
						else if (parts[r>>8].life==0)
							parts[r>>8].life = 11;
					}
				}
	}
	return 0;
}


//#TPT-Directive ElementHeader Element_FPS static int graphics(GRAPHICS_FUNC_ARGS)
int Element_FPS::graphics(GRAPHICS_FUNC_ARGS)

{
	int lifemod = ((cpart->life>10?10:cpart->life)*19);
	*colg += lifemod;
	*colb += lifemod;
	return 0;
}


Element_FPS::~Element_FPS() {}
