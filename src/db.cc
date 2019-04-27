#include "db.hpp"
#include "batch.hpp"

DB *check_database(lua_State *L, int index) {
    return *(DB **)luaL_checkudata(L, index, LVLDB_MT_DB);
}

int lvldb_database_put(lua_State *L) {
    DB *db = check_database(L, 1);
    Slice key = lua_to_slice(L, 2);
    Slice value = lua_to_slice(L, 3);
    auto wopt = lvldb_wopt(L, 4);
    Status s;
    if (wopt.Compress) {
        size_t outLen = 0;
        void *p = tdefl_compress_mem_to_heap(value.data(), value.size(), &outLen, TDEFL_WRITE_ZLIB_HEADER | TDEFL_DEFAULT_MAX_PROBES);
        if (!p) {
            luaL_error(L, "compress failed");
        }
        s = db->Put(wopt, key, Slice((const char *)p, outLen));
        mz_free(p);
    } else {
        s = db->Put(lvldb_wopt(L, 4), key, value);
    }
    lua_pushboolean(L, s.ok());
    return 1;
}

int lvldb_database_get(lua_State *L) {
    DB *db = check_database(L, 1);
    Slice key = lua_to_slice(L, 2);
    auto ropt = lvldb_ropt(L, 3);
    string value;
    Status s = db->Get(ropt, key, &value);
    if (s.ok()) {
        if (ropt.UnCompress) {
            miniz_uncompress(L, value.c_str(), value.size());
        } else {
            lua_pushlstring(L, value.c_str(), value.size());
        }
        return 1;
    }
    return 0;
}

int lvldb_database_has(lua_State *L) {
    DB *db = check_database(L, 1);
    Slice key = lua_to_slice(L, 2);
    Status s = db->Get(lvldb_ropt(L, 3), key, nullptr);
    if (s.ok()) {
        lua_pushboolean(L, true);
    } else {
        lua_pushboolean(L, false);
    }
    return 1;
}

int lvldb_database_del(lua_State *L) {
    DB *db = check_database(L, 1);
    Slice key = lua_to_slice(L, 2);
    Status s = db->Delete(lvldb_wopt(L, 3), key);
    if (s.ok())
        lua_pushboolean(L, true);
    else {
        lua_pushboolean(L, false);
    }
    return 1;
}

int lvldb_database_iterator(lua_State *L) {
    DB *db = check_database(L, 1);
    Iterator *it = db->NewIterator(lvldb_ropt(L, 2));
    *(Iterator **)lua_newuserdata(L, sizeof(Iterator **)) = it;
    luaL_getmetatable(L, LVLDB_MT_ITER);
    lua_setmetatable(L, -2);

    return 1;
}

int lvldb_database_write(lua_State *L) {
    DB *db = check_database(L, 1);
    auto rawbatch = (WriteBatch *)luaL_testudata(L, 2, LVLDB_MT_RAW_BATCH);
    if (rawbatch) {
        db->Write(lvldb_wopt(L, 3), rawbatch);
    } else {
        Batch &batch = *(check_writebatch(L, 2));
        batch.Write(L, db);
    }
    return 0;
}

int lvldb_database_snapshot(lua_State *L) {
    DB *db = check_database(L, 1);
    const Snapshot *snapshot = db->GetSnapshot();
    lua_pushlightuserdata(L, (void *)snapshot);
    return 1;
}

