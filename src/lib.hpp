#pragma once
#include <leveldb/db.h>
#include <leveldb/status.h>

#include <lua.hpp>

// LevelDB atomic ops
#include <leveldb/write_batch.h>

// LevelDB filters
#include <leveldb/filter_policy.h>

#include <leveldb/comparator.h>
using namespace leveldb;
using namespace std;
