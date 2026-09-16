# appkit-config 代码提问清单

## 1. 跨平台与文本处理

### Q1 换行符：只认 '\n'

位置：`src/config.cpp:29`（相关：`:14`）

```cpp
const auto end = text.find('\n', begin);
```

1. Linux 和 Windows 的换行分别是什么？
2. 读一个 CRLF（Windows）写的文件时，每行末尾会多出一个什么字符？它最后是谁去掉的？
3. 有些编辑器（Windows 上很常见）会在文件开头写 UTF-8 BOM，也就是开头多出 `EF BB BF` 三个字节——如果拿到这样一个文件，会出现什么现象？

### Q2 值两端的空白被裁掉了

位置：`src/config.cpp:51`

```cpp
const auto value = trim(line.substr(eq + 1));
```

1. 写 `port = 8080   `（值尾部有空格）时，`get` 出来的是什么？
2. 如果某个值**必须**保留首尾空白（密码、SQL 片段、正则、带空格的路径），这个实现会怎么坏？用户有办法表达吗？

## 2. 边界与无符号数

### Q3 这行为什么不能省

位置：`src/config.cpp:15-20`、`:30`

```cpp
if (first == std::string_view::npos) { return {}; }
const auto last = text.find_last_not_of(SPACE);
return text.substr(first, last - first + 1);
// ...
const auto line = trim(text.substr(begin, end - begin));
```

（前提：`npos` 是 `std::string_view::npos`，值等于 `size_t` 的最大值，表示"找不到"；无符号整数里没有负数。）

1. 把 `if` 那三行删掉，输入全是空白时会发生什么？
2. 另一处 `substr(begin, end - begin)` 里的 `end` 也可能是 `npos`——为什么这里不用判断，也不会抛异常？

### Q4 循环边界

位置：`src/config.cpp:28-30`、`:61-64`

```cpp
while (begin <= text.size()) {
    const auto end = text.find('\n', begin);
    // ...
    if (end == std::string_view::npos) { break; }
    begin = end + 1;
}
```

1. `begin` 会走到 `text.size()`。这时调用 `substr(begin, ...)`、`find('\n', begin)` 算越界吗？为什么？
2. 空文本 `""` 会解析失败吗？循环靠什么退出，会不会死循环？
3. 这里用 `<` 和用 `<=` 有区别吗？为什么？

## 3. 复杂度、所有权与接口约定

### Q5 全表线性扫描

位置：`src/config.cpp:53-54`（`get` 的查找在 `:70`）

```cpp
if (const auto it = std::ranges::find_if(config.m_entries, same); it != config.m_entries.end()) { ... }
```

1. 每解析一行都要扫一遍已有条目，解析 n 行的时间复杂度是多少？
2. 配置一般几十到几百行、只在启动时解析一次。这个开销需要处理吗？为什么？

### Q6 所有权边界：三个签名为什么不一样

位置：`include/appkit/config.h:27-32`、`:39-40`、`:49-50`

```cpp
struct Entry { std::string section, key, value; std::size_t line; };  // 自己持有
[[nodiscard]] auto get(...) const noexcept -> std::optional<std::string_view>;  // 借出去
[[nodiscard]] auto expand(...) const -> std::optional<std::string>;              // 造一份新的
```

1. `parse` 的入参是 `string_view`，为什么 `Entry` 必须持有 `std::string`？
2. `Config` 被拷贝之后，之前 `get` 出来的视图指向谁？
3. `expand` 为什么必须返回 `std::string`，不能借视图？

## 4. 错误模型、诊断与并发

### Q7 错误模型：为什么不用异常

位置：`include/appkit/config.h:36`（实现 `src/config.cpp:23-67`）

```cpp
[[nodiscard]] static auto parse(std::string_view text, ConfigError &error)
    -> std::optional<Config>;
```

1. 调用方是启动期的插件加载器（拿到错误要写日志、再决定是否继续启动）。这个前提下，为什么用"返回值 + 出参错误"而不是抛异常？
2. 如果要求"一次列出所有坏行"，现在"遇到第一个错就返回"能满足吗？要改什么？
3. C++23 的 `std::expected` 能直接解决第 2 问吗？为什么？

### Q8 不可变快照与线程安全

位置：`include/appkit/config.h:39-50`（查询接口）、`:65-66`（`m_entries`）

1. 解析完成后不可变、查询全是 `const`：多个线程同时 `get` / `has` / `expand` 安全吗？为什么不用加锁？
2. 如果给 `find` 加一个 `mutable` 的惰性缓存，会出现什么问题？

