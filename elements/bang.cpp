#include "Element.h"

int update_BANG(UPDATE_FUNC_ARGS) {
	int r, rx, ry, nb;
	if(parts[i].tmp==0)
	{
		if(parts[i].temp>=673.0f)
			parts[i].tmp = 1;
		else
			for (rx=-1; rx<2; rx++)
				for (ry=-1; ry<2; ry++)
					if (x+rx>=0 && y+ry>0 && x+rx<XRES && y+ry<YRES && (rx || ry))
					{
						r = pmap[y+ry][x+rx];
						if (!r)
							continue;
						if ((r&0xFF)==PT_FIRE || (r&0xFF)==PT_PLSM)
						{
							parts[i].tmp = 1;
						}
						else if ((r&0xFF)==PT_SPRK || (r&0xFF)==PT_LIGH)
						{
							parts[i].tmp = 1;
						}
					}
	
	}
	else if(parts[i].tmp==1)
	{
		int tempvalue = 2;
		sim->flood_prop(x, y, offsetof(Particle, tmp), &tempvalue, 0);
	}
	else if(parts[i].tmp==2)
	{
		parts[i].tmp = 3;
	}
	else if(parts[i].tmp>=3)
	{
		float otemp = parts[i].temp-275.13f;
		//Explode!!
		sim->pv[y/CELL][x/CELL] += 0.5f;
		parts[i].tmp = 0;
		if(!(rand()%3))
		{
			if(!(rand()%2))
			{
				sim->create_part(i, x, y, PT_FIRE);
				parts[i].temp = restrict_flt((MAX_TEMP/4)+otemp, MIN_TEMP, MAX_TEMP);
			}
			else
			{
				sim->create_part(i, x, y, PT_SMKE);
				parts[i].temp = restrict_flt((MAX_TEMP/4)+otemp, MIN_TEMP, MAX_TEMP);
			}
		}
		else
		{
			if(!(rand()%15))
			{
				sim->create_part(i, x, y, PT_BOMB);
				parts[i].tmp = 1;
				parts[i].life = 50;
				parts[i].temp = restrict_flt((MAX_TEMP/3)+otemp, MIN_TEMP, MAX_TEMP);
				parts[i].vx = rand()%20-10;
				parts[i].vy = rand()%20-10;
			}
			else
			{
				sim->kill_part(i);
			}
		}
		return 1;
	}
	return 0;
}
