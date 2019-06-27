封装 leveldb 库给 lua 使用

## 特性

- 扩展 leveldb::Batch 支持 get 方法，获取 put 到 batch 中的 value，支持多线程访问。
- 扩展 leveldb::WriteOptions，添加 compress 选项设置 value 需要压缩后写入 db。
- 扩展 leveldb::ReadOptions，添加 decompress 选项设置 value 需要解压后返回。
- 提供 miniz 压缩方法和 base64 编码方法。
- 允许打开同一份 db 文件多次，也支持不同 lua 虚拟机打开同一份 db，内部根据 path 维护打开数据库列表，同一个 path 内部仅打开一次，多次打开增加引用计数，使用完数据库后记得 close，在引用计数为 0 时才真正关闭数据库，注意：使用 ldb:batch()创建扩展 batch 也会增加 db 的引用计数，记得关闭这个 batch。

## API

| 函数名称                       | 说明                         |
| :----------------------------- | :--------------------------- |
| lualeveldb.open(path)          | 打开数据库文件，返回 db 对象 |
| lualeveldb.close(db)           | 关闭 db 对象                 |
| lualeveldb.options()           | 创建数据库选项对象           |
| lualeveldb.readOptions()       | 创建读取选项对象             |
| lualeveldb.writeOptions()      | 创建写入选项对象             |
| lualeveldb.repair(path)        | 修复数据库文件               |
| lualeveldb.rawbatch()          | 创建 leveldb::Batch 对象     |
| lualeveldb.mz_compress(data)   | 压缩给定数据                 |
| lualeveldb.mz_decompress(data) | 解压给定数据                 |
| lualeveldb.base64encode(data)  | base64 encode                |
| lualeveldb.base64decode(data)  | base64 decode                |

| options              | 类型 |
| :------------------- | ---- |
| createIfMissing      | bool |
| errorIfExists        | bool |
| paranoidChecks       | bool |
| writeBufferSize      | int  |
| maxOpenFiles         | int  |
| blockSize            | int  |
| blockRestartInterval | int  |
| maxFileSize          | int  |

| read options   | 类型 |
| :------------- | ---- |
| verifyChecksum | bool |
| fillCache      | bool |
| decompress     | bool |

| write options | 类型 |
| :------------ | ---- |
| sync          | bool |
| compress      | bool |

| db 对象                        | 说明                                                          |
| :----------------------------- | ------------------------------------------------------------- |
| ldb:put(key, val, [writeopts]) | 写入数据                                                      |
| ldb:get(key, [readopts])       | 获取数据                                                      |
| ldb:batch()                    | 创建 batch(内部会引用当前 db 对象,关闭数据库前记得关闭 batch) |
| ldb:close()                    | 关闭数据库                                                    |
| ldb:has(key)                   | key 是否存在                                                  |
| ldb:delete(key)                | 删除 key                                                      |
| ldb:iterator()                 | 创建迭代器对象                                                |
| ldb:write(batch)               | 写入 batch(普通 rawbatch 或者扩展 batch 都支持)               |
| ldb:snapshot()                 | 创建 snapshot                                                 |

| 迭代器对象                   | 说明                                 |
| :--------------------------- | ------------------------------------ |
| iterator:del()               | 删除迭代器对象(关闭数据库前必须删除) |
| iterator:seek(prefix)        | seek 到指定位置                      |
| iterator:seekToFirst()       | seek 到数据库起始位置                |
| iterator:seekToLast()        | seek 到数据库最后一个位置            |
| iterator:valid()             | 迭代器是否有效                       |
| iterator:next()              | 移动到下一个位置                     |
| iterator:prev()              | 移动到前一个位置                     |
| iterator:key()               | 获取 key                             |
| iterator:value([uncompress]) | 获取 value                           |

| batch 对象                                               | 说明                                                                 |
| -------------------------------------------------------- | -------------------------------------------------------------------- |
| batch:put(key, val, [writeopts])                         | 写入数据                                                             |
| batch:get(key, [readopts])                               | 获取数据                                                             |
| batch:lock(cb)                                           | 锁定 batch 并执行回调函数                                            |
| batch:close()                                            | 关闭 batch(关闭数据库前必须关闭 batch)                               |
| batch:delete(key)                                        | 删除 key                                                             |
| batch:clear()                                            | 清除 batch                                                           |
| batch:set_need_lock()                                    | 设置 batch 需要多线程锁(不在同一线程时需要加锁)                      |
| batch:get_int_param(id) / batch:set_int_param(id, value) | 设置 int 参数，用于多线程之间传递某些参数，支持 0-31 共 32 个参数    |
| batch:get_str_param(id) / batch:set_str_param(id, value) | 设置 string 参数，用于多线程之间传递某些参数，支持 0-31 共 32 个参数 |

| rawbatch 对象(leveldb::Batch)    | 说明       |
| -------------------------------- | ---------- |
| batch:put(key, val, [writeopts]) | 写入数据   |
| batch:delete(key)                | 删除 key   |
| batch:clear()                    | 清除 batch |

## Build

windows: 使用 visual studio 2017 打开项目编译
linux: make
