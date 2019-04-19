#include "lua-leveldb.hpp"
#include <map>
#include <mutex>

// Rock info
#define LUALEVELDB_VERSION "Lua-LevelDB 0.4.0"
#define LUALEVELDB_COPYRIGHT "Copyright (C) 2012-18, Lua-LevelDB by Marco Pompili (pompilimrc@gmail.com)."
#define LUALEVELDB_DESCRIPTION "Lua bindings for Google's LevelDB library."
#define LUALEVELDB_LOGMODE 0

using namespace std;
using namespace leveldb;

struct DbRef {
    void *db;
    int refCount;
};

map<string, DbRef> g_register_dbs;
mutex g_mutex;
void *l_get_db(const string &db_path) {
    std::lock_guard<std::mutex> guard(g_mutex);
    auto it = g_register_dbs.find(db_path);
    if (it == g_register_dbs.end()) {
        return nullptr;
    }
    return it->second.db;
}

void l_ref_db(void *db) {
    std::lock_guard<std::mutex> guard(g_mutex);
    for (auto it = g_register_dbs.begin(); it != g_register_dbs.end(); ++it) {
        if (it->second.db == db) {
            ++it->second.refCount;
            return;
        }
    }
}

void l_register_db(const string &db_path, void *db) {
    std::lock_guard<std::mutex> guard(g_mutex);
    auto it = g_register_dbs.find(db_path);
    if (it == g_register_dbs.end()) {
        g_register_dbs.insert(std::make_pair(db_path, DbRef{ db, 1 }));
    } else {
        it->second.refCount++;
    }
}

void l_unregister_db(void *db, const std::function<void(void *)> &delete_cb) {
    {
        std::lock_guard<std::mutex> guard(g_mutex);
        for (auto it = g_register_dbs.begin(); it != g_register_dbs.end(); ++it) {
            if (it->second.db == db) {
                if (--it->second.refCount == 0) {
                    g_register_dbs.erase(it);
                    break;
                }
                return;
            }
        }
    }
    delete_cb(db);
}

int lvldb_open(lua_State *L) {
    DB *db;
    Options *opt = check_options(L, 1);
    const char *filename = luaL_checkstring(L, 2);

    Status s;
    db = (DB *)l_get_db(filename);
    if (!db) {
        s = DB::Open(*(opt), filename, &db);
    }

    if (!s.ok())
        cerr << "lvldb_open: Error opening creating database: " << s.ToString() << endl;
    else {
        *(DB**)lua_newuserdata(L, sizeof(DB**)) = db;
        luaL_getmetatable(L, LVLDB_MT_DB);
        lua_setmetatable(L, -2);
        l_register_db(filename, db);
    }

    return 1;
}

int lvldb_close(lua_State *L) {
    DB **db = (DB **)luaL_checkudata(L, 1, LVLDB_MT_DB);
    if (*db) {
        l_unregister_db(*db, [](void *db) {
            delete (DB *)db;
        });
        *db = nullptr;
    }
    return 0;
}

int lvldb_options(lua_State *L) {
    Options *optp = (Options *)lua_newuserdata(L, sizeof(Options));

    *(optp) = Options(); // set default values

    luaL_getmetatable(L, LVLDB_MT_OPT);
    lua_setmetatable(L, -2);
    return 1;
}

int lvldb_batch(lua_State *L) {
    string name;
    Batch *batchp = nullptr;
    DB *db = *(DB **)luaL_checkudata(L, 1, LVLDB_MT_DB);
    if (lua_gettop(L) >= 2) {
        name = luaL_checkstring(L, 2);
        batchp = (Batch *)l_get_db(name);
    }

    if (!batchp) {
        batchp = new Batch(db);
    }

    *(Batch **)lua_newuserdata(L, sizeof(Batch *)) = batchp;
    luaL_getmetatable(L, LVLDB_MT_BATCH);
    lua_setmetatable(L, -2);
    if (!name.empty()) {
        l_register_db(name, batchp);
    }
    return 1;
}

int lvldb_raw_batch(lua_State *L) {
    WriteBatch *batchp = (WriteBatch *)lua_newuserdata(L, sizeof(WriteBatch));
    new (batchp) WriteBatch;
    luaL_getmetatable(L, LVLDB_MT_RAWBATCH);
    lua_setmetatable(L, -2);
    return 1;
}

int lvldb_check(lua_State *L) {
    DB *db = (DB *)lua_touserdata(L, 1);

    lua_pushboolean(L, db != NULL ? true : false);

    return 1;
}

int lvldb_repair(lua_State *L) {
    string dbname = luaL_checkstring(L, 1);

    Status s = leveldb::RepairDB(dbname, lvldb_opt(L, 2));

    if (s.ok())
        lua_pushboolean(L, true);
    else {
        cerr << "Error repairing database: " << s.ToString() << endl;
        lua_pushboolean(L, false);
    }

    return 1;
}

int lvldb_bloom_filter_policy(lua_State *L) {
    luaL_checktype(L, 1, LUA_TUSERDATA);
    Options *opt = (Options *)luaL_checkudata(L, 1, LVLDB_MT_OPT);
    int bits_per_key = (int)lua_tointeger(L, 2);
    const FilterPolicy *fp = NewBloomFilterPolicy(bits_per_key);

    opt->filter_policy = fp;

    return 0;
}

int lvldb_miniz_compress(lua_State *L) {
    size_t l = 0;
    auto p = luaL_checklstring(L, 1, &l);
    miniz_compress(L, p, l);
    return 1;
}

int lvldb_miniz_uncompress(lua_State *L) {
    size_t l = 0;
    auto p = luaL_checklstring(L, 1, &l);
    miniz_uncompress(L, p, l);
    return 1;
}

// empty
static const struct luaL_Reg E[] = { {NULL, NULL} };

// main methods
static const luaL_Reg lvldb_leveldb_m[] = {
    {"open", lvldb_open},
    {"close", lvldb_close},
    {"options", lvldb_options},
    {"readOptions", lvldb_read_options},
    {"writeOptions", lvldb_write_options},
    {"repair ", lvldb_repair},
    {"rawbatch", lvldb_raw_batch},
    {"check", lvldb_check},
    {"mz_compress", lvldb_miniz_compress},
    {"mz_uncompress", lvldb_miniz_uncompress},
    {"bloomFilterPolicy", lvldb_bloom_filter_policy},
    {NULL, NULL} };

// options methods
static const luaL_Reg lvldb_options_m[] = {
    {NULL, NULL} };

// options meta-methods
static const luaL_Reg lvldb_options_meta[] = {
    {"__tostring", lvldb_options_tostring},
    {NULL, NULL} };

// options getters
static const Xet_reg_pre options_getters[] = {
    {"createIfMissing", get_bool, offsetof(Options, create_if_missing)},
    {"errorIfExists", get_bool, offsetof(Options, error_if_exists)},
    {"paranoidChecks", get_bool, offsetof(Options, paranoid_checks)},
    {"writeBufferSize", get_size, offsetof(Options, write_buffer_size)},
    {"maxOpenFiles", get_int, offsetof(Options, max_open_files)},
    {"blockSize", get_size, offsetof(Options, block_size)},
    {"blockRestartInterval", get_int, offsetof(Options, block_restart_interval)},
    {"maxFileSize", get_int, offsetof(Options, max_file_size)},
    {NULL, NULL} };

// options setters
static const Xet_reg_pre options_setters[] = {
    {"createIfMissing", set_bool, offsetof(Options, create_if_missing)},
    {"errorIfExists", set_bool, offsetof(Options, error_if_exists)},
    {"paranoidChecks", set_bool, offsetof(Options, paranoid_checks)},
    {"writeBufferSize", set_size, offsetof(Options, write_buffer_size)},
    {"maxOpenFiles", set_int, offsetof(Options, max_open_files)},
    {"blockSize", set_size, offsetof(Options, block_size)},
    {"blockRestartInterval", set_int, offsetof(Options, block_restart_interval)},
    {"maxFileSize", set_int, offsetof(Options, max_file_size)},
    {NULL, NULL} };

// read options methods
static const luaL_Reg lvldb_read_options_m[] = {
    {NULL, NULL} };

// read options meta-methods
static const luaL_Reg lvldb_read_options_meta[] = {
    {"__tostring", lvldb_read_options_tostring},
    {NULL, NULL} };

// read options getters
static const Xet_reg_pre read_options_getters[] = {
    {"verifyChecksum", get_bool, offsetof(MyReadOptions, verify_checksums)},
    {"fillCache", get_bool, offsetof(MyReadOptions, fill_cache)},
    {"uncompress", get_bool, offsetof(MyReadOptions, UnCompress)},
    {NULL, NULL} };

// read options setters
static const Xet_reg_pre read_options_setters[] = {
    {"verifyChecksum", set_bool, offsetof(MyReadOptions, verify_checksums)},
    {"fillCache", set_bool, offsetof(MyReadOptions, fill_cache)},
    {"uncompress", set_bool, offsetof(MyReadOptions, UnCompress)},
    {NULL, NULL} };

// write options methods
static const luaL_Reg lvldb_write_options_m[] = {
    {NULL, NULL} };

// write options meta-methods
static const luaL_Reg lvldb_write_options_meta[] = {
    {"__tostring", lvldb_write_options_tostring},
    {NULL, NULL} };

// write options getters
static const Xet_reg_pre write_options_getters[] = {
    {"sync", get_bool, offsetof(MyWriteOptions, sync)},
    {"compress", get_bool, offsetof(MyWriteOptions, Compress)},
    {NULL, NULL} };

// write options setters
static const Xet_reg_pre write_options_setters[] = {
    {"sync", set_bool, offsetof(MyWriteOptions, sync)},
    {"compress", set_bool, offsetof(MyWriteOptions, Compress)},
    {NULL, NULL} };

// database methods
static const luaL_Reg lvldb_database_m[] = {
    {"__tostring", lvldb_database_tostring},
    {"put", lvldb_database_put},
    {"set", lvldb_database_set},
    {"batch", lvldb_batch},
    {"get", lvldb_database_get},
    {"has", lvldb_database_has},
    {"delete", lvldb_database_del},
    {"iterator", lvldb_database_iterator},
    {"write", lvldb_database_write},
    {"snapshot", lvldb_database_snapshot},
    {"__gc", lvldb_close},
    {NULL, NULL} };

// iterator methods
static const struct luaL_Reg lvldb_iterator_m[] = {
    {"del", lvldb_iterator_delete},
    {"seek", lvldb_iterator_seek},
    {"seekToFirst", lvldb_iterator_seek_to_first},
    {"seekToLast", lvldb_iterator_seek_to_last},
    {"valid", lvldb_iterator_valid},
    {"next", lvldb_iterator_next},
    {"key", lvldb_iterator_key},
    {"value", lvldb_iterator_val},
    {"__gc", lvldb_iterator_delete},
    {NULL, NULL} };

// batch methods
static const luaL_Reg lvldb_batch_m[] = {
    {"put", lvldb_batch_put},
    {"get", lvldb_batch_get},
    {"lock", lvldb_batch_lock},
    {"close", lvldb_batch_close},
    {"get_int_param", lvldb_batch_int_param},
    {"get_str_param", lvldb_batch_str_param},
    {"set_int_param", lvldb_batch_set_int_param},
    {"set_str_param", lvldb_batch_set_str_param},
    {"set_need_lock", lvldb_batch_set_need_lock},
    {"delete", lvldb_batch_del},
    {"clear", lvldb_batch_clear},
    {"__gc", lvdb_batch_gc},
    {NULL, NULL} };


// batch methods
static const luaL_Reg lvldb_raw_batch_m[] = {
    {"put", lvldb_raw_batch_put},
    {"delete", lvldb_raw_batch_del},
    {"clear", lvldb_raw_batch_clear},
    {"__gc", lvdb_raw_batch_gc},
    {NULL, NULL} };


extern "C"
{
    // Initialization
    LUALIB_API int luaopen_lualeveldb(lua_State *L) {
        lua_newtable(L);

        // register module information
        lua_pushliteral(L, LUALEVELDB_VERSION);
        lua_setfield(L, -2, "_VERSION");

        lua_pushliteral(L, LUALEVELDB_COPYRIGHT);
        lua_setfield(L, -2, "_COPYRIGHT");

        lua_pushliteral(L, LUALEVELDB_DESCRIPTION);
        lua_setfield(L, -2, "_DESCRIPTION");

        // LevelDB functions
#if LUA_VERSION_NUM > 501
        luaL_setfuncs(L, lvldb_leveldb_m, 0);
#else
        luaL_register(L, NULL, lvldb_leveldb_m);
#endif

        // initialize meta-tables methods
        init_metatable(L, LVLDB_MT_DB, lvldb_database_m);
        init_complex_metatable(L, LVLDB_MT_OPT, lvldb_options_m, lvldb_options_meta, options_getters, options_setters);
        init_complex_metatable(L, LVLDB_MT_ROPT, lvldb_read_options_m, lvldb_read_options_meta, read_options_getters, read_options_setters);
        init_complex_metatable(L, LVLDB_MT_WOPT, lvldb_write_options_m, lvldb_write_options_meta, write_options_getters, write_options_setters);
        init_metatable(L, LVLDB_MT_ITER, lvldb_iterator_m);
        init_metatable(L, LVLDB_MT_RAWBATCH, lvldb_raw_batch_m);
        init_metatable(L, LVLDB_MT_BATCH, lvldb_batch_m);

        return 1;
    }
}
