# appkit-config

## 模块简介

`svcd` 是一个用 C++20 编写的后台服务框架，代码组织在 `appkit::` 命名空间下，负责插件加载、任务编排与运行期配置。
本模块 `appkit::config` 是其中的只读 INI 配置读取组件，由插件启动器在加载期调用，框架其余模块只通过 `get/has/expand` 查询。
它只解析调用方读入内存的文本（默认配置示例见 `config/svcd.ini`，测试夹具在 `tests/data/`），提供 `[section]`、`key = value` 解析与 `${...}` 引用展开。
模块不做文件 IO、不感知路径与字符编码，读取文件与编码处理由调用方负责；仅使用 ISO C++20 标准库。

## 待实现功能点（`Config::expand`）

实现 `Config::expand(section, key, error)`：取出 `(section, key)` 条目的原始值，把其中的 `${...}` 引用
递归替换为最终字符串并返回；任一步失败都写入 `error` 并返回 `nullopt`。

### 引用语法

值里可以出现两类引用，`$` 后面必须紧跟 `{`，以 `}` 结束：

| 写法 | 含义 | 例（取自 `tests/data/expand_ok.ini` 的 `[server]` 节：`addr = ${host}:${port}`） |
| --- | --- | --- |
| `${key}` | 在**引用所在条目自己的节**里查 `key` | `${host}` 取 `[server] host`，得到 `127.0.0.1` |
| `${section.key}` | 按**第一个** `.` 拆成节名与键名，跨节查 | `${server.addr}` 取 `[server] addr` |

两个容易搞错的点：

1. `${key}` 的"当前节"是**引用所在条目**的节，**不是**被展开的那个键所在的节。
   例：展开 `[client] url` 时遇到 `${server.addr}`，进去后 `addr` 属于 `[server]`，所以它里面的 `${host}` 查的是 `[server] host`。
2. 若引用所在条目属于**全局节**（文件里没有 `[section]` 头），它的节名是空串，`${key}` 就在空串节里查。

### 展开示例：输入 → 输出

**输入**（调用方读入内存的文本，内容与 `tests/data/expand_ok.ini` 相同；行号自上而下为 1 ~ 11）：

```ini
prefix = /opt/svcd
bin = ${prefix}/bin
deep = ${bin}/deep
[server]
host = 127.0.0.1
port = 8080
addr = ${host}:${port}
[client]
name = client-a
url = ${server.addr}/api
alias = ${url}?name=${name}
```

**调用与输出**：

```cpp
auto error = appkit::ConfigError{};
const auto config = appkit::Config::parse(text, error);   // 解析上面的文本

config->expand("",       "bin",    error);  // 输入 ("", "bin")        输出 "/opt/svcd/bin"
config->expand("",       "deep",   error);  // 输入 ("", "deep")       输出 "/opt/svcd/bin/deep"
config->expand("server", "addr",   error);  // 输入 ("server", "addr") 输出 "127.0.0.1:8080"
config->expand("client", "url",    error);  // 输入 ("client", "url")  输出 "127.0.0.1:8080/api"
config->expand("client", "alias",  error);  // 输入 ("client", "alias")输出 "127.0.0.1:8080/api?name=client-a"
config->expand("",       "absent", error);  // 输入 ("", "absent")     输出 std::nullopt（没有这个键）
```

**逐层看 `expand("client", "alias")`**：

| 步骤 | 当前文本 | 查找规则 |
| --- | --- | --- |
| 起始 | `${url}?name=${name}` | `alias` 在 `[client]` 节 |
| 展开 `${url}` | `${server.addr}/api` | 无点号 → 在 `[client]` 找 `url` |
| 展开 `${server.addr}` | `${host}:${port}` | 有点号 → 跨节取 `[server] addr` |
| 展开 `${host}` | `127.0.0.1` | 无点号 → 在**引用所在条目 `addr` 自己的节** `[server]` 找 |
| 展开 `${port}` | `8080` | 同上 |
| 展开 `${name}` | `client-a` | 无点号 → 在 `[client]` 找 |
| 最终 | `127.0.0.1:8080/api?name=client-a` | 文本里不再含 `$` 即返回 |

### 语义

- 递归展开：被引用键的值里若仍含 `${...}`，要继续展开，直到得到不含引用的最终字符串。
- 引用不存在：返回 `nullopt`，`error` 给出诊断。
- 循环引用（含 `self = ${self}` 自引用）：返回 `nullopt`，`error` 给出诊断。

### 非法语法

以下写法均返回 `nullopt` 并写入 `error`：

- `${}`：引用名为空。
- `${a.}`：`.` 后的键名为空。
- `${.a}`：`.` 前的节名为空。
- `${a`：只有 `$` 与 `{`，没有闭合的 `}`。
- 裸 `$`（如 `$notaref`）：`$` 之后不是 `{`。

### 诊断契约（错误行号与 message）

`expand` 失败时通过 `ConfigError` 上报，调用方（插件启动器）会把它写进启动日志，所以**行号必须准确**：

- **行号一律取"出错引用所在条目"的行号**，而不是被展开的那个键的行号：
  - 引用不存在 → 报**引用所在条目**的行号；
  - 循环引用 → 报**闭环处**那条引用所在条目的行号
    （`a = ${b}` / `b = ${a}`，展开 `a` 时报 `b` 的行号；`self = ${self}` 报自己的行号）；
  - 非法引用语法 → 报该条目行号。
- `error.message` 用**纯 ASCII 英文短句**；引用缺失键时**必须把缺失的引用文本写进 message**
  （例如 `db.user`），便于排障。

**失败示例：输入 → 输出**

**输入**（内容与 `tests/data/expand_errors.ini` 相同；行号自上而下为 1 ~ 9）：

```ini
a = ${b}
b = ${a}
self = ${self}
path = ${missing}/x
cross = ${db.user}@host
empty = ${}
tail = ${a.}
open = ${a
bare = $notaref
```

| 调用 | 返回 | `error.line` | `error.message` |
| --- | --- | --- | --- |
| `expand("", "a", error)` | `std::nullopt` | `2` | 循环引用说明：`a → b → a`，报闭环处 `${a}` 所在条目 `b` 的行号 |
| `expand("", "self", error)` | `std::nullopt` | `3` | 循环引用说明（自己引用自己） |
| `expand("", "path", error)` | `std::nullopt` | `4` | 含缺失的引用文本 `missing` |
| `expand("", "cross", error)` | `std::nullopt` | `5` | 含缺失的引用文本 `db.user` |
| `expand("", "empty", error)` | `std::nullopt` | `6` | 说明引用名为空 |
| `expand("", "tail", error)` | `std::nullopt` | `7` | 说明键名为空 |
| `expand("", "open", error)` | `std::nullopt` | `8` | 说明 `$` 与 `{` 后缺闭合 |
| `expand("", "bare", error)` | `std::nullopt` | `9` | 说明 `$` 之后不是 `{` |

### 约束

- 函数保持 `const`，不改动成员变量（`class Config` 的成员保留 `m_` 前缀）。
- 仅使用 ISO C++20 标准库，平台无关；运行期错误文本为纯 ASCII。
- 递归时注意深层引用链与循环，不能死循环。

### 验收

    cmake -S . -B build
    cmake --build build
    ctest --test-dir build --output-on-failure

实现后必须 100% 通过；当前交付态有 11 条断言失败，全部落在 `expand`（`testExpand` 与 `testExpandErrors`）。
