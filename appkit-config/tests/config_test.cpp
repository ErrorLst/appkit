// appkit::config 单元测试：覆盖解析既有行为与 expand 待实现功能点。
// 失败时打印"期望值 / 实际值"，便于直接定位差异。
#include "appkit/config.h"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <type_traits>

#ifndef APPKIT_CONFIG_TEST_DATA_DIR
#define APPKIT_CONFIG_TEST_DATA_DIR "."
#endif
#ifndef APPKIT_CONFIG_EXAMPLE_DIR
#define APPKIT_CONFIG_EXAMPLE_DIR "config"
#endif

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

/// 断言 expand 失败；若它居然成功了，把实际返回的字符串打出来。
static auto checkExpandFails(const appkit::Config &config, std::string_view section, std::string_view key,
                             appkit::ConfigError &error, int line) -> void {
    error = appkit::ConfigError{};
    const auto result = config.expand(section, key, error);
    if (!result.has_value()) {
        return;
    }
    std::cout << "CHECK_EXPAND_FAILS failed at line " << line << ": expand(\"" << section << "\", \"" << key << "\")\n";
    std::cout << "    expected: failure (std::nullopt)\n";
    std::cout << "    actual:   success, returned ";
    printValue(*result);
    std::cout << '\n';
    ++g_failures;
}

/// 断言 haystack 中含 needle；失败时两者都打印。
static auto checkContains(std::string_view haystack, std::string_view needle, int line) -> void {
    if (haystack.find(needle) != std::string_view::npos) {
        return;
    }
    std::cout << "CHECK_CONTAINS failed at line " << line << '\n';
    std::cout << "    expected: contains ";
    printQuoted(needle);
    std::cout << '\n';
    std::cout << "    actual:   ";
    printQuoted(haystack);
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
/// 取字符串值；缺失时返回空视图便于直接比较。
static auto value(const appkit::Config &config, std::string_view section, std::string_view key) -> std::string_view {
    return config.get(section, key).value_or(std::string_view{});
}

/// 以二进制读取整个文件；打开失败时打印错误行并计入失败。
static auto readFile(const std::filesystem::path &path) -> std::string {
    auto in = std::ifstream{path, std::ios::binary};
    if (!in) {
        std::cout << "CHECK failed: cannot read fixture " << path.string() << '\n';
        ++g_failures;
        return {};
    }
    return std::string{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
}

/// 数据目录下某个 fixture 的完整路径。
static auto dataPath(const char *name) -> std::filesystem::path {
    return std::filesystem::path{APPKIT_CONFIG_TEST_DATA_DIR} / name;
}

// ------------------------------ 用例 ------------------------------
static auto testParse() -> void {
    const auto text = readFile(dataPath("parse_basic.ini"));
    auto error = appkit::ConfigError{};
    const auto parsed = appkit::Config::parse(text, error);
    CHECK_EQ(parsed.has_value(), true);
    if (!parsed) {
        return;
    }
    const auto &config = *parsed;
    CHECK_EQ(config.size(), std::size_t{4});
    CHECK_EQ(value(config, "", "name"), std::string_view{"svcd"});
    CHECK_EQ(value(config, "", "spaced"), std::string_view{"trimmed"});
    CHECK_EQ(value(config, "server", "host"), std::string_view{"127.0.0.1"});
    CHECK_EQ(value(config, "server", "port"), std::string_view{"9090"});
    CHECK_EQ(config.has("server", "port"), true);
    CHECK_EQ(config.has("server", "absent"), false);
    CHECK_EQ(config.get("missing", "host").has_value(), false);

    auto bad = appkit::ConfigError{};
    const auto broken_line = readFile(dataPath("parse_broken_line.ini"));
    CHECK_EQ(appkit::Config::parse(broken_line, bad).has_value(), false);
    CHECK_EQ(bad.line, std::size_t{2});
    CHECK_EQ(bad.message.empty(), false);
    const auto broken_section = readFile(dataPath("parse_broken_section.ini"));
    CHECK_EQ(appkit::Config::parse(broken_section, bad).has_value(), false);
    CHECK_EQ(bad.line, std::size_t{1});
}

static auto testExpand() -> void {
    const auto text = readFile(dataPath("expand_ok.ini"));
    auto error = appkit::ConfigError{};
    const auto parsed = appkit::Config::parse(text, error);
    CHECK_EQ(parsed.has_value(), true);
    if (!parsed) {
        return;
    }
    const auto &config = *parsed;
    auto expand = appkit::ConfigError{};
    CHECK_EQ(config.expand("", "bin", expand).value_or(""), std::string{"/opt/svcd/bin"});
    CHECK_EQ(config.expand("", "deep", expand).value_or(""), std::string{"/opt/svcd/bin/deep"});
    CHECK_EQ(config.expand("server", "addr", expand).value_or(""), std::string{"127.0.0.1:8080"});
    CHECK_EQ(config.expand("client", "url", expand).value_or(""), std::string{"127.0.0.1:8080/api"});
    CHECK_EQ(config.expand("client", "alias", expand).value_or(""), std::string{"127.0.0.1:8080/api?name=client-a"});
    checkExpandFails(config, "", "absent", expand, __LINE__);
}

static auto testExpandErrors() -> void {
    const auto text = readFile(dataPath("expand_errors.ini"));
    auto error = appkit::ConfigError{};
    const auto parsed = appkit::Config::parse(text, error);
    CHECK_EQ(parsed.has_value(), true);
    if (!parsed) {
        return;
    }
    const auto &config = *parsed;

    auto cycle = appkit::ConfigError{};
    checkExpandFails(config, "", "a", cycle, __LINE__);
    CHECK_EQ(cycle.line, std::size_t{2});
    CHECK_EQ(cycle.message.empty(), false); // 文案不限，但必须给出可排障的说明
    checkExpandFails(config, "", "self", cycle, __LINE__);
    CHECK_EQ(cycle.line, std::size_t{3});

    auto missing = appkit::ConfigError{};
    checkExpandFails(config, "", "path", missing, __LINE__);
    CHECK_EQ(missing.line, std::size_t{4});
    CHECK_EQ(missing.message.empty(), false);
    checkExpandFails(config, "", "cross", missing, __LINE__);
    CHECK_EQ(missing.line, std::size_t{5});
    checkContains(missing.message, "db.user", __LINE__);

    auto syntax = appkit::ConfigError{};
    checkExpandFails(config, "", "empty", syntax, __LINE__);
    CHECK_EQ(syntax.line, std::size_t{6});
    checkExpandFails(config, "", "tail", syntax, __LINE__);
    checkExpandFails(config, "", "open", syntax, __LINE__);
    checkExpandFails(config, "", "bare", syntax, __LINE__);
}

/// svcd.ini 冒烟检查：示例配置可解析且取值正确；不调用 expand。
static auto testSvcdExample() -> void {
    const auto text = readFile(std::filesystem::path{APPKIT_CONFIG_EXAMPLE_DIR} / "svcd.ini");
    auto error = appkit::ConfigError{};
    const auto parsed = appkit::Config::parse(text, error);
    CHECK_EQ(parsed.has_value(), true);
    if (!parsed) {
        return;
    }
    const auto &config = *parsed;
    CHECK_EQ(config.has("server", "listen"), true);
    CHECK_EQ(value(config, "log", "level"), std::string_view{"info"});
}

auto main() -> int {
    std::cout << std::unitbuf; // 每次输出立即刷新：调试时即使提前中断也能看到已产生的输出
    testParse();
    testExpand();
    testExpandErrors();
    testSvcdExample();
    if (g_failures != 0) {
        std::cout << g_failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "all checks passed\n";
    return 0;
}
