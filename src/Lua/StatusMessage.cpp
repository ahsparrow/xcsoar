/*
Copyright_License {

  XCSoar Glide Computer - http://www.xcsoar.org/
  Copyright (C) 2000-2016 The XCSoar Project
  A detailed list of copyright holders can be found in the file "AUTHORS".

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
}
*/

#include "StatusMessage.hpp"
#include "Util.hpp"
#include "Util/StringAPI.hxx"
#include "../StatusMessage.hpp"

extern "C" {
#include <lauxlib.h>
}

static int
l_statusmessage_index(lua_State *L)
{
  const char *name = lua_tostring(L, 2);
  if (name == nullptr) {
    return 0;
  } else if (StringIsEqual(name, "size")) {
    Lua::Push(L, SizeStatusMessage());
  } else if (StringIsEqual(name, "max_size")) {
    Lua::Push(L, MaxSizeStatusMessage());
  } else
    return 0;

  return 1;
}

static int
l_statusmessage_set(lua_State *L)
{
  if (lua_gettop(L) != 4)
    return luaL_error(L, "Invalid parameters");

  bool stat = SetStatusMessage(luaL_checkstring(L, 1), luaL_checkstring(L, 2),
                               lua_toboolean(L, 3), luaL_checknumber(L, 4));

  if (!stat)
    return luaL_error(L, "Unable to set status message");
  else
    return 0;
}

static int
l_statusmessage_reset(lua_State *L)
{
  if (lua_gettop(L) != 0)
    return luaL_error(L, "Invalid parameters");

  ResetStatusMessage();

  return 0;
}

static constexpr struct luaL_Reg settings_funcs[] = {
  {"set", l_statusmessage_set},
  {"reset", l_statusmessage_reset},
  {nullptr, nullptr}
};

void
Lua::InitStatusMessage(lua_State *L)
{
  lua_getglobal(L, "xcsoar");

  lua_newtable(L);

  lua_newtable(L);
  SetField(L, -2, "__index", l_statusmessage_index);
  lua_setmetatable(L, -2);

  luaL_setfuncs(L, settings_funcs, 0);

  lua_setfield(L, -2, "statusmessage");

  lua_pop(L, 1);
}
