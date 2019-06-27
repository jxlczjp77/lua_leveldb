#include <stdio.h>
#include <lua.hpp>

typedef int(*Xet_func)(lua_State *L, void *v);

struct Xet_reg_pre {
    const char *name;
    Xet_func get_func;
    Xet_func set_func;
    size_t offset;
};

typedef const Xet_reg_pre *Xet_reg;

void init_metatable(lua_State *L, const char *metatable_name, const luaL_Reg methods[], const Xet_reg_pre getsets[] = NULL);
