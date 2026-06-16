#include "bindings-math.h"

#include "../luaapi/context.h"
#include "../luaapi/types.h"
#include "../luaapi/macros.h"

#include "CVec.h"
#include "util/angle.h"
#include "util/math_func.h"
#include "CodeAttributes.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <iostream>

using std::cerr;
using std::endl;
#include <boost/lexical_cast.hpp>
#include <boost/bind/bind.hpp>
using boost::lexical_cast;

namespace LuaBindings
{

INLINE lua_Number luaL_checknumber(lua_State *L, int narg)
{
	lua_Number d = lua_tonumber(L, narg);
	//if(d == 0 && !lua_isnumber(L, narg))
	//	; // TODO: tag_error(L, narg, LUA_TNUMBER);
	return d;
}

/*! sqrt(n)

	Returns the squareroot of n.
*/
int l_sqrt(lua_State* L)
{
	lua_pushnumber(L, sqrt(luaL_checknumber(L, 1)));
	return 1;
}

/*! abs(n)

	Returns the absolute value of n.
*/
int l_abs(lua_State* L)
{
	lua_pushnumber(L, fabs(luaL_checknumber(L, 1)));
	return 1;
}

/*! floor(n)

	Returns the number n rounded down towards infinity.
*/
int l_floor(lua_State* L)
{
	lua_pushnumber(L, floor(luaL_checknumber(L, 1)));
	return 1;
}

int l_round(lua_State* L)
{
	int d = lua_tointeger(L, 2);
	char buffer[256];
	sprintf(buffer, "%.*f", d, lua_tonumber(L, 1));
	lua_pushstring(L, buffer);
	return 1;
}

/*! randomint(l, u)

	Returns a random integer in the interval [l, u].
*/
int l_randomint(lua_State* L)
{
	int l = lua_tointeger(L, 1);
	int u = lua_tointeger(L, 2);
	
	//lua_pushnumber(L, l + (unsigned int)(rndgen()) % (u - l + 1));
	lua_pushinteger(L, l + rndInt(u - l + 1));
	
	return 1;
}

/*! randomfloat(l, u)

	Returns a random floating point number in the interval [l, u].
*/
int l_randomfloat(lua_State* L)
{
	lua_Number l = luaL_checknumber(L, 1);
	lua_Number u = luaL_checknumber(L, 2);
	
	lua_pushnumber(L, l + rnd() * (u - l));
	
	return 1;
}

/*! to_time_string(v)

	Converts a frame count //v// to a string showing hours, minutes and seconds as HH:MM:SS.
*/
int l_toTimeString(lua_State* L)
{
	lua_Integer v = lua_tointeger(L, 1);
	
	lua_Integer sec = v / 100;
	
	char c[8];
	c[0] = ((sec / 36000) % 10) + '0';
	c[1] = ((sec / 3600) % 10) + '0';
	c[2] = ':';
	c[3] = ((sec / 600) % 6) + '0';
	c[4] = ((sec / 60) % 10) + '0';
	c[5] = ':';
	c[6] = ((sec / 10) % 6) + '0';
	c[7] = (sec % 10) + '0';
	lua_pushlstring(L, c, 8);
	
	return 1;
}

/*! vector_diff(x1, y1, x2, y2)

	Returns a tuple equal to (x2 - x1, y2 - y1)
*/
int l_vector_diff(lua_State* L)
{
	lua_pushnumber(L, lua_tonumber(L, 3) - lua_tonumber(L, 1));
	lua_pushnumber(L, lua_tonumber(L, 4) - lua_tonumber(L, 2));
	
	return 2;
}

/*! vector_distance(x1, y1, x2, y2)

	Returns the distance from (x1, y1) to (x2, y2)
*/
int l_vector_distance(lua_State* L)
{
	lua_Number vx = lua_tonumber(L, 3) - lua_tonumber(L, 1);
	lua_Number vy = lua_tonumber(L, 4) - lua_tonumber(L, 2);
	vx *= vx;
	vy *= vy;
	lua_pushnumber(L, sqrt(vx + vy));
	
	return 1;
}

/*! vector_direction(x1, y1, x2, y2)

	Returns the angle between (x1, y1) and (x2, y2)
	in degrees.
*/
int l_vector_direction(lua_State* L)
{
	lua_Number vx = lua_tonumber(L, 3) - lua_tonumber(L, 1);
	lua_Number vy = lua_tonumber(L, 4) - lua_tonumber(L, 2);

	lua_pushnumber(L, rad2deg((float)atan2(vx, -vy)) );
	
	return 1;
}

/*! vector_add(x1, y1, x2, y2)

	Returns a tuple equal to (x1 + x2, y1 + y2)
*/
int l_vector_add(lua_State* L)
{
	lua_pushnumber(L, lua_tonumber(L, 1) + lua_tonumber(L, 3));
	lua_pushnumber(L, lua_tonumber(L, 2) + lua_tonumber(L, 4));
	
	return 2;
}


/*! angle_diff(a, b)

	Returns the relative angle in (-180, 180) between
	angle //a// and //b// such that (a + angle_diff(a, b)) = b (mod 360)
*/
int l_angle_diff(lua_State* L)
{
	AngleDiff diff(AngleDiff(lua_tonumber(L, 1)).relative(AngleDiff(lua_tonumber(L, 2))));
	
	lua_pushnumber(L, diff.toDeg());
	
	return 1;
}

/*! angle_clamp(angle)

	Returns //angle// normalized to [0, 360).
*/
int l_angle_clamp(lua_State* L)
{
	//lua_Number ang = lua_tonumber(L, 1);
	Angle ang((double)lua_tonumber(L, 1));
	
	ang.clamp();
	
	lua_pushnumber(L, ang.toDeg());
	
	return 1;
}

/*! angle_vector(angle[, length = 1])

	Returns a tuple representing the angle
	with length //length//.
*/
int l_angle_vector(lua_State* L)
{
	Angle ang((double)lua_tonumber(L, 1));
	lua_Number len = 1.0;
	
	if(lua_gettop(L) >= 2)
		len = lua_tonumber(L, 2);
	
	Vec vec(ang, len);
	lua_pushnumber(L, vec.x);
	lua_pushnumber(L, vec.y);
	
	return 2;
}

void initMath(LuaContext& context)
{
	context.functions()
		("sqrt", l_sqrt)
		("abs", l_abs)
		("floor", l_floor)
		
		("round", l_round)
	
		("randomint", l_randomint)
		("randomfloat", l_randomfloat)
		
		("to_time_string", l_toTimeString)
	
		("vector_diff", l_vector_diff)
		("vector_distance", l_vector_distance)
		("vector_direction", l_vector_direction)
		("vector_add", l_vector_add)
	
		("angle_clamp", l_angle_clamp)
		("angle_diff", l_angle_diff)
		("angle_vector", l_angle_vector)
	;
}

}
