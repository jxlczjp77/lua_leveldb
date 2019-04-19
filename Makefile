CXX=g++
CXXFLAGS=-g -O2 -Wall -std=c++11 -shared -fpic
LDLIBS=-lleveldb -lsnappy -lpthread
RM=rm -f
TARGET=lualeveldb.so

# Add a custom version below (5.1/5.2/5.3)
LUA_VERSION=5.3
LUA_DIR=/usr/local
LUA_INCDIR=$(LUA_DIR)/include/lua$(LUA_VERSION)

LEVELDB_DIR = src
MINIZ_DIR = 3rd/miniz

$(TARGET): $(LEVELDB_DIR)/*.cc $(MINIZ_DIR)/*.c
	$(CXX) $(CXXFLAGS) -I$(LEVELDB_DIR) -I$(MINIZ_DIR) $^ -o $@ $(LDLIBS)

.PHONY: clean
clean:
	$(RM) *.so *.o
