#include "iter.hpp"
using namespace leveldb;

Iterator *check_iter(lua_State *L) {
    auto ud = (Iterator **)luaL_checkudata(L, 1, LVLDB_MT_ITER);
    return *ud;
}

int lvldb_iterator_delete(lua_State *L) {
    auto ud = (Iterator **)luaL_checkudata(L, 1, LVLDB_MT_ITER);
    auto iter = *ud;
    if (iter) {
        *ud = nullptr;
        delete iter;
    }
    return 0;
}

int lvldb_iterator_seek(lua_State *L) {
    Iterator *iter = check_iter(L);
    auto start = lua_to_slice(L, 2);
    iter->Seek(start);
    return 0;
}

int lvldb_iterator_seek_to_first(lua_State *L) {
    Iterator *iter = check_iter(L);
    iter->SeekToFirst();
    return 0;
}

int lvldb_iterator_seek_to_last(lua_State *L) {
    Iterator *iter = check_iter(L);
    iter->SeekToLast();
    return 0;
}

int lvldb_iterator_valid(lua_State *L) {
    Iterator *iter = check_iter(L);
    lua_pushboolean(L, iter->Valid());
    return 1;
}

int lvldb_iterator_next(lua_State *L) {
    Iterator *iter = check_iter(L);
    iter->Next();
    return 0;
}

int lvldb_iterator_key(lua_State *L) {
    Iterator *iter = check_iter(L);
    Slice key = iter->key();
    lua_pushlstring(L, key.data(), key.size());
    return 1;
}

int lvldb_iterator_val(lua_State *L) {
    Iterator *iter = check_iter(L);
    Slice val = iter->value();
    bool uncompress = false;
    if (lua_gettop(L) >= 2 && !lua_isnil(L, 2)) {
        luaL_checktype(L, 2, LUA_TBOOLEAN);
        uncompress = lua_toboolean(L, 2);
    }
    if (uncompress) {
        miniz_uncompress(L, val.data(), val.size());
    } else {
        lua_pushlstring(L, val.data(), val.size());
    }
    return 1;
}
