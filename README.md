封装 leveldb 库给 lua 使用

## 特性

- 允许打开同一份 db 文件多次。
- 扩展 leveldb::Batch，支持 get 方法获取 put 到 batch 中的 value，支持多线程访问。
- 扩展 leveldb::WriteOptions，添加 compress 选项设置 value 需要压缩后写入 db。
- 扩展 leveldb::ReadOptions，添加 decompress 选项设置 value 需要解压后返回。
- 提供 miniz 压缩方法和 base64 编码方法。

## API

| 函数名称                       | 说明                       |
| :----------------------------- | :------------------------- |
| lualeveldb.open(path)          | 打开数据库文件，返回db对象 |
| lualeveldb.close(db)           | 关闭db对象                 |
| lualeveldb.options()           | 创建数据库选项对象         |
| lualeveldb.readOptions()       | 创建读取选项对象           |
| lualeveldb.writeOptions()      | 创建写入选项对象           |
| lualeveldb.repair(path)        | 修复数据库文件             |
| lualeveldb.rawbatch()          | 创建leveldb::Batch对象     |
| lualeveldb.mz_compress(data)   | 压缩给定数据               |
| lualeveldb.mz_decompress(data) | 解压给定数据               |
| lualeveldb.base64encode(data)  | base64 encode              |
| lualeveldb.base64decode(data)  | base64 decode              |

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

| db对象                         | 说明           |
| :----------------------------- | -------------- |
| ldb:put(key, val, [writeopts]) | 写入数据       |
| ldb:get(key, [readopts])       | 获取数据       |
| ldb:batch()                    | 创建batch      |
| ldb:close()                    | 关闭数据库     |
| ldb:has(key)                   | key是否存在    |
| ldb:delete(key)                | 删除key        |
| ldb:iterator()                 | 创建迭代器对象 |
| ldb:write(batch)               | 写入batch      |
| ldb:snapshot()                 | 创建snapshot   |

| 迭代器对象                   | 说明                     |
| :--------------------------- | ------------------------ |
| iterator:del()               | 删除迭代器对象           |
| iterator:seek(prefix)        | seek到指定位置           |
| iterator:seekToFirst()       | seek到数据库起始位置     |
| iterator:seekToLast()        | seek到数据库最后一个位置 |
| iterator:valid()             | 迭代器是否有效           |
| iterator:next()              | 移动到下一个位置         |
| iterator:prev()              | 移动到前一个位置         |
| iterator:key()               | 获取key                  |
| iterator:value([uncompress]) | 获取value                |

| batch对象                                    | 说明                                       |
| -------------------------------------------- | ------------------------------------------ |
| batch:put(key, val, [writeopts])             | 写入数据                                   |
| batch:get(key, [readopts])                   | 获取数据                                   |
| batch:lock(cb)                               | 锁定batch并执行回调函数                    |
| batch:close()                                | 关闭batch                                  |
| batch:delete(key)                            | 删除key                                    |
| batch:clear()                                | 清除batch                                  |
| batch:set_need_lock()                        | 设置batch需要多线程锁                      |
| get_int_param(id) / set_int_param(id, value) | 设置int参数，用于多线程之间传递某些参数    |
| get_str_param(id) / set_str_param(id, value) | 设置string参数，用于多线程之间传递某些参数 |

| rawbatch对象(leveldb::Batch)     | 说明      |
| -------------------------------- | --------- |
| batch:put(key, val, [writeopts]) | 写入数据  |
| batch:delete(key)                | 删除key   |
| batch:clear()                    | 清除batch |



## Build

windows: 使用 visual studio 2017 打开项目编译
linux: make