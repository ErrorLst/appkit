// appkit::logfile 单元测试：覆盖既有行为与 rotate 待实现功能点。
// 失败时打印"期望值 / 实际值"，便于直接定位差异。
#include "appkit/logfile.h"

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>

static auto g_failures = int{0};

// ------------------------------ 失败输出 ------------------------------
/// 打印带引号的文本；控制字符转成 \n \r \t，便于看清空串与不可见字符。
static auto printQuoted(std::string_view text) -> void {
    std::cout << '"';
    for (const auto ch : text) {
        switch (ch) {
        case '\n':
            std::cout << "\\n";
            break;
        case '\r':
            std::cout << "\\r";
            break;
        case '\t':
            std::cout << "\\t";
            break;
        default:
            std::cout << ch;
            break;
        }
    }
    std::cout << '"';
}

/// LogStatus 的可读名字，便于失败输出定位。
static auto statusName(appkit::LogStatus status) -> const char * {
    switch (status) {
    case appkit::LogStatus::OK:
        return "OK";
    case appkit::LogStatus::ALREADY_OPEN:
        return "ALREADY_OPEN";
    case appkit::LogStatus::NOT_OPEN:
        return "NOT_OPEN";
    case appkit::LogStatus::INVALID_ARGUMENT:
        return "INVALID_ARGUMENT";
    case appkit::LogStatus::IO_ERROR:
        return "IO_ERROR";
    }
    return "unknown";
}

/// 打印 LogStatus；重载优先于下面的模板。
static auto printValue(appkit::LogStatus status) -> void { std::cout << statusName(status); }

/// 打印一个值：bool 打 true/false，字符串类加引号，其余直接输出。
template <class T> static auto printValue(const T &value) -> void {
    if constexpr (std::is_same_v<T, bool>) {
        std::cout << (value ? "true" : "false");
    } else if constexpr (std::is_convertible_v<T, std::string_view>) {
        printQuoted(std::string_view{value});
    } else {
        std::cout << value;
    }
}

/// 打印一处 CHECK_EQ 失败：期望值 + 实际值。
template <class A, class E>
static auto reportMismatch(int line, const char *expr, const A &actual, const E &expected) -> void {
    std::cout << "CHECK_EQ failed at line " << line << ": " << expr << '\n';
    std::cout << "    expected: ";
    printValue(expected);
    std::cout << '\n';
    std::cout << "    actual:   ";
    printValue(actual);
    std::cout << '\n';
    ++g_failures;
}

#define CHECK_EQ(actual, expected)                                                                                     \
    do {                                                                                                               \
        const auto &actual_ = (actual);                                                                                \
        const auto &expected_ = (expected);                                                                            \
        if (!(actual_ == expected_)) {                                                                                 \
            reportMismatch(__LINE__, #actual, actual_, expected_);                                                     \
        }                                                                                                              \
    } while (false)

// ------------------------------ 测试辅助 ------------------------------
/// 取出状态码，并把说明存到 message，保持断言里"调用 + 断言"一行写完。
static auto statusOf(const std::pair<appkit::LogStatus, std::string> &result, std::string &message)
    -> appkit::LogStatus {
    message = result.second;
    return result.first;
}

/// 读取整个文件内容；文件不存在或打不开时返回空串，让 CHECK_EQ 打印出差异。
static auto readAll(const std::filesystem::path &path) -> std::string {
    auto in = std::ifstream{path, std::ios::binary};
    return std::string{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
}

/// 轮转后的历史文件路径，例如 app.log.1。
static auto historyPath(const std::filesystem::path &base, std::size_t index) -> std::filesystem::path {
    return std::filesystem::path{base.string() + "." + std::to_string(index)};
}

/// 独占的临时目录，析构时清理，保证测试互不干扰且可重复。
struct TempDir {
    std::filesystem::path root;
    TempDir() {
        auto ec = std::error_code{};
        const auto seed = std::chrono::steady_clock::now().time_since_epoch().count();
        root = std::filesystem::temp_directory_path(ec) / ("appkit_logfile_test_" + std::to_string(seed));
        std::filesystem::create_directories(root, ec);
    }
    ~TempDir() {
        auto ec = std::error_code{};
        std::filesystem::remove_all(root, ec);
    }
    TempDir(const TempDir &) = delete;
    auto operator=(const TempDir &) -> TempDir & = delete;
};

// ------------------------------ 用例 ------------------------------
static auto testAppendAndSize() -> void {
    const auto dir = TempDir{};
    const auto path = dir.root / "app.log";
    auto log = appkit::RotatingFileLog{};
    auto error = std::string{};
    CHECK_EQ(statusOf(log.open({path, 1u << 20, 3}), error), appkit::LogStatus::OK);
    CHECK_EQ(statusOf(log.writeLine("hello"), error), appkit::LogStatus::OK);
    CHECK_EQ(statusOf(log.writeLine(std::string{"world"}), error), appkit::LogStatus::OK);
    CHECK_EQ(log.currentSize(), appkit::ByteCount{12});
    const auto &options = log.getOptions();
    CHECK_EQ(options.path, path);
    CHECK_EQ(options.max_bytes, appkit::ByteCount{1u << 20});
    CHECK_EQ(options.keep, std::size_t{3});
    log.close();
    CHECK_EQ(log.isOpen(), false);
    CHECK_EQ(statusOf(log.open({path, 1u << 20, 3}), error), appkit::LogStatus::OK); // 追加打开已存在文件
    CHECK_EQ(log.currentSize(), appkit::ByteCount{12});
    log.close();
    CHECK_EQ(readAll(path), std::string{"hello\nworld\n"});
}

static auto testRotation() -> void {
    const auto dir = TempDir{};
    const auto path = dir.root / "rotate.log";
    auto log = appkit::RotatingFileLog{};
    auto error = std::string{};
    CHECK_EQ(statusOf(log.open({path, 10, 2}), error), appkit::LogStatus::OK);
    CHECK_EQ(statusOf(log.writeLine("AAAA"), error), appkit::LogStatus::OK);
    CHECK_EQ(statusOf(log.writeLine("BBBB"), error), appkit::LogStatus::OK);
    CHECK_EQ(log.currentSize(), appkit::ByteCount{10});
    CHECK_EQ(statusOf(log.writeLine("CCCC"), error), appkit::LogStatus::OK); // 已达上限，写前先轮转
    CHECK_EQ(log.rotationCount(), std::size_t{1});
    CHECK_EQ(log.currentSize(), appkit::ByteCount{5});
    log.close(); // 关闭后缓冲内容才落盘，才能读取当前文件
    CHECK_EQ(readAll(historyPath(path, 1)), std::string{"AAAA\nBBBB\n"});
    CHECK_EQ(readAll(path), std::string{"CCCC\n"});
}

static auto testKeepAndZero() -> void {
    const auto dir = TempDir{};
    auto error = std::string{};
    {
        const auto path = dir.root / "keep.log";
        auto log = appkit::RotatingFileLog{};
        CHECK_EQ(statusOf(log.open({path, 6, 1}), error), appkit::LogStatus::OK);
        for (const auto *line : {"L1", "L2", "L3", "L4", "L5"}) {
            CHECK_EQ(statusOf(log.writeLine(line), error), appkit::LogStatus::OK);
        }
        CHECK_EQ(log.rotationCount(), std::size_t{2});
        CHECK_EQ(readAll(historyPath(path, 1)), std::string{"L3\nL4\n"});
        CHECK_EQ(std::filesystem::exists(historyPath(path, 2)), false);
    }
    {
        const auto path = dir.root / "zero.log";
        auto log = appkit::RotatingFileLog{};
        CHECK_EQ(statusOf(log.open({path, 6, 0}), error), appkit::LogStatus::OK);
        CHECK_EQ(statusOf(log.writeLine("M1"), error), appkit::LogStatus::OK);
        CHECK_EQ(statusOf(log.writeLine("M2"), error), appkit::LogStatus::OK);
        CHECK_EQ(statusOf(log.writeLine("M3"), error), appkit::LogStatus::OK);
        log.close();
        CHECK_EQ(std::filesystem::exists(historyPath(path, 1)), false);
        CHECK_EQ(std::filesystem::exists(historyPath(path, 2)), false);
        CHECK_EQ(readAll(path), std::string{"M3\n"});
    }
}

static auto testOpenAndStatusErrors() -> void {
    const auto dir = TempDir{};
    auto log = appkit::RotatingFileLog{};
    auto error = std::string{};
    CHECK_EQ(statusOf(log.writeLine("x"), error), appkit::LogStatus::NOT_OPEN);
    CHECK_EQ(statusOf(log.open({dir.root / "missing" / "a.log", 64, 2}), error), appkit::LogStatus::IO_ERROR);
    CHECK_EQ(error.empty(), false);
    CHECK_EQ(log.isOpen(), false);
    CHECK_EQ(statusOf(log.open({}), error), appkit::LogStatus::INVALID_ARGUMENT);
    CHECK_EQ(statusOf(log.open({dir.root / "b.log", 0, 2}), error), appkit::LogStatus::INVALID_ARGUMENT);
    const auto path = dir.root / "dup.log";
    CHECK_EQ(statusOf(log.open({path, 64, 2}), error), appkit::LogStatus::OK);
    CHECK_EQ(log.isOpen(), true);
    CHECK_EQ(statusOf(log.open({path, 64, 2}), error), appkit::LogStatus::ALREADY_OPEN);
}

static auto testRaiiFlush() -> void {
    const auto dir = TempDir{};
    const auto path = dir.root / "raii.log";
    {
        auto log = appkit::RotatingFileLog{};
        auto error = std::string{};
        CHECK_EQ(statusOf(log.open({path, 64, 2}), error), appkit::LogStatus::OK);
        CHECK_EQ(statusOf(log.writeLine("flushed"), error), appkit::LogStatus::OK);
    } // 析构自动 close 并 flush
    CHECK_EQ(readAll(path), std::string{"flushed\n"});
}

auto main() -> int {
    std::cout << std::unitbuf; // 每次输出立即刷新：调试时即使提前中断也能看到已产生的输出
    testAppendAndSize();
    testRotation();
    testKeepAndZero();
    testOpenAndStatusErrors();
    testRaiiFlush();
    if (g_failures != 0) {
        std::cout << g_failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "all checks passed\n";
    return 0;
}
