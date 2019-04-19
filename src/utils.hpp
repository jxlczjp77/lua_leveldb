#pragma once
#include <sstream>

#include "lua.hpp"
#include "lib.hpp"
#include <functional>
#include <miniz.h>

// Lua Meta-tables names
#define LVLDB_MOD_NAME		"leveldb"
#define LVLDB_MT_OPT		"leveldb.opt"
#define LVLDB_MT_ROPT		"leveldb.ropt"
#define LVLDB_MT_WOPT		"leveldb.wopt"
#define LVLDB_MT_DB			"leveldb.db"
#define LVLDB_MT_ITER		"leveldb.iter"
#define LVLDB_MT_RAWBATCH	"leveldb.rawbtch"
#define LVLDB_MT_BATCH		"leveldb.btch"

using namespace std;
using namespace leveldb;
class Batch;

struct MyReadOptions : public ReadOptions {
    bool UnCompress;
};

struct MyWriteOptions : public WriteOptions {
    bool Compress;
};

Slice lua_to_slice(lua_State *L, int i);
string bool_tostring(int boolean);
string pointer_tostring(void *p);
string filter_tostring(const FilterPolicy *fp);

Options *check_options(lua_State *L, int index);
MyReadOptions *check_read_options(lua_State *L, int index);
MyWriteOptions *check_write_options(lua_State *L, int index);

WriteBatch *check_raw_writebatch(lua_State *L, int index);
Batch *check_writebatch(lua_State *L, int index);
void l_unregister_db(void *db, const std::function<void(void *)> &delete_cb = std::function<void(void *)>());
void l_ref_db(void *db);

void miniz_compress(lua_State *L, const char *data, size_t len);
void miniz_uncompress(lua_State *L, const char *data, size_t len);

#define lvldb_opt(L, l) ( lua_gettop(L) >= l ? *(check_options(L, l)) : Options() )
#define lvldb_ropt(L, l) ( lua_gettop(L) >= l ? *(check_read_options(L, l)) : MyReadOptions() )
#define lvldb_wopt(L, l) ( lua_gettop(L) >= l ? *(check_write_options(L, l)) : MyWriteOptions() )
