﻿#pragma once
#include <sstream>

#include "lib.hpp"
#include "utils.hpp"

int get_int(lua_State *L, void *v);
int set_int(lua_State *L, void *v);
int get_number(lua_State *L, void *v);
int set_number(lua_State *L, void *v);
int get_size(lua_State *L, void *v);
int set_size(lua_State *L, void *v);
int get_bool(lua_State *L, void *v);
int set_bool(lua_State *L, void *v);

int lvldb_options_tostring(lua_State *L);
int lvldb_read_options(lua_State *L);
int lvldb_read_options_tostring(lua_State *L);
int lvldb_write_options(lua_State *L);
int lvldb_write_options_tostring(lua_State *L);
