#include "lua-leveldb.hpp"
#include "utils.hpp"
#include <map>
#include <mutex>
#include <chrono>
#include <sys/timeb.h>

// Rock info
#define LUALEVELDB_VERSION "Lua-LevelDB 0.4.0"
#define LUALEVELDB_COPYRIGHT "Copyright (C) 2012-18, Lua-LevelDB by Marco Pompili (pompilimrc@gmail.com)."
#define LUALEVELDB_DESCRIPTION "Lua bindings for Google's LevelDB library."
#define LUALEVELDB_LOGMODE 0

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
        luaL_error(L, "lvldb_open: Error opening creating database: %s", s.ToString().c_str());
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
    new (optp) Options();
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
    luaL_getmetatable(L, LVLDB_MT_RAW_BATCH);
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

int lvldb_now(lua_State *L) {
    auto n = std::chrono::high_resolution_clock::now();
    auto p = (decltype(n) *)lua_newuserdata(L, sizeof(n));
    new(p) decltype(n)(n);
    return 1;
}

int lvldb_elapsed(lua_State *L) {
    auto end = std::chrono::high_resolution_clock::now();
    auto start = (decltype(end) *)lua_touserdata(L, 1);
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - *start);
    lua_pushinteger(L, elapsed.count());
    return 1;
}

int lvldb_tick(lua_State *L) {
    timeb t;
    ftime(&t);
    lua_pushinteger(L, t.time * 1000 + t.millitm);
    return 1;
}

int lvldb_miniz_compress(lua_State *L) {
    size_t l = 0;
    auto p = luaL_checklstring(L, 1, &l);
    miniz_compress(L, p, l);
    return 1;
}

int lvldb_miniz_decompress(lua_State *L) {
    size_t l = 0;
    auto p = luaL_checklstring(L, 1, &l);
    miniz_uncompress(L, p, l);
    return 1;
}

// base64
#define SMALL_CHUNK 256
static int
lb64encode(lua_State *L) {
	static const char* encoding = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	size_t sz = 0;
	const uint8_t * text = (const uint8_t *)luaL_checklstring(L, 1, &sz);
	int encode_sz = int((sz + 2)/3*4);
	char tmp[SMALL_CHUNK];
	char *buffer = tmp;
	if (encode_sz > SMALL_CHUNK) {
		buffer = (char*)lua_newuserdata(L, encode_sz);
	}
	int i,j;
	j=0;
	for (i=0;i<(int)sz-2;i+=3) {
		uint32_t v = text[i] << 16 | text[i+1] << 8 | text[i+2];
		buffer[j] = encoding[v >> 18];
		buffer[j+1] = encoding[(v >> 12) & 0x3f];
		buffer[j+2] = encoding[(v >> 6) & 0x3f];
		buffer[j+3] = encoding[(v) & 0x3f];
		j+=4;
	}
	int padding = int(sz-i);
	uint32_t v;
	switch(padding) {
	case 1 :
		v = text[i];
		buffer[j] = encoding[v >> 2];
		buffer[j+1] = encoding[(v & 3) << 4];
		buffer[j+2] = '=';
		buffer[j+3] = '=';
		break;
	case 2 :
		v = text[i] << 8 | text[i+1];
		buffer[j] = encoding[v >> 10];
		buffer[j+1] = encoding[(v >> 4) & 0x3f];
		buffer[j+2] = encoding[(v & 0xf) << 2];
		buffer[j+3] = '=';
		break;
	}
	lua_pushlstring(L, buffer, encode_sz);
	return 1;
}

static inline int
b64index(uint8_t c) {
	static const int decoding[] = {62,-1,-1,-1,63,52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-2,-1,-1,-1,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,-1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51};
	int decoding_size = sizeof(decoding)/sizeof(decoding[0]);
	if (c<43) {
		return -1;
	}
	c -= 43;
	if (c>=decoding_size)
		return -1;
	return decoding[c];
}

static int
lb64decode(lua_State *L) {
	size_t sz = 0;
	const uint8_t * text = (const uint8_t *)luaL_checklstring(L, 1, &sz);
	int decode_sz = int((sz+3)/4*3);
	char tmp[SMALL_CHUNK];
	char *buffer = tmp;
	if (decode_sz > SMALL_CHUNK) {
		buffer = (char*)lua_newuserdata(L, decode_sz);
	}
	int i,j;
	int output = 0;
	for (i=0;i<sz;) {
		int padding = 0;
		int c[4];
		for (j=0;j<4;) {
			if (i>=sz) {
				return luaL_error(L, "Invalid base64 text");
			}
			c[j] = b64index(text[i]);
			if (c[j] == -1) {
				++i;
				continue;
			}
			if (c[j] == -2) {
				++padding;
			}
			++i;
			++j;
		}
		uint32_t v;
		switch (padding) {
		case 0:
			v = (unsigned)c[0] << 18 | c[1] << 12 | c[2] << 6 | c[3];
			buffer[output] = v >> 16;
			buffer[output+1] = (v >> 8) & 0xff;
			buffer[output+2] = v & 0xff;
			output += 3;
			break;
		case 1:
			if (c[3] != -2 || (c[2] & 3)!=0) {
				return luaL_error(L, "Invalid base64 text");
			}
			v = (unsigned)c[0] << 10 | c[1] << 4 | c[2] >> 2 ;
			buffer[output] = v >> 8;
			buffer[output+1] = v & 0xff;
			output += 2;
			break;
		case 2:
			if (c[3] != -2 || c[2] != -2 || (c[1] & 0xf) !=0)  {
				return luaL_error(L, "Invalid base64 text");
			}
			v = (unsigned)c[0] << 2 | c[1] >> 4;
			buffer[output] = v;
			++ output;
			break;
		default:
			return luaL_error(L, "Invalid base64 text");
		}
	}
	lua_pushlstring(L, buffer, output);
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
    {"repair", lvldb_repair},
    {"rawbatch", lvldb_raw_batch},
    {"check", lvldb_check},
    {"now", lvldb_now},
    {"elapsed", lvldb_elapsed},
    {"tick", lvldb_tick},
    {"mz_compress", lvldb_miniz_compress},
    {"mz_decompress", lvldb_miniz_decompress},
	{"base64encode", lb64encode},
	{"base64decode", lb64decode},
    {NULL, NULL} };

// options methods
static const luaL_Reg lvldb_options_m[] = {
    {NULL, NULL} };

// options meta-methods
static const luaL_Reg lvldb_options_meta[] = {
    {"__tostring", lvldb_options_tostring},
    {NULL, NULL} };

// options getters
static const Xet_reg_pre options_getsets[] = {
    {"createIfMissing", get_bool, set_bool, offsetof(Options, create_if_missing)},
    {"errorIfExists", get_bool, set_bool, offsetof(Options, error_if_exists)},
    {"paranoidChecks", get_bool, set_bool, offsetof(Options, paranoid_checks)},
    {"writeBufferSize", get_size, set_size, offsetof(Options, write_buffer_size)},
    {"maxOpenFiles", get_int, set_int, offsetof(Options, max_open_files)},
    {"blockSize", get_size, set_size, offsetof(Options, block_size)},
    {"blockRestartInterval", get_int, set_int, offsetof(Options, block_restart_interval)},
    {"maxFileSize", get_int, set_int, offsetof(Options, max_file_size)},
    {NULL, NULL} };

// read options methods
static const luaL_Reg lvldb_read_options_m[] = {
    {"__tostring", lvldb_read_options_tostring},
    {NULL, NULL} };

// read options getters
static const Xet_reg_pre read_options_getsets[] = {
    {"verifyChecksum", get_bool, set_bool, offsetof(MyReadOptions, verify_checksums)},
    {"fillCache", get_bool, set_bool, offsetof(MyReadOptions, fill_cache)},
    {"decompress", get_bool, set_bool, offsetof(MyReadOptions, UnCompress)},
    {NULL, NULL} };

// write options methods
static const luaL_Reg lvldb_write_options_m[] = {
    {"__tostring", lvldb_write_options_tostring},
    {NULL, NULL} };

// write options getters
static const Xet_reg_pre write_options_getsets[] = {
    {"sync", get_bool, set_bool, offsetof(MyWriteOptions, sync)},
    {"compress", get_bool, set_bool, offsetof(MyWriteOptions, Compress)},
    {NULL, NULL} };

// database methods
static const luaL_Reg lvldb_database_m[] = {
    {"put", lvldb_database_put},
    {"get", lvldb_database_get},
    {"batch", lvldb_batch},
    {"close", lvldb_close},
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
    {"prev", lvldb_iterator_prev},
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
    {"__gc", lvldb_raw_batch_gc},
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

        luaL_setfuncs(L, lvldb_leveldb_m, 0);

        // initialize meta-tables methods
        init_metatable(L, LVLDB_MT_DB, lvldb_database_m);
        init_metatable(L, LVLDB_MT_OPT, lvldb_options_m, options_getsets);
        init_metatable(L, LVLDB_MT_ROPT, lvldb_read_options_m, read_options_getsets);
        init_metatable(L, LVLDB_MT_WOPT, lvldb_write_options_m, write_options_getsets);
        init_metatable(L, LVLDB_MT_ITER, lvldb_iterator_m);
        init_metatable(L, LVLDB_MT_BATCH, lvldb_batch_m);
        init_metatable(L, LVLDB_MT_RAW_BATCH, lvldb_raw_batch_m);

        return 1;
    }
}
