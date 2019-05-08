#include "batch.hpp"

Batch::Batch(DB *db) {
    m_db = db;
    l_ref_db(db);
    for (int i = 0; i < MAX_PARAM_NUM; i++) {
        m_int_param[i] = 0;
    }
}

Batch::~Batch() {
    l_unregister_db(m_db, [](void *db) {
        delete (DB *)db;
    });
}

void Batch::Put(lua_State *L, const Slice &key, Slice &val, bool compress) {
    std::lock_guard<MyMutex> guard(m_mutex);
    string value;
    if (compress) {
        size_t outLen = 0;
        void *p = tdefl_compress_mem_to_heap(val.data(), val.size(), &outLen, TDEFL_WRITE_ZLIB_HEADER | TDEFL_DEFAULT_MAX_PROBES);
        if (!p) {
            luaL_error(L, "compress failed");
        }
        m_batch.Put(key, Slice((const char *)p, outLen));
        value = string((const char *)p, outLen);
        mz_free(p);
    } else {
        m_batch.Put(key, val);
        value = val.ToString();
    }

    lua_pushinteger(L, value.size());
    auto key_ = key.ToString();
    auto it = m_dels.find(key_);
    if (it != m_dels.end()) {
        m_dels.erase(it);
    }
    auto it1 = m_upds.find(key_);
    if (it1 == m_upds.end()) {
        m_upds.emplace(key_, value);
    } else {
        it1->second = std::move(value);
    }
}

void Batch::Delete(const Slice &key) {
    std::lock_guard<MyMutex> guard(m_mutex);
    m_batch.Delete(key);
    auto key_ = key.ToString();
    auto it = m_dels.find(key_);
    if (it == m_dels.end()) {
        m_dels.emplace(key_);
    }
}

void Batch::Clear() {
    std::lock_guard<MyMutex> guard(m_mutex);
    m_batch.Clear();
    m_dels.clear();
    m_upds.clear();
}

int Batch::Get(lua_State *L, const Slice &key, bool uncompress) {
    std::lock_guard<MyMutex> guard(m_mutex);
    auto key_ = key.ToString();
    auto it = m_dels.find(key_);
    if (it != m_dels.end()) {
        return 0;
    }
    auto it1 = m_upds.find(key_);
    if (it1 != m_upds.end()) {
        auto &val = it1->second;
        if (uncompress) {
            miniz_uncompress(L, val.c_str(), val.size());
        } else {
            lua_pushlstring(L, val.c_str(), val.size());
        }
        return 1;
    }
    string value;
    Status s = m_db->Get(ReadOptions(), key, &value);
    if (s.ok()) {
        if (uncompress) {
            miniz_uncompress(L, value.c_str(), value.size());
        } else {
            lua_pushlstring(L, value.c_str(), value.size());
        }
        return 1;
    }
    return 0;
}

void Batch::Write(lua_State *L, DB *db) {
    std::lock_guard<MyMutex> guard(m_mutex);
    db->Write(lvldb_wopt(L, 3), &m_batch);
    Clear();
}

int Batch::GetIntParam(lua_State *L, int idx) {
    std::lock_guard<MyMutex> guard(m_mutex);
    if (idx < 0 || idx >= MAX_PARAM_NUM) {
        return 0;
    }
    lua_pushinteger(L, m_int_param[idx]);
    return 1;
}

int Batch::GetStringParam(lua_State *L, int idx) {
    std::lock_guard<MyMutex> guard(m_mutex);
    if (idx < 0 || idx >= MAX_PARAM_NUM) {
        return 0;
    }
    auto &param = m_str_param[idx];
    lua_pushlstring(L, param.c_str(), param.length());
    return 1;
}

void Batch::SetIntParam(lua_State *L, int idx, int64_t value) {
    std::lock_guard<MyMutex> guard(m_mutex);
    if (idx >= 0 || idx < MAX_PARAM_NUM) {
        m_int_param[idx] = value;
    }
}

void Batch::SetStringParam(lua_State *L, int idx, string &&value) {
    std::lock_guard<MyMutex> guard(m_mutex);
    if (idx >= 0 || idx < MAX_PARAM_NUM) {
        m_str_param[idx] = std::move(value);
    }
}

int lvldb_batch_put(lua_State *L) {
    Batch &batch = *(check_writebatch(L, 1));
    Slice key = lua_to_slice(L, 2);
    Slice value = lua_to_slice(L, 3);
    bool compress = false;
    if (lua_gettop(L) >= 4) {
        luaL_checktype(L, 4, LUA_TBOOLEAN);
        compress = lua_toboolean(L, 4);
    }
    batch.Put(L, key, value, compress);
    return 1;
}

int lvldb_batch_get(lua_State *L) {
    Batch &batch = *(check_writebatch(L, 1));
    Slice key = lua_to_slice(L, 2);
    bool uncompress = false;
    if (lua_gettop(L) >= 3 && !lua_isnil(L, 3)) {
        luaL_checktype(L, 3, LUA_TBOOLEAN);
        uncompress = lua_toboolean(L, 3);
    }
    return batch.Get(L, key, uncompress);
}

int traceback(lua_State *L) {
    const char *msg = lua_tostring(L, 1);
    if (msg)
        luaL_traceback(L, L, msg, 1);
    else {
        lua_pushliteral(L, "(no error message)");
    }
    return 1;
}

int lvldb_batch_lock(lua_State *L) {
    Batch &batch = *(check_writebatch(L, 1));
    luaL_checktype(L, 2, LUA_TFUNCTION);
    std::lock_guard<MyMutex> guard(batch.m_mutex);
    lua_pushcfunction(L, traceback);
    int top = lua_gettop(L);
    lua_pushvalue(L, 2);
    if (lua_pcall(L, 0, 0, -2)) {
        lua_remove(L, -2);
        assert(lua_gettop(L) == top);
        luaL_checktype(L, -1, LUA_TSTRING);
        return 1;
    }
    assert(lua_gettop(L) == top);
    return 0;
}

int lvldb_batch_set_need_lock(lua_State *L) {
    Batch &batch = *(check_writebatch(L, 1));
    luaL_checktype(L, 2, LUA_TBOOLEAN);
    bool b = lua_toboolean(L, 2);
    std::lock_guard<recursive_mutex> guard(batch.m_mutex.m_mutex);
    batch.m_mutex.m_need_mutex = b;
    return 0;
}

int lvldb_batch_int_param(lua_State *L) {
    Batch &batch = *(check_writebatch(L, 1));
    int idx = (int)luaL_checkinteger(L, 2);
    return batch.GetIntParam(L, idx);
}

int lvldb_batch_str_param(lua_State *L) {
    Batch &batch = *(check_writebatch(L, 1));
    int idx = (int)luaL_checkinteger(L, 2);
    return batch.GetStringParam(L, idx);
}

int lvldb_batch_set_int_param(lua_State *L) {
    Batch &batch = *(check_writebatch(L, 1));
    int idx = (int)luaL_checkinteger(L, 2);
    int64_t value = (int64_t)luaL_checkinteger(L, 3);
    batch.SetIntParam(L, idx, value);
    return 0;
}

int lvldb_batch_set_str_param(lua_State *L) {
    Batch &batch = *(check_writebatch(L, 1));
    int idx = (int)luaL_checkinteger(L, 2);
    size_t len;
    auto p = luaL_checklstring(L, 3, &len);
    batch.SetStringParam(L, idx, string(p, len));
    return 0;
}

int lvldb_batch_del(lua_State *L) {
    Batch &batch = *(check_writebatch(L, 1));
    Slice key = lua_to_slice(L, 2);
    batch.Delete(key);
    return 0;
}

int lvldb_batch_clear(lua_State *L) {
    Batch &batch = *(check_writebatch(L, 1));
    batch.Clear();
    return 0;
}

int lvldb_batch_close(lua_State *L) {
    Batch **batch = (Batch **)luaL_checkudata(L, 1, LVLDB_MT_BATCH);
    if (*batch) {
        l_unregister_db(*batch, [](void *batch) {
            ((Batch *)batch)->~Batch();
        });
        *batch = nullptr;
    }
    return 0;
}

int lvdb_batch_gc(lua_State *L) {
    Batch *batch = check_writebatch(L, 1);
    if (batch) {
        l_unregister_db(batch, [](void *d) {
            ((Batch *)d)->~Batch();
        });
    }
    return 0;
}

int lvldb_raw_batch_put(lua_State *L) {
    WriteBatch &batch = *(check_raw_writebatch(L, 1));
    Slice key = lua_to_slice(L, 2);
    Slice val = lua_to_slice(L, 3);
    bool compress = false;
    if (lua_gettop(L) >= 4) {
        luaL_checktype(L, 4, LUA_TBOOLEAN);
        compress = lua_toboolean(L, 4);
    }
    if (compress) {
        size_t outLen = 0;
        void *p = tdefl_compress_mem_to_heap(val.data(), val.size(), &outLen, TDEFL_WRITE_ZLIB_HEADER | TDEFL_DEFAULT_MAX_PROBES);
        if (!p) {
            luaL_error(L, "compress failed");
        }
        batch.Put(key, Slice((const char *)p, outLen));
        lua_pushinteger(L, outLen);
        mz_free(p);
    } else {
        batch.Put(key, val);
        lua_pushinteger(L, val.size());
    }
    return 1;
}

int lvldb_raw_batch_del(lua_State *L) {
    WriteBatch &batch = *(check_raw_writebatch(L, 1));
    Slice key = lua_to_slice(L, 2);
    batch.Delete(key);
    return 0;
}

int lvldb_raw_batch_clear(lua_State *L) {
    WriteBatch &batch = *(check_raw_writebatch(L, 1));
    batch.Clear();
    return 0;
}

int lvldb_raw_batch_gc(lua_State *L) {
    WriteBatch *batch = check_raw_writebatch(L, 1);
    batch->~WriteBatch();
    return 0;
}
