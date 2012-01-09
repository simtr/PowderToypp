#include "Element.h"

signed char pos_1_rx[] = {-1,-1,-1, 0, 0, 1, 1, 1};
signed char pos_1_ry[] = {-1, 0, 1,-1, 1,-1, 0, 1};

void pushParticle(Simulation * sim, int i, int count, int original)
{
	int rndstore, rnd, rx, ry, r, x, y, np, q, notctype=(((sim->parts[i].ctype)%3)+2);
	if ((sim->parts[i].tmp&0xFF) == 0 || count >= 2)//don't push if there is nothing there, max speed of 2 per frame
		return;
	x = (int)(sim->parts[i].x+0.5f);
	y = (int)(sim->parts[i].y+0.5f);
	if( !(sim->parts[i].tmp&0x200) )
	{ 
		//normal random push
		rndstore = rand();
		// RAND_MAX is at least 32767 on all platforms i.e. pow(8,5)-1
		// so can go 5 cycles without regenerating rndstore
		for (q=0; q<3; q++)//try to push twice
		{
			rnd = rndstore&7;
			rndstore = rndstore>>3;
			rx = pos_1_rx[rnd];
			ry = pos_1_ry[rnd];
			if (x+rx>=0 && y+ry>=0 && x+rx<XRES && y+ry<YRES)
			{
				r = sim->pmap[y+ry][x+rx];
				if (!r)
					continue;
				else if ((r&0xFF)==PT_PIPE && sim->parts[r>>8].ctype!=notctype && (sim->parts[r>>8].tmp&0xFF)==0)
				{
					sim->parts[r>>8].tmp = (sim->parts[r>>8].tmp&~0xFF) | (sim->parts[i].tmp&0xFF);
					sim->parts[r>>8].temp = sim->parts[i].temp;
					sim->parts[r>>8].flags = sim->parts[i].flags;
					sim->parts[r>>8].pavg[0] = sim->parts[i].pavg[0];
					sim->parts[r>>8].pavg[1] = sim->parts[i].pavg[1];
					if (r>>8 > original)
						sim->parts[r>>8].tmp2 = 1;//skip particle push, normalizes speed
					sim->parts[i].tmp &= ~0xFF;
					count++;
					pushParticle(sim, r>>8,count,original);
				}
			}
		}
	}
	else //predefined 1 pixel thick pipe movement
	{
		int coords = 7 - ((sim->parts[i].tmp>>10)&7);
		r = sim->pmap[y+ pos_1_ry[coords]][x+ pos_1_rx[coords]];
		if (!r)
		{
		}
		else if ((r&0xFF)==PT_PIPE && sim->parts[r>>8].ctype!=notctype && (sim->parts[r>>8].tmp&0xFF)==0)
		{
			sim->parts[r>>8].tmp = (sim->parts[r>>8].tmp&~0xFF) | (sim->parts[i].tmp&0xFF);
			sim->parts[r>>8].temp = sim->parts[i].temp;
			sim->parts[r>>8].flags = sim->parts[i].flags;
			sim->parts[r>>8].pavg[0] = sim->parts[i].pavg[0];
			sim->parts[r>>8].pavg[1] = sim->parts[i].pavg[1];
			if (r>>8 > original)
				sim->parts[r>>8].tmp2 = 1;//skip particle push, normalizes speed
			sim->parts[i].tmp &= ~0xFF;
			count++;
			pushParticle(sim, r>>8,count,original);
		}


	}
	return;
}

int update_PIPE(UPDATE_FUNC_ARGS) {
	int r, rx, ry, np;
	int rnd, rndstore;
	if (parts[i].ctype>=2 && parts[i].ctype<=4)
	{
		if (parts[i].life==3)
		{
			int lastneighbor = -1;
			int neighborcount = 0;
			int count = 0;
			// make automatic pipe pattern
			for (rx=-1; rx<2; rx++)
				for (ry=-1; ry<2; ry++)
					if (x+rx>=0 && y+ry>0 && x+rx<XRES && y+ry<YRES && (rx || ry))
					{
						r = pmap[y+ry][x+rx];
						if (!r)
							continue;
						if ((r&0xFF)==PT_PIPE&&parts[r>>8].ctype==1)
						{
							parts[r>>8].ctype = (((parts[i].ctype)%3)+2);//reverse
							parts[r>>8].life = 6;
							if ( parts[i].tmp&0x100)//is a single pixel pipe
							{
								parts[r>>8].tmp |= 0x200;//will transfer to a single pixel pipe
								parts[r>>8].tmp |= count<<10;//coords of where it came from
							}
							neighborcount ++;
							lastneighbor = r>>8;
						}
						else if ((r&0xFF)==PT_PIPE&&parts[r>>8].ctype!=(((parts[i].ctype-1)%3)+2))
						{
							neighborcount ++;
							lastneighbor = r>>8;
						}
						count++;
					}
					if(neighborcount == 1)
						parts[lastneighbor].tmp |= 0x100;
		}
		else
		{
			if (parts[i].tmp2 == 1)//skip particle push to prevent particle number being higher causeing speed up
			{
				parts[i].tmp2 = 0 ;
			}
			else
			{
				pushParticle(sim, i,0,i);
			}

			if (nt)//there is something besides PIPE around current particle
			{
				rndstore = rand();
				rnd = rndstore&7;
				rndstore = rndstore>>3;
				rx = pos_1_rx[rnd];
				ry = pos_1_ry[rnd];
				if (x+rx>=0 && y+ry>=0 && x+rx<XRES && y+ry<YRES)
				{
					r = pmap[y+ry][x+rx];
					if(!r)
						r = sim->photons[y+ry][x+rx];
					if (surround_space && !r && (parts[i].tmp&0xFF)!=0)  //creating at end
					{
						np = sim->create_part(-1,x+rx,y+ry,parts[i].tmp&0xFF);
						if (np!=-1)
						{
							parts[np].temp = parts[i].temp;//pipe saves temp and life now
							parts[np].life = parts[i].flags;
							parts[np].tmp = parts[i].pavg[0];
							parts[np].ctype = parts[i].pavg[1];
							parts[i].tmp &= ~0xFF;
						}
					}
					//try eating particle at entrance
					else if ((parts[i].tmp&0xFF) == 0 && (sim->ptypes[r&0xFF].falldown!= 0 || sim->ptypes[r&0xFF].state == ST_GAS))
					{
						if ((r&0xFF)==PT_SOAP)
							sim->detach(r>>8);
						parts[i].tmp =  (parts[i].tmp&~0xFF) | parts[r>>8].type;
						parts[i].temp = parts[r>>8].temp;
						parts[i].flags = parts[r>>8].life;
						parts[i].pavg[0] = parts[r>>8].tmp;
						parts[i].pavg[1] = parts[r>>8].ctype;
						sim->kill_part(r>>8);
					}
					else if ((parts[i].tmp&0xFF) == 0 && (r&0xFF)==PT_STOR && parts[r>>8].tmp && (sim->ptypes[parts[r>>8].tmp].falldown!= 0 || sim->ptypes[parts[r>>8].tmp].state == ST_GAS))
					{
						parts[i].tmp =  parts[r>>8].tmp;
						parts[i].temp = parts[r>>8].temp;
						parts[i].flags = parts[r>>8].flags;
						parts[i].pavg[0] = parts[r>>8].pavg[0];
						parts[i].pavg[1] = parts[r>>8].pavg[1];
						parts[r>>8].tmp = 0;
						parts[r>>8].life = 0;
					}
				}
			}
		}
	}
	else if (!parts[i].ctype && parts[i].life<=10)
	{
		if (parts[i].temp<272.15)//manual pipe colors
		{
			if (parts[i].temp>173.25&&parts[i].temp<273.15)
			{
				parts[i].ctype = 2;
				parts[i].life = 0;
			}
			if (parts[i].temp>73.25&&parts[i].temp<=173.15)
			{
				parts[i].ctype = 3;
				parts[i].life = 0;
			}
			if (parts[i].temp>=0&&parts[i].temp<=73.15)
			{
				parts[i].ctype = 4;
				parts[i].life = 0;
			}
		}
		else
		{
			// make a border
			for (rx=-2; rx<3; rx++)
				for (ry=-2; ry<3; ry++)
				{
					if (x+rx>=0 && y+ry>0 && x+rx<XRES && y+ry<YRES && (rx || ry))
					{
						r = pmap[y+ry][x+rx];
						if (!r)
							sim->create_part(-1,x+rx,y+ry,PT_BRCK);//BRCK border, people didn't like DMND
					}
				}
			if (parts[i].life<=1)
				parts[i].ctype = 1;
		}
	}
	else if (parts[i].ctype==1)//wait for empty space before starting to generate automatic pipe pattern
	{
		if (!parts[i].life)
		{
			for (rx=-1; rx<2; rx++)
				for (ry=-1; ry<2; ry++)
					if (x+rx>=0 && y+ry>0 && x+rx<XRES && y+ry<YRES && (rx || ry))
					{
						if (!pmap[y+ry][x+rx])
							parts[i].life=50;
					}
		}
		else if (parts[i].life==5)//check for beginning of pipe single pixel
		{
			int issingle = 1;
			for (rx=-1; rx<2; rx++)
				for (ry=-1; ry<2; ry++)
					if (x+rx>=0 && y+ry>0 && x+rx<XRES && y+ry<YRES && (rx || ry))
					{
						r = pmap[y+ry][x+rx];
						if ((r&0xFF)==PT_PIPE && parts[i].ctype==1 && parts[i].life )
							issingle = 0;
					}
					if (issingle)
						parts[i].tmp |= 0x100;
		}
		else if (parts[i].life==2)
		{
			parts[i].ctype = 2;
			parts[i].life = 6;
		}
	}
	return 0;
}

int graphics_PIPE(GRAPHICS_FUNC_ARGS)
{

	if ((cpart->tmp&0xFF)>0 && (cpart->tmp&0xFF)<PT_NUM)
	{
		//Create a temp. particle and do a subcall.
		Particle tpart;
		int t;
		memset(&tpart, 0, sizeof(Particle));
		tpart.type = cpart->tmp&0xFF;
		tpart.temp = cpart->temp;
		tpart.life = cpart->flags;
		tpart.tmp = cpart->pavg[0];
		tpart.ctype = cpart->pavg[1];
		t = tpart.type;
		if (ren->graphicscache[t].isready)
		{
			*pixel_mode = ren->graphicscache[t].pixel_mode;
			*colr = ren->graphicscache[t].colr;
			*colg = ren->graphicscache[t].colg;
			*colb = ren->graphicscache[t].colb;
			*firea = ren->graphicscache[t].firea;
			*firer = ren->graphicscache[t].firer;
			*fireg = ren->graphicscache[t].fireg;
			*fireb = ren->graphicscache[t].fireb;
		}
		else
		{
			*colr = PIXR(ren->sim->ptypes[t].pcolors);
			*colg = PIXR(ren->sim->ptypes[t].pcolors);
			*colb = PIXR(ren->sim->ptypes[t].pcolors);
			if (ren->sim->ptypes[t].graphics_func)
			{
				(*(ren->sim->ptypes[t].graphics_func))(ren, &tpart, nx, ny, pixel_mode, cola, colr, colg, colb, firea, firer, fireg, fireb);
			}
			else
			{
				graphics_DEFAULT(ren, &tpart, nx, ny, pixel_mode, cola, colr, colg, colb, firea, firer, fireg, fireb);
			}
		}
		//*colr = PIXR(ptypes[cpart->tmp&0xFF].pcolors);
		//*colg = PIXG(ptypes[cpart->tmp&0xFF].pcolors);
		//*colb = PIXB(ptypes[cpart->tmp&0xFF].pcolors);
	}
	else
	{
		if (cpart->ctype==2)
		{
			*colr = 50;
			*colg = 1;
			*colb = 1;
		}
		else if (cpart->ctype==3)
		{
			*colr = 1;
			*colg = 50;
			*colb = 1;
		}
		else if (cpart->ctype==4)
		{
			*colr = 1;
			*colg = 1;
			*colb = 50;
		}
		else if (cpart->temp<272.15&&cpart->ctype!=1)
		{
			if (cpart->temp>173.25&&cpart->temp<273.15)
			{
				*colr = 50;
				*colg = 1;
				*colb = 1;
			}
			if (cpart->temp>73.25&&cpart->temp<=173.15)
			{
				*colr = 1;
				*colg = 50;
				*colb = 1;
			}
			if (cpart->temp>=0&&cpart->temp<=73.15)
			{
				*colr = 1;
				*colg = 1;
				*colb = 50;
			}
		}
	}
	return 0;
}
