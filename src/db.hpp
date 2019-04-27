#pragma once
#include <iostream>
#include <sstream>

#include "lib.hpp"
#include "utils.hpp"

DB *check_database(lua_State *L, int index);

int lvldb_database_put(lua_State *L);
int lvldb_database_get(lua_State *L);
int lvldb_database_has(lua_State *L);
int lvldb_database_del(lua_State *L);
int lvldb_database_iterator(lua_State *L);
int lvldb_database_write(lua_State *L);
int lvldb_database_snapshot(lua_State *L);

