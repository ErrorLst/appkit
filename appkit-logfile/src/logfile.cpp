#include "appkit/logfile.h"

#include <cerrno>
#include <filesystem>
#include <string>
#include <system_error>
#include <utility>

namespace appkit {

auto RotatingFileLog::open(Options options) -> std::pair<LogStatus, std::string> {
    if (m_open) {
        return {LogStatus::ALREADY_OPEN, "log file already open"};
    }
    if (options.path.empty() || options.max_bytes == 0) {
        return {LogStatus::INVALID_ARGUMENT, "invalid options: path must not be empty and maxBytes must be > 0"};
    }
    const auto parent = options.path.parent_path();
    auto ec = std::error_code{};
    if (!parent.empty() && !std::filesystem::is_directory(parent, ec)) {
        if (ec) {
            return {LogStatus::IO_ERROR, "log directory not accessible: " + parent.string() + ": " + ec.message()};
        }
        return {LogStatus::IO_ERROR, "log directory does not exist: " + parent.string()};
    }
    errno = 0;
    m_out.open(options.path, std::ios::out | std::ios::app | std::ios::binary);
    if (!m_out.is_open()) {
        const auto sys = std::error_code{errno, std::system_category()};
        return {LogStatus::IO_ERROR, "cannot open log file: " + options.path.string() + ": " + sys.message()};
    }
    const auto existing = std::filesystem::file_size(options.path, ec);
    if (ec) {
        m_out.close();
        m_out.clear();
        return {LogStatus::IO_ERROR, "cannot stat log file: " + options.path.string() + ": " + ec.message()};
    }
    m_options = std::move(options);
    m_size = existing;
    m_open = true;
    return {LogStatus::OK, {}};
}

auto RotatingFileLog::close() noexcept -> void {
    if (m_out.is_open()) {
        m_out.flush();
        m_out.close();
    }
    m_out.clear();
    m_open = false;
}

auto RotatingFileLog::isOpen() const noexcept -> bool { return m_open; }

auto RotatingFileLog::currentSize() const noexcept -> ByteCount { return m_size; }

auto RotatingFileLog::rotationCount() const noexcept -> std::size_t { return m_rotations; }

auto RotatingFileLog::getOptions() const noexcept -> const Options & { return m_options; }

auto RotatingFileLog::writeLine(std::string_view line) -> std::pair<LogStatus, std::string> {
    if (!m_open) {
        return {LogStatus::NOT_OPEN, "log file is not open"};
    }
    if (m_size >= m_options.max_bytes) {
        if (auto result = rotate(); result.first != LogStatus::OK) {
            return result;
        }
    }
    m_out << line << '\n';
    if (!m_out) {
        return {LogStatus::IO_ERROR, "failed to write log line"};
    }
    m_size += line.size() + 1;
    return {LogStatus::OK, {}};
}

#ifdef FLAG

auto RotatingFileLog::rotate() -> std::pair<LogStatus, std::string> {
    // TODO(appkit::logfile): 按 README 待实现功能点滚动历史文件并重开当前文件。
    return {LogStatus::IO_ERROR, "RotatingFileLog::rotate is not implemented"};
}

#else

/// 历史文件路径：base.N（base 为当前日志文件，N 越大越旧）。
static auto historyPath(const std::filesystem::path &base, std::size_t index) -> std::filesystem::path {
    return std::filesystem::path{base.string() + "." + std::to_string(index)};
}

auto RotatingFileLog::rotate() -> std::pair<LogStatus, std::string> {
    auto ec = std::error_code{};

    if (m_options.keep == 0) {
        close();
        std::filesystem::remove(m_options.path, ec);
        if (ec) {
            return {LogStatus::IO_ERROR, ""};
        }

        auto reopened = open(m_options);
        if (reopened.first == LogStatus::OK) {
            ++m_rotations;
        }
        return reopened;
    }

    const auto oldest = historyPath(m_options.path, m_options.keep);
    std::filesystem::remove(oldest, ec);
    if (ec) {
        return {LogStatus::IO_ERROR, ""};
    }

    for (auto index = m_options.keep; index > 1; --index) {
        const auto from = historyPath(m_options.path, index - 1);
        const auto to = historyPath(m_options.path, index);
        std::filesystem::rename(from, to, ec);
        if (ec) {
            if (ec == std::errc::no_such_file_or_directory) {
                ec.clear(); // 源文件不存在：跳过这一步，不算失败
                continue;
            }
            close(); // 失败后半途的历史已改动，就地关流，避免调用方继续写时反复轮转
            return {LogStatus::IO_ERROR, ""};
        }
    }

    close();
    const auto first = historyPath(m_options.path, 1);
    std::filesystem::rename(m_options.path, first, ec);
    if (ec) {
        return {LogStatus::IO_ERROR, ""};
    }

    auto reopened = open(m_options);
    if (reopened.first == LogStatus::OK) {
        ++m_rotations;
    }
    return reopened;
}

#endif

} // namespace appkit
