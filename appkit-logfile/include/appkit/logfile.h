#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>

namespace appkit {

/// 日志写入结果状态码。
enum class LogStatus { OK, ALREADY_OPEN, NOT_OPEN, INVALID_ARGUMENT, IO_ERROR };

/// 字节计数别名，统一文件大小与上限的类型。
using ByteCount = std::uintmax_t;

/// 可作为日志行文本传入的类型。
template <class T>
concept LineText = std::convertible_to<T, std::string_view>;

/// 按大小滚动的日志文件：svcd 运行日志写入器。
/// 轮转命名沿用 logrotate 习惯：app.log、app.log.1、app.log.2 …（序号越大越旧）。
class RotatingFileLog final {
  public:
    /// 打开参数。
    struct Options {
        std::filesystem::path path;              ///< 当前日志文件路径
        ByteCount max_bytes = DEFAULT_MAX_BYTES; ///< 单文件上限，达到即轮转
        std::size_t keep = DEFAULT_KEEP;         ///< 保留的历史文件个数（0 = 不保留）
    };

  public:
    RotatingFileLog() = default;
    ~RotatingFileLog() { close(); }
    RotatingFileLog(const RotatingFileLog &) = delete;
    auto operator=(const RotatingFileLog &) -> RotatingFileLog & = delete;
    RotatingFileLog(RotatingFileLog &&) = delete;
    auto operator=(RotatingFileLog &&) -> RotatingFileLog & = delete;

    /// 以追加方式打开；参数非法返回 INVALID_ARGUMENT。说明文本随状态码一起返回，成功时为空串。
    [[nodiscard]] auto open(Options options) -> std::pair<LogStatus, std::string>;

    /// flush 并关闭；可重复调用。
    auto close() noexcept -> void;

    /// 是否处于打开状态。
    [[nodiscard]] auto isOpen() const noexcept -> bool;

    /// 写一行（自动补 '\n'）；写前若已达上限则先轮转。
    [[nodiscard]] auto writeLine(std::string_view line) -> std::pair<LogStatus, std::string>;

    /// 便捷重载：接受任意可转换为 string_view 的文本。
    template <LineText T> [[nodiscard]] auto writeLine(const T &line) -> std::pair<LogStatus, std::string> {
        return writeLine(std::string_view{line});
    }

    ///< 当前文件字节数
    [[nodiscard]] auto currentSize() const noexcept -> ByteCount;

    ///< 已发生的轮转次数
    [[nodiscard]] auto rotationCount() const noexcept -> std::size_t;

    ///< 当前生效的参数
    [[nodiscard]] auto getOptions() const noexcept -> const Options &;

  private:
    /// 关闭当前流，按 logrotate 语义滚动历史文件，再以截断模式重开当前文件。
    auto rotate() -> std::pair<LogStatus, std::string>;

  private:
    Options m_options{};
    std::ofstream m_out;
    ByteCount m_size = 0;
    std::size_t m_rotations = 0;
    bool m_open = false;

    static constexpr auto DEFAULT_MAX_BYTES = ByteCount{1u << 20}; ///< 单文件默认上限
    static constexpr auto DEFAULT_KEEP = std::size_t{3};           ///< 默认保留的历史文件个数
};

} // namespace appkit
