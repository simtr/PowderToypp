/*
 * LuaScriptInterface.cpp
 *
 *  Created on: Feb 11, 2012
 *      Author: Simon
 */

#include <string>
#include "Config.h"
#include "LuaScriptInterface.h"
#include "simulation/Simulation.h"
#include "game/GameModel.h"
#include "LuaScriptHelper.h"

LuaScriptInterface::LuaScriptInterface(GameModel * m):
	CommandInterface(m),
	currentCommand(false)
{
	int i = 0, j;
	char tmpname[12];
	int currentElementMeta, currentElement;
	const static struct luaL_reg tptluaapi [] = {
		{"test", &luatpt_test},
		{"drawtext", &luatpt_drawtext},
		{"create", &luatpt_create},
		{"set_pause", &luatpt_setpause},
		{"toggle_pause", &luatpt_togglepause},
		{"set_console", &luatpt_setconsole},
		{"log", &luatpt_log},
		{"set_pressure", &luatpt_set_pressure},
		{"set_gravity", &luatpt_set_gravity},
		{"reset_gravity_field", &luatpt_reset_gravity_field},
		{"reset_velocity", &luatpt_reset_velocity},
		{"reset_spark", &luatpt_reset_spark},
		{"set_property", &luatpt_set_property},
		{"get_property", &luatpt_get_property},
		{"set_wallmap", &luatpt_set_wallmap},
		{"get_wallmap", &luatpt_get_wallmap},
		{"set_elecmap", &luatpt_set_elecmap},
		{"get_elecmap", &luatpt_get_elecmap},
		{"drawpixel", &luatpt_drawpixel},
		{"drawrect", &luatpt_drawrect},
		{"fillrect", &luatpt_fillrect},
		{"drawline", &luatpt_drawline},
		{"textwidth", &luatpt_textwidth},
		{"get_name", &luatpt_get_name},
		{"set_shortcuts", &luatpt_set_shortcuts},
		{"delete", &luatpt_delete},
		{"register_step", &luatpt_register_step},
		{"unregister_step", &luatpt_unregister_step},
		{"register_mouseclick", &luatpt_register_mouseclick},
		{"unregister_mouseclick", &luatpt_unregister_mouseclick},
		{"register_keypress", &luatpt_register_keypress},
		{"unregister_keypress", &luatpt_unregister_keypress},
		{"register_mouseevent", &luatpt_register_mouseclick},
		{"unregister_mouseevent", &luatpt_unregister_mouseclick},
		{"register_keyevent", &luatpt_register_keypress},
		{"unregister_keyevent", &luatpt_unregister_keypress},
		{"input", &luatpt_input},
		{"message_box", &luatpt_message_box},
		{"get_numOfParts", &luatpt_get_numOfParts},
		{"start_getPartIndex", &luatpt_start_getPartIndex},
		{"next_getPartIndex", &luatpt_next_getPartIndex},
		{"getPartIndex", &luatpt_getPartIndex},
		{"hud", &luatpt_hud},
		{"newtonian_gravity", &luatpt_gravity},
		{"ambient_heat", &luatpt_airheat},
		{"active_menu", &luatpt_active_menu},
		{"decorations_enable", &luatpt_decorations_enable},
		{"display_mode", &luatpt_cmode_set},
		{"throw_error", &luatpt_error},
		{"heat", &luatpt_heat},
		{"setfire", &luatpt_setfire},
		{"setdebug", &luatpt_setdebug},
		{"setfpscap",&luatpt_setfpscap},
		{"getscript",&luatpt_getscript},
		{"setwindowsize",&luatpt_setwindowsize},
		{"watertest",&luatpt_togglewater},
		{"screenshot",&luatpt_screenshot},
		{"element",&luatpt_getelement},
		{"element_func",&luatpt_element_func},
		{NULL,NULL}
	};

	luacon_currentCommand = &currentCommand;
	luacon_lastError = &lastError;

	luacon_model = m;
	luacon_sim = m->GetSimulation();
	luacon_g = ui::Engine::Ref().g;
	luacon_ci = this;

	l = lua_open();
	luaL_openlibs(l);

	//Replace print function with our screen logging thingy
	lua_pushcfunction(l, luatpt_log);
	lua_setglobal(l, "print");

	//Register all tpt functions
	luaL_register(l, "tpt", tptluaapi);

	tptProperties = lua_gettop(l);

	lua_pushinteger(l, 0);
	lua_setfield(l, tptProperties, "mousex");
	lua_pushinteger(l, 0);
	lua_setfield(l, tptProperties, "mousey");
	lua_pushinteger(l, 0);
	lua_setfield(l, tptProperties, "selectedl");
	lua_pushinteger(l, 0);
	lua_setfield(l, tptProperties, "selectedr");

	lua_newtable(l);
	tptPropertiesVersion = lua_gettop(l);
	lua_pushinteger(l, SAVE_VERSION);
	lua_setfield(l, tptPropertiesVersion, "major");
	lua_pushinteger(l, MINOR_VERSION);
	lua_setfield(l, tptPropertiesVersion, "minor");
	lua_pushinteger(l, BUILD_NUM);
	lua_setfield(l, tptPropertiesVersion, "build");
	lua_setfield(l, tptProperties, "version");

#ifdef FFI
	//LuaJIT's ffi gives us direct access to parts data, no need for nested metatables. HOWEVER, this is in no way safe, it's entirely possible for someone to try to read parts[-10]
	lua_pushlightuserdata(l, parts);
	lua_setfield(l, tptProperties, "partsdata");

	luaL_dostring (l, "ffi = require(\"ffi\")\n\
ffi.cdef[[\n\
typedef struct { int type; int life, ctype; float x, y, vx, vy; float temp; float pavg[2]; int flags; int tmp; int tmp2; unsigned int dcolour; } particle;\n\
]]\n\
tpt.parts = ffi.cast(\"particle *\", tpt.partsdata)\n\
ffi = nil\n\
tpt.partsdata = nil");
	//Since ffi is REALLY REALLY dangrous, we'll remove it from the environment completely (TODO)
	//lua_pushstring(l, "parts");
	//tptPartsCData = lua_gettable(l, tptProperties);
#else
	lua_newtable(l);
	tptParts = lua_gettop(l);
	lua_newtable(l);
	tptPartsMeta = lua_gettop(l);
	lua_pushcfunction(l, luacon_partswrite);
	lua_setfield(l, tptPartsMeta, "__newindex");
	lua_pushcfunction(l, luacon_partsread);
	lua_setfield(l, tptPartsMeta, "__index");
	lua_setmetatable(l, tptParts);
	lua_setfield(l, tptProperties, "parts");

	lua_newtable(l);
	tptPart = lua_gettop(l);
	lua_newtable(l);
	tptPartMeta = lua_gettop(l);
	lua_pushcfunction(l, luacon_partwrite);
	lua_setfield(l, tptPartMeta, "__newindex");
	lua_pushcfunction(l, luacon_partread);
	lua_setfield(l, tptPartMeta, "__index");
	lua_setmetatable(l, tptPart);

	tptPart = luaL_ref(l, LUA_REGISTRYINDEX);
#endif

	lua_newtable(l);
	tptElements = lua_gettop(l);
	for(i = 1; i < PT_NUM; i++)
	{
		for(j = 0; j < strlen(luacon_sim->elements[i].Name); j++)
			tmpname[j] = tolower(luacon_sim->elements[i].Name[j]);
		tmpname[strlen(luacon_sim->elements[i].Name)] = 0;

		lua_newtable(l);
		currentElement = lua_gettop(l);
		lua_pushinteger(l, i);
		lua_setfield(l, currentElement, "id");

		lua_newtable(l);
		currentElementMeta = lua_gettop(l);
		lua_pushcfunction(l, luacon_elementwrite);
		lua_setfield(l, currentElementMeta, "__newindex");
		lua_pushcfunction(l, luacon_elementread);
		lua_setfield(l, currentElementMeta, "__index");
		lua_setmetatable(l, currentElement);

		lua_setfield(l, tptElements, tmpname);
	}
	lua_setfield(l, tptProperties, "el");

	lua_newtable(l);
	tptElementTransitions = lua_gettop(l);
	for(i = 1; i < PT_NUM; i++)
	{
		for(j = 0; j < strlen(luacon_sim->elements[i].Name); j++)
			tmpname[j] = tolower(luacon_sim->elements[i].Name[j]);
		tmpname[strlen(luacon_sim->elements[i].Name)] = 0;

		lua_newtable(l);
		currentElement = lua_gettop(l);
		lua_newtable(l);
		currentElementMeta = lua_gettop(l);
		lua_pushinteger(l, i);
		lua_setfield(l, currentElement, "value");
		lua_pushcfunction(l, luacon_transitionwrite);
		lua_setfield(l, currentElementMeta, "__newindex");
		lua_pushcfunction(l, luacon_transitionread);
		lua_setfield(l, currentElementMeta, "__index");
		lua_setmetatable(l, currentElement);

		lua_setfield(l, tptElementTransitions, tmpname);
	}
	lua_setfield(l, tptProperties, "eltransition");

	lua_el_func = (int*)calloc(PT_NUM, sizeof(int));
	lua_el_mode = (int*)calloc(PT_NUM, sizeof(int));
	for(i = 0; i < PT_NUM; i++)
	{
		lua_el_mode[i] = 0;
	}

}

bool LuaScriptInterface::OnBrushChanged(int brushType, int rx, int ry)
{
	luacon_brushx = rx;
	luacon_brushy = ry;
	return true;
}

bool LuaScriptInterface::OnMouseMove(int x, int y, int dx, int dy)
{
	luacon_mousex = x;
	luacon_mousey = y;
	return true;
}

bool LuaScriptInterface::OnMouseDown(int x, int y, unsigned button)
{
	luacon_mousedown = true;
	luacon_mousebutton = button;
	return luacon_mouseevent(x, y, button, LUACON_MDOWN, 0);
}

bool LuaScriptInterface::OnMouseUp(int x, int y, unsigned button)
{
	luacon_mousedown = false;
	return luacon_mouseevent(x, y, button, LUACON_MUP, 0);
}

bool LuaScriptInterface::OnMouseWheel(int x, int y, int d)
{
	return luacon_mouseevent(x, y, luacon_mousedown?luacon_mousebutton:0, 0, d);
}

bool LuaScriptInterface::OnKeyPress(int key, Uint16 character, bool shift, bool ctrl, bool alt)
{
	int modifiers;
	if(shift)
		modifiers |= 0x001;
	if(ctrl)
		modifiers |= 0x040;
	if(alt)
		modifiers |= 0x100;
	return luacon_keyevent(key, modifiers, LUACON_KDOWN);
}

bool LuaScriptInterface::OnKeyRelease(int key, Uint16 character, bool shift, bool ctrl, bool alt)
{
	int modifiers;
	if(shift)
		modifiers |= 0x001;
	if(ctrl)
		modifiers |= 0x040;
	if(alt)
		modifiers |= 0x100;
	return luacon_keyevent(key, modifiers, LUACON_KUP);
}

void LuaScriptInterface::OnTick()
{
	if(luacon_mousedown)
		luacon_mouseevent(luacon_mousex, luacon_mousey, luacon_mousebutton, LUACON_MPRESS, 0);
	luacon_step(luacon_mousex, luacon_mousey, luacon_selectedl, luacon_selectedr, luacon_brushx, luacon_brushy);
}

int LuaScriptInterface::Command(std::string command)
{
	int ret;
	lastError = "";
	currentCommand = true;
	if((ret = luaL_dostring(l, command.c_str())))
	{
		lastError = luacon_geterror();
		//Log(LogError, lastError);
	}
	currentCommand = false;
	return ret;
}

std::string LuaScriptInterface::FormatCommand(std::string command)
{
	return command;
}

LuaScriptInterface::~LuaScriptInterface() {
	// TODO Auto-generated destructor stub
}

#ifndef FFI
int luacon_partread(lua_State* l){
	int format, offset, tempinteger;
	float tempfloat;
	int i;
	char * key = mystrdup((char*)luaL_optstring(l, 2, ""));
	offset = luacon_particle_getproperty(key, &format);

	i = cIndex;

	if(i < 0 || i >= NPART || offset==-1)
	{
		if(i < 0 || i >= NPART) {
			free(key);
			return luaL_error(l, "Out of range");
		} else if(strcmp(key, "id")==0) {
			free(key);
			lua_pushnumber(l, i);
			return 1;
		} else {
			free(key);
			return luaL_error(l, "Invalid property");
		}
	}
	free(key);
	switch(format)
	{
	case 0:
		tempinteger = *((int*)(((unsigned char*)&luacon_sim->parts[i])+offset));
		lua_pushnumber(l, tempinteger);
		break;
	case 1:
		tempfloat = *((float*)(((unsigned char*)&luacon_sim->parts[i])+offset));
		lua_pushnumber(l, tempfloat);
		break;
	}
	return 1;
}
int luacon_partwrite(lua_State* l){
	int format, offset;
	int i;
	char * key = mystrdup((char*)luaL_optstring(l, 2, ""));
	offset = luacon_particle_getproperty(key, &format);

	i = cIndex;

	if(i < 0 || i >= NPART || offset==-1)
	{
		if(i < 0 || i >= NPART) {
			free(key);
			return luaL_error(l, "array index out of bounds");
		} else {
			free(key);
			return luaL_error(l, "Invalid property");
		}
	}
	free(key);
	switch(format)
	{
	case 0:
		*((int*)(((unsigned char*)&luacon_sim->parts[i])+offset)) = luaL_optinteger(l, 3, 0);
		break;
	case 1:
		*((float*)(((unsigned char*)&luacon_sim->parts[i])+offset)) = luaL_optnumber(l, 3, 0);
		break;
	}
	return 1;
}
int luacon_partsread(lua_State* l){
	int format, offset;
	char * tempstring;
	int tempinteger;
	float tempfloat;
	int i, currentPart, currentPartMeta;

	i = luaL_optinteger(l, 2, 0);

	if(i<0 || i>=NPART)
	{
		return luaL_error(l, "array index out of bounds");
	}

	lua_rawgeti(l, LUA_REGISTRYINDEX, tptPart);
	cIndex = i;
	return 1;
}
int luacon_partswrite(lua_State* l){
	return luaL_error(l, "table readonly");
}
#endif
int luacon_particle_getproperty(char * key, int * format)
{
	int offset;
	if (strcmp(key, "type")==0){
		offset = offsetof(Particle, type);
		*format = 0;
	} else if (strcmp(key, "life")==0){
		offset = offsetof(Particle, life);
		*format = 0;
	} else if (strcmp(key, "ctype")==0){
		offset = offsetof(Particle, ctype);
		*format = 0;
	} else if (strcmp(key, "temp")==0){
		offset = offsetof(Particle, temp);
		*format = 1;
	} else if (strcmp(key, "tmp")==0){
		offset = offsetof(Particle, tmp);
		*format = 0;
	} else if (strcmp(key, "tmp2")==0){
		offset = offsetof(Particle, tmp2);
		*format = 0;
	} else if (strcmp(key, "vy")==0){
		offset = offsetof(Particle, vy);
		*format = 1;
	} else if (strcmp(key, "vx")==0){
		offset = offsetof(Particle, vx);
		*format = 1;
	} else if (strcmp(key, "x")==0){
		offset = offsetof(Particle, x);
		*format = 1;
	} else if (strcmp(key, "y")==0){
		offset = offsetof(Particle, y);
		*format = 1;
	} else if (strcmp(key, "dcolour")==0){
		offset = offsetof(Particle, dcolour);
		*format = 0;
	} else if (strcmp(key, "dcolor")==0){
		offset = offsetof(Particle, dcolour);
		*format = 0;
	} else {
		offset = -1;
	}
	return offset;
}
int luacon_transition_getproperty(char * key, int * format)
{
	int offset;
	if (strcmp(key, "presHighValue")==0){
		offset = offsetof(Element, HighPressure);
		*format = 1;
	} else if (strcmp(key, "presHighType")==0){
		offset = offsetof(Element, HighPressureTransition);
		*format = 0;
	} else if (strcmp(key, "presLowValue")==0){
		offset = offsetof(Element, LowPressure);
		*format = 1;
	} else if (strcmp(key, "presLowType")==0){
		offset = offsetof(Element, LowPressureTransition);
		*format = 0;
	} else if (strcmp(key, "tempHighValue")==0){
		offset = offsetof(Element, HighTemperature);
		*format = 1;
	} else if (strcmp(key, "tempHighType")==0){
		offset = offsetof(Element, HighTemperatureTransition);
		*format = 0;
	} else if (strcmp(key, "tempLowValue")==0){
		offset = offsetof(Element, LowTemperature);
		*format = 1;
	} else if (strcmp(key, "tempLowType")==0){
		offset = offsetof(Element, LowTemperatureTransition);
		*format = 0;
	} else {
		offset = -1;
	}
	return offset;
}
int luacon_transitionread(lua_State* l){
	int format, offset;
	int tempinteger;
	float tempfloat;
	int i;
	char * key = mystrdup((char*)luaL_optstring(l, 2, ""));
	offset = luacon_transition_getproperty(key, &format);
	free(key);

	//Get Raw Index value for element
	lua_pushstring(l, "value");
	lua_rawget(l, 1);

	i = lua_tointeger(l, lua_gettop(l));

	lua_pop(l, 1);

	if(i < 0 || i >= PT_NUM || offset==-1)
	{
		return luaL_error(l, "Invalid property");
	}
	switch(format)
	{
	case 0:
		tempinteger = *((int*)(((unsigned char*)&luacon_sim->elements[i])+offset));
		lua_pushnumber(l, tempinteger);
		break;
	case 1:
		tempfloat = *((float*)(((unsigned char*)&luacon_sim->elements[i])+offset));
		lua_pushnumber(l, tempfloat);
		break;
	}
	return 1;
}
int luacon_transitionwrite(lua_State* l){
	int format, offset;
	int tempinteger;
	float tempfloat;
	int i;
	char * key = mystrdup((char*)luaL_optstring(l, 2, ""));
	offset = luacon_transition_getproperty(key, &format);
	free(key);

	//Get Raw Index value for element
	lua_pushstring(l, "value");
	lua_rawget(l, 1);

	i = lua_tointeger (l, lua_gettop(l));

	lua_pop(l, 1);

	if(i < 0 || i >= PT_NUM || offset==-1)
	{
		return luaL_error(l, "Invalid property");
	}
	switch(format)
	{
	case 0:
		*((int*)(((unsigned char*)&luacon_sim->elements[i])+offset)) = luaL_optinteger(l, 3, 0);
		break;
	case 1:
		*((float*)(((unsigned char*)&luacon_sim->elements[i])+offset)) = luaL_optnumber(l, 3, 0);
		break;
	}
	return 0;
}
int luacon_element_getproperty(char * key, int * format, unsigned int * modified_stuff)
{
	int offset;
	if (strcmp(key, "name")==0){
		offset = offsetof(Element, Name);
		*format = 2;
	}
	else if (strcmp(key, "color")==0){
		offset = offsetof(Element, Colour);
		*format = 0;
		if (modified_stuff)
			*modified_stuff |= LUACON_EL_MODIFIED_GRAPHICS;
	}
	else if (strcmp(key, "colour")==0){
		offset = offsetof(Element, Colour);
		*format = 0;
		if (modified_stuff)
			*modified_stuff |= LUACON_EL_MODIFIED_GRAPHICS;
	}
	else if (strcmp(key, "advection")==0){
		offset = offsetof(Element, Advection);
		*format = 1;
	}
	else if (strcmp(key, "airdrag")==0){
		offset = offsetof(Element, AirDrag);
		*format = 1;
	}
	else if (strcmp(key, "airloss")==0){
		offset = offsetof(Element, AirLoss);
		*format = 1;
	}
	else if (strcmp(key, "loss")==0){
		offset = offsetof(Element, Loss);
		*format = 1;
	}
	else if (strcmp(key, "collision")==0){
		offset = offsetof(Element, Collision);
		*format = 1;
	}
	else if (strcmp(key, "gravity")==0){
		offset = offsetof(Element, Gravity);
		*format = 1;
	}
	else if (strcmp(key, "diffusion")==0){
		offset = offsetof(Element, Diffusion);
		*format = 1;
	}
	else if (strcmp(key, "hotair")==0){
		offset = offsetof(Element, HotAir);
		*format = 1;
	}
	else if (strcmp(key, "falldown")==0){
		offset = offsetof(Element, Falldown);
		*format = 0;
	}
	else if (strcmp(key, "flammable")==0){
		offset = offsetof(Element, Flammable);
		*format = 0;
	}
	else if (strcmp(key, "explosive")==0){
		offset = offsetof(Element, Explosive);
		*format = 0;
	}
	else if (strcmp(key, "meltable")==0){
		offset = offsetof(Element, Meltable);
		*format = 0;
	}
	else if (strcmp(key, "hardness")==0){
		offset = offsetof(Element, Hardness);
		*format = 0;
	}
	else if (strcmp(key, "menu")==0){
		offset = offsetof(Element, MenuVisible);
		*format = 0;
		if (modified_stuff)
			*modified_stuff |= LUACON_EL_MODIFIED_MENUS;
	}
	else if (strcmp(key, "enabled")==0){
		offset = offsetof(Element, Enabled);
		*format = 0;
	}
	else if (strcmp(key, "weight")==0){
		offset = offsetof(Element, Weight);
		*format = 0;
		if (modified_stuff)
			*modified_stuff |= LUACON_EL_MODIFIED_CANMOVE;
	}
	else if (strcmp(key, "menusection")==0){
		offset = offsetof(Element, MenuSection);
		*format = 0;
		if (modified_stuff)
			*modified_stuff |= LUACON_EL_MODIFIED_MENUS;
	}
	else if (strcmp(key, "heat")==0){
		offset = offsetof(Element, Temperature);
		*format = 1;
	}
	else if (strcmp(key, "hconduct")==0){
		offset = offsetof(Element, HeatConduct);
		*format = 3;
	}
	else if (strcmp(key, "state")==0){
		offset = offsetof(Element, State);
		*format = 3;
	}
	else if (strcmp(key, "properties")==0){
		offset = offsetof(Element, Properties);
		*format = 0;
		if (modified_stuff)
			*modified_stuff |= LUACON_EL_MODIFIED_GRAPHICS | LUACON_EL_MODIFIED_CANMOVE;
	}
	else if (strcmp(key, "description")==0){
		offset = offsetof(Element, Description);
		*format = 2;
	}
	else {
		return -1;
	}
	return offset;
}
int luacon_elementread(lua_State* l){
	int format, offset;
	char * tempstring;
	int tempinteger;
	float tempfloat;
	int i;
	char * key = mystrdup((char*)luaL_optstring(l, 2, ""));
	offset = luacon_element_getproperty(key, &format, NULL);
	free(key);

	//Get Raw Index value for element
	lua_pushstring(l, "id");
	lua_rawget(l, 1);

	i = lua_tointeger (l, lua_gettop(l));

	lua_pop(l, 1);

	if(i < 0 || i >= PT_NUM || offset==-1)
	{
		return luaL_error(l, "Invalid property");
	}
	switch(format)
	{
	case 0:
		tempinteger = *((int*)(((unsigned char*)&luacon_sim->elements[i])+offset));
		lua_pushnumber(l, tempinteger);
		break;
	case 1:
		tempfloat = *((float*)(((unsigned char*)&luacon_sim->elements[i])+offset));
		lua_pushnumber(l, tempfloat);
		break;
	case 2:
		tempstring = *((char**)(((unsigned char*)&luacon_sim->elements[i])+offset));
		lua_pushstring(l, tempstring);
		break;
	case 3:
		tempinteger = *((unsigned char*)(((unsigned char*)&luacon_sim->elements[i])+offset));
		lua_pushnumber(l, tempinteger);
		break;
	}
	return 1;
}
int luacon_elementwrite(lua_State* l){
	int format, offset;
	char * tempstring;
	int tempinteger;
	float tempfloat;
	int i;
	unsigned int modified_stuff = 0;
	char * key = mystrdup((char*)luaL_optstring(l, 2, ""));
	offset = luacon_element_getproperty(key, &format, &modified_stuff);

	//Get Raw Index value for element
	lua_pushstring(l, "id");
	lua_rawget(l, 1);

	i = lua_tointeger (l, lua_gettop(l));

	lua_pop(l, 1);

	if(i < 0 || i >= PT_NUM || offset==-1)
	{
		free(key);
		return luaL_error(l, "Invalid property");
	}
	switch(format)
	{
	case 0:
		*((int*)(((unsigned char*)&luacon_sim->elements[i])+offset)) = luaL_optinteger(l, 3, 0);
		break;
	case 1:
		*((float*)(((unsigned char*)&luacon_sim->elements[i])+offset)) = luaL_optnumber(l, 3, 0);
		break;
	case 2:
		tempstring = mystrdup((char*)luaL_optstring(l, 3, ""));
		if(strcmp(key, "name")==0)
		{
			int j = 0;
			//Convert to upper case
			for(j = 0; j < strlen(tempstring); j++)
				tempstring[j] = toupper(tempstring[j]);
			if(strlen(tempstring)>4)
			{
				free(tempstring);
				free(key);
				return luaL_error(l, "Name too long");
			}
			if(luacon_ci->GetParticleType(tempstring) == -1)
			{
				free(tempstring);
				free(key);
				return luaL_error(l, "Name in use");
			}
		}
		*((char**)(((unsigned char*)&luacon_sim->elements[i])+offset)) = tempstring;
		//Need some way of cleaning up previous values
		break;
	case 3:
		*((unsigned char*)(((unsigned char*)&luacon_sim->elements[i])+offset)) = luaL_optinteger(l, 3, 0);
		break;
	}
	if (modified_stuff)
	{
		//if (modified_stuff & LUACON_EL_MODIFIED_MENUS)
			//luacon_model->notifyMenuListChanged();
		if (modified_stuff & LUACON_EL_MODIFIED_CANMOVE)
			luacon_sim->init_can_move();
		if (modified_stuff & LUACON_EL_MODIFIED_GRAPHICS)
			memset(luacon_model->GetRenderer()->graphicscache, 0, sizeof(gcache_item)*PT_NUM);
	}
	free(key);
	return 0;
}
int luacon_keyevent(int key, int modifier, int event){
	int i = 0, kpcontinue = 1;
	char tempkey[] = {key, 0};
	if(keypress_function_count){
		for(i = 0; i < keypress_function_count && kpcontinue; i++){
			lua_rawgeti(luacon_ci->l, LUA_REGISTRYINDEX, keypress_functions[i]);
			lua_pushstring(luacon_ci->l, tempkey);
			lua_pushinteger(luacon_ci->l, key);
			lua_pushinteger(luacon_ci->l, modifier);
			lua_pushinteger(luacon_ci->l, event);
			lua_pcall(luacon_ci->l, 4, 1, 0);
			if(lua_isboolean(luacon_ci->l, -1)){
				kpcontinue = lua_toboolean(luacon_ci->l, -1);
			}
			lua_pop(luacon_ci->l, 1);
		}
	}
	return kpcontinue;
}
int luacon_mouseevent(int mx, int my, int mb, int event, int mouse_wheel){
	int i = 0, mpcontinue = 1;
	if(mouseclick_function_count){
		for(i = 0; i < mouseclick_function_count && mpcontinue; i++){
			lua_rawgeti(luacon_ci->l, LUA_REGISTRYINDEX, mouseclick_functions[i]);
			lua_pushinteger(luacon_ci->l, mx);
			lua_pushinteger(luacon_ci->l, my);
			lua_pushinteger(luacon_ci->l, mb);
			lua_pushinteger(luacon_ci->l, event);
			lua_pushinteger(luacon_ci->l, mouse_wheel);
			lua_pcall(luacon_ci->l, 5, 1, 0);
			if(lua_isboolean(luacon_ci->l, -1)){
				mpcontinue = lua_toboolean(luacon_ci->l, -1);
			}
			lua_pop(luacon_ci->l, 1);
		}
	}
	return mpcontinue;
}
int luacon_step(int mx, int my, int selectl, int selectr, int bsx, int bsy){
	int tempret = 0, tempb, i, callret;
	lua_pushinteger(luacon_ci->l, bsy);
	lua_pushinteger(luacon_ci->l, bsx);
	lua_pushinteger(luacon_ci->l, selectr);
	lua_pushinteger(luacon_ci->l, selectl);
	lua_pushinteger(luacon_ci->l, my);
	lua_pushinteger(luacon_ci->l, mx);
	lua_setfield(luacon_ci->l, tptProperties, "mousex");
	lua_setfield(luacon_ci->l, tptProperties, "mousey");
	lua_setfield(luacon_ci->l, tptProperties, "selectedl");
	lua_setfield(luacon_ci->l, tptProperties, "selectedr");
	lua_setfield(luacon_ci->l, tptProperties, "brushx");
	lua_setfield(luacon_ci->l, tptProperties, "brushy");
	if(step_functions[0]){
		//Set mouse globals
		for(i = 0; i<6; i++){
			if(step_functions[i]){
				lua_rawgeti(luacon_ci->l, LUA_REGISTRYINDEX, step_functions[i]);
				callret = lua_pcall(luacon_ci->l, 0, 0, 0);
				if (callret)
				{
					// failed, TODO: better error reporting
					luacon_ci->Log(CommandInterface::LogError, luacon_geterror());//("%s\n",luacon_geterror());
				}
			}
		}
		return tempret;
	}
	return 0;
}
int luacon_eval(char *command){
	return luaL_dostring (luacon_ci->l, command);
}
int luacon_part_update(int t, int i, int x, int y, int surround_space, int nt)
{
	int retval = 0;
	if(lua_el_func[t]){
		lua_rawgeti(luacon_ci->l, LUA_REGISTRYINDEX, lua_el_func[t]);
		lua_pushinteger(luacon_ci->l, i);
		lua_pushinteger(luacon_ci->l, x);
		lua_pushinteger(luacon_ci->l, y);
		lua_pushinteger(luacon_ci->l, surround_space);
		lua_pushinteger(luacon_ci->l, nt);
		lua_pcall(luacon_ci->l, 5, 1, 0);
		if(lua_isboolean(luacon_ci->l, -1)){
			retval = lua_toboolean(luacon_ci->l, -1);
		}
		lua_pop(luacon_ci->l, 1);
	}
	return retval;
}
char *luacon_geterror(){
	char *error = (char*)lua_tostring(luacon_ci->l, -1);
	if(error==NULL || !error[0]){
		error = "failed to execute";
	}
	return error;
}
/*void luacon_close(){
	lua_close(l);
}*/

//TPT Interface methods
int luatpt_test(lua_State* l)
{
    int testint = 0;
	testint = luaL_optint(l, 1, 0);
	printf("Test successful, got %d\n", testint);
	return 0;
}
int luatpt_getelement(lua_State *l)
{
	int t;
	char * name = (char*)luaL_optstring(l, 1, "dust");
	if ((t = luacon_ci->GetParticleType(name))==-1)
		return luaL_error(l,"Unrecognised element '%s'", name);
	lua_pushinteger(l, t);
	return 1;
}
int luatpt_element_func(lua_State *l)
{
	if(lua_isfunction(l, 1))
	{
		int element = luaL_optint(l, 2, 0);
		int replace = luaL_optint(l, 3, 0);
		int function;
		lua_pushvalue(l, 1);
		function = luaL_ref(l, LUA_REGISTRYINDEX);
		if(element > 0 && element < PT_NUM)
		{
			lua_el_func[element] = function;
			if(replace)
				lua_el_mode[element] = 2;
			else
				lua_el_mode[element] = 1;
			return 0;
		}
		else
		{
			return luaL_error(l, "Invalid element");
		}
	}
	return 0;
}
int luatpt_error(lua_State* l)
{
	/*char *error = "";
	error = mystrdup((char*)luaL_optstring(l, 1, "Error text"));
	if(vid_buf!=NULL){
		error_ui(vid_buf, 0, error);
		free(error);
		return 0;
	}
	free(error);
	return luaL_error(l, "Screen buffer does not exist");*/
	//TODO IMPLEMENT
	return 0;
}
int luatpt_drawtext(lua_State* l)
{
    char *string;
	int textx, texty, textred, textgreen, textblue, textalpha;
	textx = luaL_optint(l, 1, 0);
	texty = luaL_optint(l, 2, 0);
	string = (char*)luaL_optstring(l, 3, "");
	textred = luaL_optint(l, 4, 255);
	textgreen = luaL_optint(l, 5, 255);
	textblue = luaL_optint(l, 6, 255);
	textalpha = luaL_optint(l, 7, 255);
	if (textx<0 || texty<0 || textx>=XRES+BARSIZE || texty>=YRES+MENUSIZE)
		return luaL_error(l, "Screen coordinates out of range (%d,%d)", textx, texty);
	if (textred<0) textred = 0;
	if (textred>255) textred = 255;
	if (textgreen<0) textgreen = 0;
	if (textgreen>255) textgreen = 255;
	if (textblue<0) textblue = 0;
	if (textblue>255) textblue = 255;
	if (textalpha<0) textalpha = 0;
	if (textalpha>255) textalpha = 255;
	if(luacon_g!=NULL){
		luacon_g->drawtext(textx, texty, string, textred, textgreen, textblue, textalpha);
		return 0;
	}
	return luaL_error(l, "Screen buffer does not exist");
}

int luatpt_create(lua_State* l)
{
	int x, y, retid, t = -1;
	char * name;
	x = abs(luaL_optint(l, 1, 0));
	y = abs(luaL_optint(l, 2, 0));
	if(x < XRES && y < YRES){
		if(lua_isnumber(l, 3)){
			t = luaL_optint(l, 3, 0);
			if (t<0 || t >= PT_NUM || !luacon_sim->elements[t].Enabled)
				return luaL_error(l, "Unrecognised element number '%d'", t);
		} else {
			name = (char*)luaL_optstring(l, 3, "dust");
			if ((t = luacon_ci->GetParticleType(std::string(name))) == -1)
				return luaL_error(l,"Unrecognised element '%s'", name);
		}
		retid = luacon_sim->create_part(-1, x, y, t);
		// failing to create a particle often happens (e.g. if space is already occupied) and isn't usually important, so don't raise an error
		lua_pushinteger(l, retid);
		return 1;
	}
	return luaL_error(l, "Coordinates out of range (%d,%d)", x, y);
}

int luatpt_setpause(lua_State* l)
{
	int pausestate;
	pausestate = luaL_optint(l, 1, 0);
	luacon_model->SetPaused(pausestate==0?0:1);
	return 0;
}

int luatpt_togglepause(lua_State* l)
{
	luacon_model->SetPaused(!luacon_model->GetPaused());
	//sys_pause=!sys_pause;
	return 0;
}

int luatpt_togglewater(lua_State* l)
{
	luacon_sim->water_equal_test=!luacon_sim->water_equal_test;
	return 0;
}

int luatpt_setconsole(lua_State* l)
{
	/*int consolestate;
	consolestate = luaL_optint(l, 1, 0);
	console_mode = (consolestate==0?0:1);
	return 0;*/
	//TODO IMPLEMENT
	return 0;
}

int luatpt_log(lua_State* l)
{
	if((*luacon_currentCommand) && !(*luacon_lastError).length())
		(*luacon_lastError) = luaL_optstring(l, 1, "");
	else
		luacon_ci->Log(CommandInterface::LogNotice, luaL_optstring(l, 1, ""));
	return 0;
}

int luatpt_set_pressure(lua_State* l)
{
	int nx, ny;
	int x1, y1, width, height;
	float value;
	x1 = abs(luaL_optint(l, 1, 0));
	y1 = abs(luaL_optint(l, 2, 0));
	width = abs(luaL_optint(l, 3, XRES/CELL));
	height = abs(luaL_optint(l, 4, YRES/CELL));
	value = (float)luaL_optint(l, 5, 0.0f);
	if(value > 256.0f)
		value = 256.0f;
	else if(value < -256.0f)
		value = -256.0f;

	if(x1 > (XRES/CELL)-1)
		x1 = (XRES/CELL)-1;
	if(y1 > (YRES/CELL)-1)
		y1 = (YRES/CELL)-1;
	if(x1+width > (XRES/CELL)-1)
		width = (XRES/CELL)-x1;
	if(y1+height > (YRES/CELL)-1)
		height = (YRES/CELL)-y1;
	for (nx = x1; nx<x1+width; nx++)
		for (ny = y1; ny<y1+height; ny++)
		{
			luacon_sim->pv[ny][nx] = value;
		}
	return 0;
}

int luatpt_set_gravity(lua_State* l)
{
	int nx, ny;
	int x1, y1, width, height;
	float value;
	x1 = abs(luaL_optint(l, 1, 0));
	y1 = abs(luaL_optint(l, 2, 0));
	width = abs(luaL_optint(l, 3, XRES/CELL));
	height = abs(luaL_optint(l, 4, YRES/CELL));
	value = (float)luaL_optint(l, 5, 0.0f);
	if(value > 256.0f)
		value = 256.0f;
	else if(value < -256.0f)
		value = -256.0f;

	if(x1 > (XRES/CELL)-1)
		x1 = (XRES/CELL)-1;
	if(y1 > (YRES/CELL)-1)
		y1 = (YRES/CELL)-1;
	if(x1+width > (XRES/CELL)-1)
		width = (XRES/CELL)-x1;
	if(y1+height > (YRES/CELL)-1)
		height = (YRES/CELL)-y1;
	for (nx = x1; nx<x1+width; nx++)
		for (ny = y1; ny<y1+height; ny++)
		{
			luacon_sim->gravmap[ny*(XRES/CELL)+nx] = value;
		}
	return 0;
}

int luatpt_reset_gravity_field(lua_State* l)
{
	int nx, ny;
	int x1, y1, width, height;
	x1 = abs(luaL_optint(l, 1, 0));
	y1 = abs(luaL_optint(l, 2, 0));
	width = abs(luaL_optint(l, 3, XRES/CELL));
	height = abs(luaL_optint(l, 4, YRES/CELL));
	if(x1 > (XRES/CELL)-1)
		x1 = (XRES/CELL)-1;
	if(y1 > (YRES/CELL)-1)
		y1 = (YRES/CELL)-1;
	if(x1+width > (XRES/CELL)-1)
		width = (XRES/CELL)-x1;
	if(y1+height > (YRES/CELL)-1)
		height = (YRES/CELL)-y1;
	for (nx = x1; nx<x1+width; nx++)
		for (ny = y1; ny<y1+height; ny++)
		{
			luacon_sim->gravx[ny*(XRES/CELL)+nx] = 0;
			luacon_sim->gravy[ny*(XRES/CELL)+nx] = 0;
			luacon_sim->gravp[ny*(XRES/CELL)+nx] = 0;
		}
	return 0;
}

int luatpt_reset_velocity(lua_State* l)
{
	int nx, ny;
	int x1, y1, width, height;
	x1 = abs(luaL_optint(l, 1, 0));
	y1 = abs(luaL_optint(l, 2, 0));
	width = abs(luaL_optint(l, 3, XRES/CELL));
	height = abs(luaL_optint(l, 4, YRES/CELL));
	if(x1 > (XRES/CELL)-1)
		x1 = (XRES/CELL)-1;
	if(y1 > (YRES/CELL)-1)
		y1 = (YRES/CELL)-1;
	if(x1+width > (XRES/CELL)-1)
		width = (XRES/CELL)-x1;
	if(y1+height > (YRES/CELL)-1)
		height = (YRES/CELL)-y1;
	for (nx = x1; nx<x1+width; nx++)
		for (ny = y1; ny<y1+height; ny++)
		{
			luacon_sim->vx[ny][nx] = 0;
			luacon_sim->vy[ny][nx] = 0;
		}
	return 0;
}

int luatpt_reset_spark(lua_State* l)
{
	int i;
	for (i=0; i<NPART; i++)
	{
		if (luacon_sim->parts[i].type==PT_SPRK)
		{
			if (luacon_sim->parts[i].ctype >= 0 && luacon_sim->parts[i].ctype < PT_NUM)
			{
				luacon_sim->parts[i].type = luacon_sim->parts[i].ctype;
				luacon_sim->parts[i].life = 0;
			}
			else
				luacon_sim->kill_part(i);
		}
	}
	return 0;
}

int luatpt_set_property(lua_State* l)
{
	char *prop, *name;
	int r, i, x, y, w, h, t, nx, ny, partsel = 0, acount;
	float f;
	size_t offset;
	acount = lua_gettop(l);
	prop = (char*)luaL_optstring(l, 1, "");
	if(lua_isnumber(l, 3))
		i = abs(luaL_optint(l, 3, -1));
	else
		i = -1;
	if(lua_isnumber(l, 4))
		y = abs(luaL_optint(l, 4, -1));
	else
		y = -1;
	if(lua_isnumber(l, 5))
		w = abs(luaL_optint(l, 5, -1));
	else
		w = -1;
	if(lua_isnumber(l, 6))
		h = abs(luaL_optint(l, 6, -1));
	else
		h = -1;

	CommandInterface::FormatType format;
	offset = luacon_ci->GetPropertyOffset(prop, format);
	if(offset == -1)
		return luaL_error(l, "Invalid property '%s'", prop);
	if(acount>2){
		if(!lua_isnumber(l, acount) && lua_isstring(l, acount)){
			name = (char*)luaL_optstring(l, acount, "none");
			if((partsel = luacon_ci->GetParticleType(std::string(name)))==-1)
				return luaL_error(l, "Unrecognised element '%s'", name);
		}
	}
	if(lua_isnumber(l, 2)){
		if(format == CommandInterface::FormatFloat){
			f = luaL_optnumber(l, 2, 0);
		} else {
			t = luaL_optint(l, 2, 0);
		}
		//TODO Element ID check
		//if (format == 3 && (t<0 || t>=PT_NUM))
		//	return luaL_error(l, "Unrecognised element number '%d'", t);
	} else {
		name = (char*)luaL_optstring(l, 2, "dust");
		//if (!console_parse_type(name, &t, NULL))
		if((t = luacon_ci->GetParticleType(std::string(name)))==-1)
			return luaL_error(l, "Unrecognised element '%s'", name);
	}
	if(i == -1 || (w != -1 && h != -1)){
		// Got a region
		if(i == -1){
			i = 0;
			y = 0;
			w = XRES;
			h = YRES;
		}
		if (i>=XRES || y>=YRES)
			return luaL_error(l, "Coordinates out of range (%d,%d)", i, y);
		x = i;
		if(x+w > XRES)
			w = XRES-x;
		if(y+h > YRES)
			h = YRES-y;
		for (nx = x; nx<x+w; nx++)
			for (ny = y; ny<y+h; ny++){
				r = luacon_sim->pmap[ny][nx];
				if (!r || (partsel && partsel != luacon_sim->parts[r>>8].type))
				{
					r = luacon_sim->photons[ny][nx];
					if (!r || (partsel && partsel != luacon_sim->parts[r>>8].type))
						continue;
				}
				i = r>>8;
				if(format == CommandInterface::FormatFloat){
					*((float*)(((unsigned char*)&luacon_sim->parts[i])+offset)) = f;
				} else {
					*((int*)(((unsigned char*)&luacon_sim->parts[i])+offset)) = t;
				}
			}
	} else {
		// Got coords or particle index
		if(i != -1 && y != -1){
			if (i>=XRES || y>=YRES)
				return luaL_error(l, "Coordinates out of range (%d,%d)", i, y);
			r = luacon_sim->pmap[y][i];
			if (!r || (partsel && partsel != luacon_sim->parts[r>>8].type))
				r = luacon_sim->photons[y][i];
			if (!r || (partsel && partsel != luacon_sim->parts[r>>8].type))
				return 0;
			i = r>>8;
		}
		if (i < 0 || i >= NPART)
			return luaL_error(l, "Invalid particle ID '%d'", i);
		if (!luacon_sim->parts[i].type)
			return 0;
		if (partsel && partsel != luacon_sim->parts[i].type)
			return 0;
		if(format == CommandInterface::FormatFloat){
			*((float*)(((unsigned char*)&luacon_sim->parts[i])+offset)) = f;
		} else {
			*((int*)(((unsigned char*)&luacon_sim->parts[i])+offset)) = t;
		}
	}
	return 0;
}

int luatpt_set_wallmap(lua_State* l)
{
	int nx, ny, acount;
	int x1, y1, width, height;
	float value;
	acount = lua_gettop(l);

	x1 = abs(luaL_optint(l, 1, 0));
	y1 = abs(luaL_optint(l, 2, 0));
	width = abs(luaL_optint(l, 3, XRES/CELL));
	height = abs(luaL_optint(l, 4, YRES/CELL));
	value = (float)luaL_optint(l, acount, 0);

	if(acount==5)	//Draw rect
	{
		if(x1 > (XRES/CELL))
			x1 = (XRES/CELL);
		if(y1 > (YRES/CELL))
			y1 = (YRES/CELL);
		if(x1+width > (XRES/CELL))
			width = (XRES/CELL)-x1;
		if(y1+height > (YRES/CELL))
			height = (YRES/CELL)-y1;
		for (nx = x1; nx<x1+width; nx++)
			for (ny = y1; ny<y1+height; ny++)
			{
				luacon_sim->bmap[ny][nx] = value;
			}
	}
	else	//Set point
	{
		if(x1 > (XRES/CELL))
			x1 = (XRES/CELL);
		if(y1 > (YRES/CELL))
			y1 = (YRES/CELL);
		luacon_sim->bmap[y1][x1] = value;
	}
	return 0;
}

int luatpt_get_wallmap(lua_State* l)
{
	int nx, ny, acount;
	int x1, y1, width, height;
	float value;
	acount = lua_gettop(l);

	x1 = abs(luaL_optint(l, 1, 0));
	y1 = abs(luaL_optint(l, 2, 0));

	if(x1 > (XRES/CELL) || y1 > (YRES/CELL))
		return luaL_error(l, "Out of range");

	lua_pushinteger(l, luacon_sim->bmap[y1][x1]);
	return 1;
}

int luatpt_set_elecmap(lua_State* l)
{
	int nx, ny, acount;
	int x1, y1, width, height;
	float value;
	acount = lua_gettop(l);

	x1 = abs(luaL_optint(l, 1, 0));
	y1 = abs(luaL_optint(l, 2, 0));
	width = abs(luaL_optint(l, 3, XRES/CELL));
	height = abs(luaL_optint(l, 4, YRES/CELL));
	value = (float)luaL_optint(l, acount, 0);

	if(acount==5)	//Draw rect
	{
		if(x1 > (XRES/CELL))
			x1 = (XRES/CELL);
		if(y1 > (YRES/CELL))
			y1 = (YRES/CELL);
		if(x1+width > (XRES/CELL))
			width = (XRES/CELL)-x1;
		if(y1+height > (YRES/CELL))
			height = (YRES/CELL)-y1;
		for (nx = x1; nx<x1+width; nx++)
			for (ny = y1; ny<y1+height; ny++)
			{
				luacon_sim->emap[ny][nx] = value;
			}
	}
	else	//Set point
	{
		if(x1 > (XRES/CELL))
			x1 = (XRES/CELL);
		if(y1 > (YRES/CELL))
			y1 = (YRES/CELL);
		luacon_sim->emap[y1][x1] = value;
	}
	return 0;
}

int luatpt_get_elecmap(lua_State* l)
{
	int nx, ny, acount;
	int x1, y1, width, height;
	float value;
	acount = lua_gettop(l);

	x1 = abs(luaL_optint(l, 1, 0));
	y1 = abs(luaL_optint(l, 2, 0));

	if(x1 > (XRES/CELL) || y1 > (YRES/CELL))
		return luaL_error(l, "Out of range");

	lua_pushinteger(l, luacon_sim->emap[y1][x1]);
	return 1;
}

int luatpt_get_property(lua_State* l)
{
	int i, r, y;
	char *prop;
	prop = (char*)luaL_optstring(l, 1, "");
	i = luaL_optint(l, 2, 0);
	y = luaL_optint(l, 3, -1);
	if(y!=-1 && y < YRES && y >= 0 && i < XRES && i >= 0){
		r = luacon_sim->pmap[y][i];
		if (!r)
			r = luacon_sim->photons[y][i];
		if (!r)
		{
			if (strcmp(prop,"type")==0){
				lua_pushinteger(l, 0);
				return 1;
			}
			return luaL_error(l, "Particle does not exist");
		}
		i = r>>8;
	}
	else if (y!=-1)
		return luaL_error(l, "Coordinates out of range (%d,%d)", i, y);
	if (i < 0 || i >= NPART)
		return luaL_error(l, "Invalid particle ID '%d'", i);
	if (luacon_sim->parts[i].type)
	{
		//TODO: Use particle_getproperty
		if (strcmp(prop,"type")==0){
			lua_pushinteger(l, luacon_sim->parts[i].type);
			return 1;
		}
		if (strcmp(prop,"life")==0){
			lua_pushinteger(l, luacon_sim->parts[i].life);
			return 1;
		}
		if (strcmp(prop,"ctype")==0){
			lua_pushinteger(l, luacon_sim->parts[i].ctype);
			return 1;
		}
		if (strcmp(prop,"temp")==0){
			lua_pushnumber(l, luacon_sim->parts[i].temp);
			return 1;
		}
		if (strcmp(prop,"tmp")==0){
			lua_pushinteger(l, luacon_sim->parts[i].tmp);
			return 1;
		}
		if (strcmp(prop,"tmp2")==0){
			lua_pushinteger(l, luacon_sim->parts[i].tmp2);
			return 1;
		}
		if (strcmp(prop,"vy")==0){
			lua_pushnumber(l, (double)luacon_sim->parts[i].vy);
			return 1;
		}
		if (strcmp(prop,"vx")==0){
			lua_pushnumber(l, (double)luacon_sim->parts[i].vx);
			return 1;
		}
		if (strcmp(prop,"x")==0){
			lua_pushnumber(l, luacon_sim->parts[i].x);
			return 1;
		}
		if (strcmp(prop,"y")==0){
			lua_pushnumber(l, luacon_sim->parts[i].y);
			return 1;
		}
		if (strcmp(prop,"dcolour")==0){
			lua_pushinteger(l, luacon_sim->parts[i].dcolour);
			return 1;
		}
		if (strcmp(prop,"id")==0){
			lua_pushnumber(l, i);
			return 1;
		}
	}
	else if (strcmp(prop,"type")==0){
		lua_pushinteger(l, 0);
		return 1;
	}
	return luaL_error(l, "Particle does not exist");
}

int luatpt_drawpixel(lua_State* l)
{
	/*int x, y, r, g, b, a;
	x = luaL_optint(l, 1, 0);
	y = luaL_optint(l, 2, 0);
	r = luaL_optint(l, 3, 255);
	g = luaL_optint(l, 4, 255);
	b = luaL_optint(l, 5, 255);
	a = luaL_optint(l, 6, 255);

	if (x<0 || y<0 || x>=XRES+BARSIZE || y>=YRES+MENUSIZE)
		return luaL_error(l, "Screen coordinates out of range (%d,%d)", x, y);
	if (r<0) r = 0;
	if (r>255) r = 255;
	if (g<0) g = 0;
	if (g>255) g = 255;
	if (b<0) b = 0;
	if (b>255) b = 255;
	if (a<0) a = 0;
	if (a>255) a = 255;
	if (luacon_g->vid!=NULL)
	{
		luacon_g->drawpixel(x, y, r, g, b, a);
		return 0;
	}*/
	return luaL_error(l, "Deprecated");
}

int luatpt_drawrect(lua_State* l)
{
	int x, y, w, h, r, g, b, a;
	x = luaL_optint(l, 1, 0);
	y = luaL_optint(l, 2, 0);
	w = luaL_optint(l, 3, 10);
	h = luaL_optint(l, 4, 10);
	r = luaL_optint(l, 5, 255);
	g = luaL_optint(l, 6, 255);
	b = luaL_optint(l, 7, 255);
	a = luaL_optint(l, 8, 255);

	if (x<0 || y<0 || x>=XRES+BARSIZE || y>=YRES+MENUSIZE)
		return luaL_error(l, "Screen coordinates out of range (%d,%d)", x, y);
	if(x+w > XRES+BARSIZE)
		w = XRES+BARSIZE-x;
	if(y+h > YRES+MENUSIZE)
		h = YRES+MENUSIZE-y;
	if (r<0) r = 0;
	if (r>255) r = 255;
	if (g<0) g = 0;
	if (g>255) g = 255;
	if (b<0) b = 0;
	if (b>255) b = 255;
	if (a<0) a = 0;
	if (a>255) a = 255;
	luacon_g->drawrect(x, y, w, h, r, g, b, a);
	return luaL_error(l, "Screen buffer does not exist");
}

int luatpt_fillrect(lua_State* l)
{
	int x,y,w,h,r,g,b,a;
	x = luaL_optint(l, 1, 0);
	y = luaL_optint(l, 2, 0);
	w = luaL_optint(l, 3, 10);
	h = luaL_optint(l, 4, 10);
	r = luaL_optint(l, 5, 255);
	g = luaL_optint(l, 6, 255);
	b = luaL_optint(l, 7, 255);
	a = luaL_optint(l, 8, 255);

	if (x<0 || y<0 || x>=XRES+BARSIZE || y>=YRES+MENUSIZE)
		return luaL_error(l, "Screen coordinates out of range (%d,%d)", x, y);
	if(x+w > XRES+BARSIZE)
		w = XRES+BARSIZE-x;
	if(y+h > YRES+MENUSIZE)
		h = YRES+MENUSIZE-y;
	if (r<0) r = 0;
	if (r>255) r = 255;
	if (g<0) g = 0;
	if (g>255) g = 255;
	if (b<0) b = 0;
	if (b>255) b = 255;
	if (a<0) a = 0;
	if (a>255) a = 255;
	luacon_g->fillrect(x, y, w, h, r, g, b, a);
	return luaL_error(l, "Screen buffer does not exist");
}

int luatpt_drawline(lua_State* l)
{
	int x1,y1,x2,y2,r,g,b,a;
	x1 = luaL_optint(l, 1, 0);
	y1 = luaL_optint(l, 2, 0);
	x2 = luaL_optint(l, 3, 10);
	y2 = luaL_optint(l, 4, 10);
	r = luaL_optint(l, 5, 255);
	g = luaL_optint(l, 6, 255);
	b = luaL_optint(l, 7, 255);
	a = luaL_optint(l, 8, 255);

	//Don't need to check coordinates, as they are checked in blendpixel
	if (r<0) r = 0;
	if (r>255) r = 255;
	if (g<0) g = 0;
	if (g>255) g = 255;
	if (b<0) b = 0;
	if (b>255) b = 255;
	if (a<0) a = 0;
	if (a>255) a = 255;
	luacon_g->draw_line(x1, y1, x2, y2, r, g, b, a);
	return luaL_error(l, "Screen buffer does not exist");
}

int luatpt_textwidth(lua_State* l)
{
	char * string;
	int strwidth = 0;
	string = (char*)luaL_optstring(l, 1, "");
	strwidth = Graphics::textwidth(string);
	lua_pushinteger(l, strwidth);
	return 1;
}

int luatpt_get_name(lua_State* l)
{
	if (luacon_model->GetUser().ID){
		lua_pushstring(l, luacon_model->GetUser().Username.c_str());
		return 1;
	}
	lua_pushstring(l, "");
	return 1;
}

int luatpt_set_shortcuts(lua_State* l)
{
	/*int state;
	state = luaL_optint(l, 1, 0);
	sys_shortcuts = (state==0?0:1);
	return 0;*/
	return luaL_error(l, "set_shortcuts: deprecated");
}

int luatpt_delete(lua_State* l)
{
	int arg1, arg2;
	arg1 = abs(luaL_optint(l, 1, 0));
	arg2 = luaL_optint(l, 2, -1);
	if(arg2 == -1 && arg1 < NPART){
		luacon_sim->kill_part(arg1);
		return 0;
	}
	arg2 = abs(arg2);
	if(arg2 < YRES && arg1 < XRES){
		luacon_sim->delete_part(arg1, arg2, 0);
		return 0;
	}
	return luaL_error(l,"Invalid coordinates or particle ID");
}

int luatpt_register_step(lua_State* l)
{
	int ref, i, ifree = -1;
	if(lua_isfunction(l, 1)){
		for(i = 0; i<6; i++){
			if(!step_functions[i]){
				if (ifree<0) ifree = i;
			} else {
				lua_rawgeti(l, LUA_REGISTRYINDEX, step_functions[i]);
				if(lua_equal(l, 1, lua_gettop(l))){
					lua_pop(l, 1);
					return luaL_error(l, "Function already registered");
				}
				lua_pop(l, 1);
			}
		}
		if (ifree>=0)
		{
			ref = luaL_ref(l, LUA_REGISTRYINDEX);
			step_functions[ifree] = ref;
			return 0;
		}
		else return luaL_error(l, "Step function limit reached");
	}
	return 0;
}
int luatpt_unregister_step(lua_State* l)
{
	int i;
	if(lua_isfunction(l, 1)){
		for(i = 0; i<6; i++){
			if (step_functions[i]){
				lua_rawgeti(l, LUA_REGISTRYINDEX, step_functions[i]);
				if(lua_equal(l, 1, lua_gettop(l))){
					lua_pop(l, 1);
					luaL_unref(l, LUA_REGISTRYINDEX, step_functions[i]);
					step_functions[i] = 0;
				}
				else lua_pop(l, 1);
			}
		}
	}
	return 0;
}
int luatpt_register_keypress(lua_State* l)
{
	int *newfunctions, i;
	if(lua_isfunction(l, 1)){
		for(i = 0; i<keypress_function_count; i++){
			lua_rawgeti(l, LUA_REGISTRYINDEX, keypress_functions[i]);
			if(lua_equal(l, 1, lua_gettop(l))){
				lua_pop(l, 1);
				return luaL_error(l, "Function already registered");
			}
			lua_pop(l, 1);
		}
		newfunctions = (int*)calloc(keypress_function_count+1, sizeof(int));
		if(keypress_functions){
			memcpy(newfunctions, keypress_functions, keypress_function_count*sizeof(int));
			free(keypress_functions);
		}
		newfunctions[keypress_function_count] = luaL_ref(l, LUA_REGISTRYINDEX);
		keypress_function_count++;
		keypress_functions = newfunctions;
	}
	return 0;
}
int luatpt_unregister_keypress(lua_State* l)
{
	int *newfunctions, i, functionindex = -1;
	if(lua_isfunction(l, 1)){
		for(i = 0; i<keypress_function_count; i++){
			lua_rawgeti(l, LUA_REGISTRYINDEX, keypress_functions[i]);
			if(lua_equal(l, 1, lua_gettop(l))){
				functionindex = i;
			}
			lua_pop(l, 1);
		}
	}
	if(functionindex != -1){
		luaL_unref(l, LUA_REGISTRYINDEX, keypress_functions[functionindex]);
		if(functionindex != keypress_function_count-1){
			memmove(keypress_functions+functionindex+1, keypress_functions+functionindex+1, (keypress_function_count-functionindex-1)*sizeof(int));
		}
		if(keypress_function_count-1 > 0){
			newfunctions = (int*)calloc(keypress_function_count-1, sizeof(int));
			memcpy(newfunctions, keypress_functions, (keypress_function_count-1)*sizeof(int));
			free(keypress_functions);
			keypress_functions = newfunctions;
		} else {
			free(keypress_functions);
			keypress_functions = NULL;
		}
		keypress_function_count--;
	} else {
		return luaL_error(l, "Function not registered");
	}
	return 0;
}
int luatpt_register_mouseclick(lua_State* l)
{
	int *newfunctions, i;
	if(lua_isfunction(l, 1)){
		for(i = 0; i<mouseclick_function_count; i++){
			lua_rawgeti(l, LUA_REGISTRYINDEX, mouseclick_functions[i]);
			if(lua_equal(l, 1, lua_gettop(l))){
				lua_pop(l, 1);
				return luaL_error(l, "Function already registered");
			}
			lua_pop(l, 1);
		}
		newfunctions = (int*)calloc(mouseclick_function_count+1, sizeof(int));
		if(mouseclick_functions){
			memcpy(newfunctions, mouseclick_functions, mouseclick_function_count*sizeof(int));
			free(mouseclick_functions);
		}
		newfunctions[mouseclick_function_count] = luaL_ref(l, LUA_REGISTRYINDEX);
		mouseclick_function_count++;
		mouseclick_functions = newfunctions;
	}
	return 0;
}
int luatpt_unregister_mouseclick(lua_State* l)
{
	int *newfunctions, i, functionindex = -1;
	if(lua_isfunction(l, 1)){
		for(i = 0; i<mouseclick_function_count; i++){
			lua_rawgeti(l, LUA_REGISTRYINDEX, mouseclick_functions[i]);
			if(lua_equal(l, 1, lua_gettop(l))){
				functionindex = i;
			}
			lua_pop(l, 1);
		}
	}
	if(functionindex != -1){
		luaL_unref(l, LUA_REGISTRYINDEX, mouseclick_functions[functionindex]);
		if(functionindex != mouseclick_function_count-1){
			memmove(mouseclick_functions+functionindex+1, mouseclick_functions+functionindex+1, (mouseclick_function_count-functionindex-1)*sizeof(int));
		}
		if(mouseclick_function_count-1 > 0){
			newfunctions = (int*)calloc(mouseclick_function_count-1, sizeof(int));
			memcpy(newfunctions, mouseclick_functions, (mouseclick_function_count-1)*sizeof(int));
			free(mouseclick_functions);
			mouseclick_functions = newfunctions;
		} else {
			free(mouseclick_functions);
			mouseclick_functions = NULL;
		}
		mouseclick_function_count--;
	} else {
		return luaL_error(l, "Function not registered");
	}
	return 0;
}
int luatpt_input(lua_State* l)
{
	/*char *prompt, *title, *result, *shadow, *text;
	title = mystrdup((char*)luaL_optstring(l, 1, "Title"));
	prompt = mystrdup((char*)luaL_optstring(l, 2, "Enter some text:"));
	text = mystrdup((char*)luaL_optstring(l, 3, ""));
	shadow = mystrdup((char*)luaL_optstring(l, 4, ""));

	if (vid_buf!=NULL)
	{
		result = input_ui(vid_buf, title, prompt, text, shadow);
		lua_pushstring(l, result);
		free(result);
		free(title);
		free(prompt);
		free(text);
		free(shadow);
		return 1;
	}
	free(title);
	free(prompt);
	free(text);
	free(shadow);
	return luaL_error(l, "Screen buffer does not exist");*/
	//TODO IMPLEMENT
	return 0;
}
int luatpt_message_box(lua_State* l)
{
	/*char *title, *text;
	title = mystrdup(luaL_optstring(l, 1, "Title"));
	text = mystrdup(luaL_optstring(l, 2, "Message"));
	if (vid_buf!=NULL)
	{
		info_ui(vid_buf, title, text);
		free(title);
		free(text);
		return 0;
	}
	free(title);
	free(text);
	return luaL_error(l, "Screen buffer does not exist");;*/
	//TODO IMPLEMENT
	return 0;
}
int luatpt_get_numOfParts(lua_State* l)
{
    lua_pushinteger(l, luacon_sim->parts_lastActiveIndex);
    return 1;
}
int luatpt_start_getPartIndex(lua_State* l)
{
    getPartIndex_curIdx = -1;
    return 1;
}
int luatpt_next_getPartIndex(lua_State* l)
{
    while(1)
    {
        getPartIndex_curIdx++;
        if(getPartIndex_curIdx >= NPART)
        {
            getPartIndex_curIdx = 0;
            lua_pushboolean(l, 0);
            return 1;
        }
        if(luacon_sim->parts[getPartIndex_curIdx].type)
            break;

    }

    lua_pushboolean(l, 1);
    return 1;
}
int luatpt_getPartIndex(lua_State* l)
{
    if(getPartIndex_curIdx < 0)
    {
        lua_pushinteger(l, 0);
        return 1;
    }
    lua_pushinteger(l, getPartIndex_curIdx);
    return 1;
}
int luatpt_hud(lua_State* l)
{
    /*int hudstate;
    hudstate = luaL_optint(l, 1, 0);
    hud_enable = (hudstate==0?0:1);
    return 0;*/
	//TODO IMPLEMENT
	return 0;
}
int luatpt_gravity(lua_State* l)
{
	//luacon_sim->
    /*int gravstate;
    gravstate = luaL_optint(l, 1, 0);
    if(gravstate)
        start_grav_async();
    else
        stop_grav_async();
    ngrav_enable = (gravstate==0?0:1);*/
	//TODO IMPLEMENT
    return 0;
}
int luatpt_airheat(lua_State* l)
{
    int aheatstate;
    aheatstate = luaL_optint(l, 1, 0);
    luacon_sim->aheat_enable = (aheatstate==0?0:1);
    return 0;
}
int luatpt_active_menu(lua_State* l)
{
    int menuid;
    menuid = luaL_optint(l, 1, -1);
    if (menuid < SC_TOTAL && menuid >= 0)
    	luacon_model->SetActiveMenu(luacon_model->GetMenuList()[menuid]);
    else
        return luaL_error(l, "Invalid menu");
    return 0;
}
int luatpt_decorations_enable(lua_State* l)
{
	int decostate;
	decostate = luaL_optint(l, 1, 0);
	luacon_model->SetDecoration(decostate==0?false:true);
	return 0;
}

int luatpt_heat(lua_State* l)
{
	int heatstate;
	heatstate = luaL_optint(l, 1, 0);
	luacon_sim->legacy_enable = (heatstate==1?0:1);
	return 0;
}
int luatpt_cmode_set(lua_State* l)
{
	//TODO IMPLEMENT
    return luaL_error(l, "Not implemented");
}
int luatpt_setfire(lua_State* l)
{
	int firesize = luaL_optint(l, 2, 4);
	float fireintensity = (float)luaL_optnumber(l, 1, 1.0f);
	luacon_model->GetRenderer()->prepare_alpha(firesize, fireintensity);
	return 0;
}
int luatpt_setdebug(lua_State* l)
{
	/*int debug = luaL_optint(l, 1, 0);
	debug_flags = debug;
	return 0;*/
	//TODO IMPLEMENT
	return luaL_error(l, "Not implemented");
}
int luatpt_setfpscap(lua_State* l)
{
	int fpscap = luaL_optint(l, 1, 0);
	if (fpscap < 2)
		return luaL_error(l, "fps cap too small");
	ui::Engine::Ref().FpsLimit = fpscap;
	return 0;
}
int luatpt_getscript(lua_State* l)
{
	/*char *fileid = NULL, *filedata = NULL, *fileuri = NULL, *fileauthor = NULL, *filename = NULL, *lastError = NULL, *luacommand = NULL;
	int len, ret,run_script;
	FILE * outputfile;

	fileauthor = mystrdup(luaL_optstring(l, 1, ""));
	fileid = mystrdup(luaL_optstring(l, 2, ""));
	run_script = luaL_optint(l, 3, 0);
	if(!fileauthor || !fileid || strlen(fileauthor)<1 || strlen(fileid)<1)
		goto fin;
	if(!confirm_ui(vid_buf, "Do you want to install script?", fileid, "Install"))
		goto fin;

	fileuri = malloc(strlen(SCRIPTSERVER)+strlen(fileauthor)+strlen(fileid)+44);
	sprintf(fileuri, "http://" SCRIPTSERVER "/GetScript.api?Author=%s&Filename=%s", fileauthor, fileid);

	filedata = http_auth_get(fileuri, svf_user_id, NULL, svf_session_id, &ret, &len);

	if(len <= 0 || !filedata)
	{
		lastError = "Server did not return data.";
		goto fin;
	}
	if(ret != 200)
	{
		lastError = http_ret_text(ret);
		goto fin;
	}

	filename = (char*)malloc(strlen(fileauthor)+strlen(fileid)+strlen(PATH_SEP)+strlen(LOCAL_LUA_DIR)+6);
	sprintf(filename, LOCAL_LUA_DIR PATH_SEP "%s_%s.lua", fileauthor, fileid);

#ifdef WIN
	_mkdir(LOCAL_LUA_DIR);
#else
	mkdir(LOCAL_LUA_DIR, 0755);
#endif

	outputfile = fopen(filename, "r");
	if(outputfile)
	{
		fclose(outputfile);
		outputfile = NULL;
		if(confirm_ui(vid_buf, "File already exists, overwrite?", filename, "Overwrite"))
		{
			outputfile = fopen(filename, "w");
		}
		else
		{
			goto fin;
		}
	}
	else
	{
		outputfile = fopen(filename, "w");
	}

	if(!outputfile)
	{
		lastError = "Unable to write to file";
		goto fin;
	}


	fputs(filedata, outputfile);
	fclose(outputfile);
	outputfile = NULL;
	if(run_script)
	{
		luacommand = malloc(strlen(filename)+20);
		sprintf(luacommand,"dofile(\"%s\")",filename);
		luaL_dostring (l, luacommand);
    }

fin:
	if(fileid) free(fileid);
	if(filedata) free(filedata);
	if(fileuri) free(fileuri);
	if(fileauthor) free(fileauthor);
	if(filename) free(filename);
	if(luacommand) free(luacommand);
	luacommand = NULL;

	if(lastError) return luaL_error(l, lastError);
	return 0;*/
	//TODO IMPLEMENT
	return luaL_error(l, "Not implemented");
}

int luatpt_setwindowsize(lua_State* l)
{
	/*int result, scale = luaL_optint(l,1,1), kiosk = luaL_optint(l,2,0);
	if (scale!=2) scale = 1;
	if (kiosk!=1) kiosk = 0;
	result = set_scale(scale, kiosk);
	lua_pushnumber(l, result);
	return 1;*/
	//TODO Implement
	return luaL_error(l, "Not implemented");
}

int luatpt_screenshot(lua_State* l)
{
	//TODO Implement
	/*int captureUI = luaL_optint(l, 1, 0);
	if(vid_buf)
	{
		if(captureUI)
		{
			dump_frame(vid_buf, XRES+BARSIZE, YRES+MENUSIZE, XRES+BARSIZE);
		}
		else
		{
			dump_frame(vid_buf, XRES, YRES, XRES+BARSIZE);
		}
		return 0;
	}*/
	return luaL_error(l, "Not implemented");
}

