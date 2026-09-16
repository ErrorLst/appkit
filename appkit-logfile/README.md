# appkit-logfile

## 模块简介

`svcd` 是一个用 C++20 编写的后台服务框架，代码组织在 `appkit::` 命名空间下，负责插件加载、任务编排与运行期配置。
本模块 `appkit::logfile` 是其中的运行日志写入器：以追加方式把调度线程与各插件产生的运行日志落到磁盘，并在当前文件达到大小上限时按 logrotate 习惯滚动出历史文件。
它只负责单文件族的写入与轮转，不做字符编码转换、文件权限/属主设置、线程安全或日志分级，这些由上层日志门面承担；上层日志门面在启动时创建一个 `RotatingFileLog`，之后只通过 `writeLine` 写入。

## 待实现功能点（`RotatingFileLog::rotate`）

`RotatingFileLog::rotate` 目前是直接返回 `{LogStatus::IO_ERROR, "RotatingFileLog::rotate is not implemented"}` 的桩，需要按下面的语义补全；`open`/`close`/`writeLine` 已实现，不要改动它们的行为。

### 返回值约定

`open` / `writeLine` / `rotate` 都返回 `std::pair<LogStatus, std::string>`：成功时第二个元素为空串，
失败时为一句话说明（纯 ASCII 英文短句）；`close` 无返回值。

### 功能目标

当前文件写入后大小达到上限 `max_bytes` 时，`writeLine` 会在写入新行之前调用 `rotate`，把已经写满的当前文件（`path`）滚动成一份历史文件，再以截断模式重新创建当前文件，使后续写入从一个空文件重新开始。

### 历史文件命名规则

沿用 logrotate 习惯：当前文件为 `svcd.log`，历史文件依次为 `svcd.log.1`、`svcd.log.2` ……。
序号越大越旧，`.1` 是最新的一份历史（最近一次滚动出来的文件），`path.keep` 是允许存在的最旧一份历史。

### 滚动顺序（关键，写错会丢历史）

设保留个数为 `keep`（`Options::keep`）。必须“先删最旧的，再从大到小逐级改名，最后改名当前文件”：

1. 先删除最旧的一份历史：`path.keep`。
2. 再从大到小逐级改名：`path.(keep-1)` → `path.keep`、`path.(keep-2)` → `path.(keep-1)`、……、`path.1` → `path.2`。
3. 最后把当前文件 `path` → `path.1`。

滚动只是一串改名（`std::filesystem::rename`）的级联，不读写文件内容，所以开销与日志文件大小无关；但它是一组多步操作，整组并不原子。

### `keep == 0` 的特例

不保留任何历史文件：直接删除当前文件 `path`，然后按下面的方式重建，不产生任何 `path.N` 历史文件。

### 文件系统操作

- 所有文件系统操作必须使用 `std::error_code` 重载，不抛异常。
- 源文件不存在不算错误：例如 `path.2` 尚不存在时，`path.1` → `path.2` 这一步直接跳过（或等价地忽略“文件不存在”这一错误码），继续处理后续改名。
- 除“源文件不存在”外的其它文件系统错误都算失败。

### 重新打开

滚动完成后，以截断模式重新打开 `path`，把当前文件大小计数归零、轮转次数计数自增 1。

### 失败处理

- 任一步骤失败都返回 `{LogStatus::IO_ERROR, 说明}`。
- 返回的说明文本要说明是“哪一步操作 + 哪个目标文件”失败（例如删除/改名/重开 `xxx` 失败的原因）。
- 必须保证“是否已打开”这个状态与实际情况一致：如果无法重新打开 `path`，则把该状态置为未打开。

### 约束

- 不得修改打开参数（`m_options`）。
- 不得让异常逃逸出 `rotate`。
- 仅使用 ISO C++20 标准库，跨平台；运行期错误文本为纯 ASCII。
- 不改动 `open`/`close`/`writeLine` 的既有行为。

### 验收

    cmake -S . -B build
    cmake --build build
    ctest --test-dir build --output-on-failure

构建必须在 `-Wall -Wextra -Wpedantic -Werror` 下零警告；测试实现后必须 100% 通过。
当前交付态有 12 条断言失败，全部落在 `rotate`（`testRotation` 与 `testKeepAndZero`）。
