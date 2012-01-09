#include "Element.h"
/*these are the count values of where the particle gets stored, depending on where it came from
   0 1 2
   7 . 3
   6 5 4
   PRTO does (count+4)%8, so that it will come out at the opposite place to where it came in
   PRTO does +/-1 to the count, so it doesn't jam as easily
*/
int update_PRTO(UPDATE_FUNC_ARGS) {
	int r, nnx, rx, ry, np, fe = 0;
	int count = 0;
	parts[i].tmp = (int)((parts[i].temp-73.15f)/100+1);
	if (parts[i].tmp>=CHANNELS) parts[i].tmp = CHANNELS-1;
	else if (parts[i].tmp<0) parts[i].tmp = 0;
	for (count=0; count<8; count++)
	{
		rx = sim->portal_rx[count];
		ry = sim->portal_ry[count];
			if (x+rx>=0 && y+ry>0 && x+rx<XRES && y+ry<YRES && (rx || ry))
			{
				r = pmap[y+ry][x+rx];
				if (!r)
					fe = 1;
				if (r)
					continue;
				if (!r)
				{
					for ( nnx =0 ; nnx<80; nnx++)
					{
						int randomness = (count + rand()%3-1 + 4)%8;//add -1,0,or 1 to count
						if (sim->portalp[parts[i].tmp][randomness][nnx].type==PT_SPRK)// TODO: make it look better, spark creation
						{
							sim->create_part(-1,x+1,y,PT_SPRK);
							sim->create_part(-1,x+1,y+1,PT_SPRK);
							sim->create_part(-1,x+1,y-1,PT_SPRK);
							sim->create_part(-1,x,y-1,PT_SPRK);
							sim->create_part(-1,x,y+1,PT_SPRK);
							sim->create_part(-1,x-1,y+1,PT_SPRK);
							sim->create_part(-1,x-1,y,PT_SPRK);
							sim->create_part(-1,x-1,y-1,PT_SPRK);
							sim->portalp[parts[i].tmp][randomness][nnx] = sim->emptyparticle;
							break;
						}
						else if (sim->portalp[parts[i].tmp][randomness][nnx].type)
						{
							if (sim->portalp[parts[i].tmp][randomness][nnx].type==PT_STKM)
								sim->player.spwn = 0;
							if (sim->portalp[parts[i].tmp][randomness][nnx].type==PT_STKM2)
								sim->player2.spwn = 0;
							np = sim->create_part(-1, x+rx, y+ry, sim->portalp[parts[i].tmp][randomness][nnx].type);
							if (np<0) continue;
							parts[np] = sim->portalp[parts[i].tmp][randomness][nnx];
							parts[np].x = x+rx;
							parts[np].y = y+ry;
							sim->portalp[parts[i].tmp][randomness][nnx] = sim->emptyparticle;
							break;
						}
					}
				}
			}
	}
	if (fe) {
		int orbd[4] = {0, 0, 0, 0};	//Orbital distances
		int orbl[4] = {0, 0, 0, 0};	//Orbital locations
		if (!parts[i].life) parts[i].life = rand();
		if (!parts[i].ctype) parts[i].life = rand();
		sim->orbitalparts_get(parts[i].life, parts[i].ctype, orbd, orbl);
		for (r = 0; r < 4; r++) {
			if (orbd[r]<254) {
				orbd[r] += 16;
				if (orbd[r]>254) {
					orbd[r] = 0;
					orbl[r] = rand()%255;
				}
				//orbl[r] += 1;
				//orbl[r] = orbl[r]%255;
			} else {
				orbd[r] = 0;
				orbl[r] = rand()%255;
			}
		}
		sim->orbitalparts_set(&parts[i].life, &parts[i].ctype, orbd, orbl);
	} else {
		parts[i].life = 0;
		parts[i].ctype = 0;
	}
	return 0;
}
