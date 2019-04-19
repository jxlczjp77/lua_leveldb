#pragma once
#include <sstream>

#include "lua.hpp"
#include "lib.hpp"
#include "utils.hpp"
#include <mutex>
#include <unordered_map>
#include <unordered_set>

using namespace leveldb;
#define MAX_PARAM_NUM 4

class MyMutex {
public:
    MyMutex() : m_need_mutex(false) {}
    ~MyMutex() {}

    void lock() {
        if (m_need_mutex) {
            m_mutex.lock();
        }
    }

    bool try_lock() { return m_need_mutex ? m_mutex.try_lock() : true; }

    void unlock() {
        if (m_need_mutex) {
            m_mutex.unlock();
        }
    }

    recursive_mutex m_mutex;
    bool m_need_mutex;
};

class Batch {
public:
    Batch(DB *db);
    ~Batch();
    void Put(lua_State *L, const Slice &key, Slice &val, bool compress);
    void Delete(const Slice &key);
    void Clear();
    int Get(lua_State *L, const Slice &key, bool uncompress);
    void Write(lua_State *L, DB *db);
    int GetIntParam(lua_State *L, int idx);
    int GetStringParam(lua_State *L, int idx);
    void SetIntParam(lua_State *L, int idx, int value);
    void SetStringParam(lua_State *L, int idx, string &&value);

    MyMutex m_mutex;
    WriteBatch m_batch;
    unordered_set<string> m_dels;
    unordered_map<string, string> m_upds;
    int m_int_param[MAX_PARAM_NUM];
    string m_str_param[MAX_PARAM_NUM];
    DB *m_db;
};

int lvldb_batch_put(lua_State *L);
int lvldb_batch_del(lua_State *L);
int lvldb_batch_get(lua_State *L);
int lvldb_batch_clear(lua_State *L);
int lvldb_batch_close(lua_State *L);
int lvdb_batch_gc(lua_State *L);
int lvldb_batch_lock(lua_State *L);
int lvldb_batch_set_need_lock(lua_State *L);
int lvldb_batch_int_param(lua_State *L);
int lvldb_batch_str_param(lua_State *L);
int lvldb_batch_set_int_param(lua_State *L);
int lvldb_batch_set_str_param(lua_State *L);

int lvldb_raw_batch_put(lua_State *L);
int lvldb_raw_batch_del(lua_State *L);
int lvldb_raw_batch_clear(lua_State *L);
int lvdb_raw_batch_gc(lua_State *L);
