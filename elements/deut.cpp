#include "Element.h"

int update_DEUT(UPDATE_FUNC_ARGS) {
	int r, rx, ry, trade, np;
	int maxlife = ((10000/(parts[i].temp + 1))-1);
	if ((10000%((int)parts[i].temp+1))>rand()%((int)parts[i].temp+1))
		maxlife ++;
	if (parts[i].life < maxlife)
	{
		for (rx=-1; rx<2; rx++)
			for (ry=-1; ry<2; ry++)
				if (x+rx>=0 && y+ry>0 && x+rx<XRES && y+ry<YRES && (rx || ry))
				{
					r = pmap[y+ry][x+rx];
					if (!r || (parts[i].life >=maxlife))
						continue;
					if ((r&0xFF)==PT_DEUT&&33>=rand()/(RAND_MAX/100)+1)
					{
						if ((parts[i].life + parts[r>>8].life + 1) <= maxlife)
						{
							parts[i].life += parts[r>>8].life + 1;
							sim->kill_part(r>>8);
						}
					}
				}
	}
	else
		for (rx=-1; rx<2; rx++)
			for (ry=-1; ry<2; ry++)
				if (x+rx>=0 && y+ry>0 && x+rx<XRES && y+ry<YRES && (rx || ry))
				{
					r = pmap[y+ry][x+rx];
					if (parts[i].life<=maxlife)
						continue;
					if ((!r)&&parts[i].life>=1)//if nothing then create deut
					{
						np = sim->create_part(-1,x+rx,y+ry,PT_DEUT);
						if (np<0) continue;
						parts[i].life--;
						parts[np].temp = parts[i].temp;
						parts[np].life = 0;
					}
				}
	for ( trade = 0; trade<4; trade ++)
	{
		rx = rand()%5-2;
		ry = rand()%5-2;
		if (x+rx>=0 && y+ry>0 && x+rx<XRES && y+ry<YRES && (rx || ry))
		{
			r = pmap[y+ry][x+rx];
			if (!r)
				continue;
			if ((r&0xFF)==PT_DEUT&&(parts[i].life>parts[r>>8].life)&&parts[i].life>0)//diffusion
			{
				int temp = parts[i].life - parts[r>>8].life;
				if (temp ==1)
				{
					parts[r>>8].life ++;
					parts[i].life --;
				}
				else if (temp>0)
				{
					parts[r>>8].life += temp/2;
					parts[i].life -= temp/2;
				}
			}
		}
	}
	return 0;
}

int graphics_DEUT(GRAPHICS_FUNC_ARGS)
{
	if(cpart->life>=700)
	{
		*colr += cpart->life*1;
		*colg += cpart->life*2;
		*colb += cpart->life*3;
		*pixel_mode |= PMODE_GLOW;
	}
	else
	{
		*colr += cpart->life*1;
		*colg += cpart->life*2;
		*colb += cpart->life*3;
		*pixel_mode |= PMODE_BLUR;
	}
	return 0;
}
