#include "simulation/Elements.h"
//#TPT-Directive ElementClass Element_NOTE PT_NOTE 165
Element_NOTE::Element_NOTE()
{
    Identifier = "DEFAULT_PT_NOTE";
    Name = "NOTE";
    Colour = PIXPACK(0xDFFF00);
    MenuVisible = 1;
    MenuSection = SC_ELEC;
    Enabled = 1;
    
    Advection = 0.6f;
    AirDrag = 0.01f * CFDS;
    AirLoss = 0.98f;
    Loss = 0.95f;
    Collision = 0.0f;
    Gravity = 0.0f;
    Diffusion = 0.00f;
    HotAir = 0.000f	* CFDS;
    Falldown = 0;
    
    Flammable = 0;
    Explosive = 0;
    Meltable = 0;
    Hardness = 20;
    
    Weight = 30;
    
    Temperature = R_TEMP-2.0f	+273.15f;
    HeatConduct = 29;
    Description = "PSCN to turn on, NSCN to turn off, metal for percussion.";
    
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
    
    Update = &Element_NOTE::update;
    Graphics = &Element_NOTE::graphics;
}

//#TPT-Directive ElementHeader Element_NOTE static int update(UPDATE_FUNC_ARGS)
int Element_NOTE::update(UPDATE_FUNC_ARGS)
{
	int r, rx, ry, nb;
	
}


//#TPT-Directive ElementHeader Element_NOTE static int graphics(GRAPHICS_FUNC_ARGS)
int Element_NOTE::graphics(GRAPHICS_FUNC_ARGS)

{
	return 0;
}


Element_NOTE::~Element_NOTE() {}
