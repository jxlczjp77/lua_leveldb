#include "utils.hpp"

Slice lua_to_slice(lua_State *L, int i) {
    size_t l = 0;
    const char *data = luaL_checklstring(L, i, &l);
    return Slice(data, l);
}

string bool_tostring(int boolean) {
    return boolean == 1 ? "true" : "false";
}

string pointer_tostring(void *p) {
    ostringstream oss(ostringstream::out);
    if (p != NULL)
        oss << &p;
    else
        oss << "NULL";
    return oss.str().c_str();
}

string filter_tostring(const FilterPolicy *fp) {
    return fp == 0 ? "NULL" : fp->Name();
}

Options *check_options(lua_State *L, int index) {
    return (Options*)luaL_checkudata(L, index, LVLDB_MT_OPT);
}

MyReadOptions *check_read_options(lua_State *L, int index) {
    void *ud = luaL_checkudata(L, index, LVLDB_MT_ROPT);
    return (MyReadOptions *)ud;
}

MyWriteOptions *check_write_options(lua_State *L, int index) {
    return (MyWriteOptions *)luaL_checkudata(L, index, LVLDB_MT_WOPT);
}

WriteBatch *check_raw_writebatch(lua_State *L, int index) {
    return (WriteBatch *)luaL_checkudata(L, index, LVLDB_MT_RAW_BATCH);
}

Batch *check_writebatch(lua_State *L, int index) {
    return *(Batch **)luaL_checkudata(L, index, LVLDB_MT_BATCH);
}

void miniz_compress(lua_State *L, const char *data, size_t len) {
    size_t outLen = 0;
    void *p = tdefl_compress_mem_to_heap(data, len, &outLen, TDEFL_WRITE_ZLIB_HEADER | TDEFL_DEFAULT_MAX_PROBES);
    if (!p) {
        lua_pushnil(L);
    } else {
        lua_pushlstring(L, (const char *)p, outLen);
        mz_free(p);
    }
}

void miniz_uncompress(lua_State *L, const char *data, size_t len) {
    size_t outLen = 0;
    void *p = tinfl_decompress_mem_to_heap(data, len, &outLen, TINFL_FLAG_PARSE_ZLIB_HEADER);
    if (!p) {
        lua_pushnil(L);
    } else {
        lua_pushlstring(L, (const char *)p, outLen);
        mz_free(p);
    }
}
