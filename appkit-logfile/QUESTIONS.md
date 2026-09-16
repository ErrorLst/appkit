# appkit-logfile 代码提问清单

## 1. 落盘时机、缓冲与 RAII

### Q1 这一行日志此刻在哪儿

位置：`src/logfile.cpp:70`（相关：`:44-51`）

```cpp
m_out << line << '\n';
```

1. 这行执行完之后，日志内容和 `m_size` 分别在哪儿？这时另一个进程去读这个文件，能读到它吗？
2. 进程被 `kill -9`（不走析构、不执行任何清理代码）时，还在缓冲区里的内容会怎样？
3. `close()`（`:44-51`）里那一行 `flush()` 删掉，行为会变吗？为什么？

### Q2 析构、close 与 noexcept

位置：`src/logfile.cpp:44-51`、`include/appkit/logfile.h:37`

```cpp
~RotatingFileLog() { close(); }
auto close() noexcept -> void;
```

1. 用户从头到尾不手动调用 `close()`，日志会丢吗？
2. 头文件注释说 `close()`"可重复调用"——为什么这个约定必须成立？
3. `close()` 里会 `flush()`，失败也可能出错，为什么还能标 `noexcept`？

## 2. 大小计数与边界

### Q3 `+ 1` 是什么，上限又能超多少

位置：`src/logfile.cpp:74`（相关：`:65-69`）

```cpp
if (m_size >= m_options.max_bytes) { ... }  // :65，写这一行之前先判断
m_out << line << '\n';
m_size += line.size() + 1;                  // :74
```

1. `line.size() + 1` 里那个 `+ 1` 对应实际写出的哪个字节？
2. 如果传进来的 `line` 自己就带 `'\n'`（比如一次写多行），`m_size` 还准吗？
3. 判断放在"写这一行之前"，所以文件可以超过 `max_bytes`。最多能超多少？

### Q4 打开已有文件的两个细节

位置：`src/logfile.cpp:26-39`

```cpp
errno = 0;
m_out.open(options.path, std::ios::out | std::ios::app | std::ios::binary);
// ...
const auto existing = std::filesystem::file_size(options.path, ec);
// ...
m_size = existing;
```

1. 文件已经存在、而且已经比 `max_bytes` 大（比如上次退出时没轮转），`open` 会怎么处理？
2. `errno = 0;` 为什么要显式清零？

## 3. 接口约定与状态一致性

### Q5 为什么不可拷贝、也不可移动

位置：`include/appkit/logfile.h:38-41`

```cpp
RotatingFileLog(const RotatingFileLog &) = delete;
auto operator=(const RotatingFileLog &) -> RotatingFileLog & = delete;
RotatingFileLog(RotatingFileLog &&) = delete;
auto operator=(RotatingFileLog &&) -> RotatingFileLog & = delete;
```

1. 删除拷贝构造和拷贝赋值，防的是什么？
2. 如果把移动构造和移动赋值改成 `= default`，会发生什么？

### Q6 成员状态哪些能省

位置：`include/appkit/logfile.h:74-78`、`src/logfile.cpp:44-51`

```cpp
Options m_options{};
std::ofstream m_out;
ByteCount m_size = 0;
std::size_t m_rotations = 0;
bool m_open = false;
```

1. `m_open` 能不能用 `m_out.is_open()` 代替？
2. `close()` 里的 `m_out.clear()` 清的是什么？会把文件清空吗？
3. `close()` 之后再 `open()`，`currentSize()` 和 `rotationCount()` 各自怎么变？

## 4. 失败处理与并发

### Q7 失败处理与线程安全

位置：`src/logfile.cpp:65-69`（相关：`include/appkit/logfile.h:71`、`README.md:7`）

```cpp
if (m_size >= m_options.max_bytes) {
    if (auto result = rotate(); result.first != LogStatus::OK) {
        return result;
    }
}
```

1. 轮转失败时 `writeLine` 直接返回错误、这一行**不写**。为什么不"先写下去、下次再轮转"？
2. 两个线程同时调用 `writeLine` 会发生什么？这个类里哪些状态会被竞争？
3. 模块简介把线程安全划给了"上层日志门面"。这个责任切分合理吗？要让本类自己保证，最小改动是什么？
