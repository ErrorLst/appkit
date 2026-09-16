#include "appkit/config.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace appkit {

/// 裁剪首尾空白；全空白返回空视图。
static constexpr auto trim(std::string_view text) noexcept -> std::string_view {
    constexpr auto SPACE = std::string_view{" \t\r\v\f"};
    const auto first = text.find_first_not_of(SPACE);
    if (first == std::string_view::npos) {
        return {};
    }
    const auto last = text.find_last_not_of(SPACE);
    return text.substr(first, last - first + 1);
}

auto Config::parse(std::string_view text, ConfigError &error) -> std::optional<Config> {
    auto config = Config{};
    auto section = std::string{};
    auto line_number = std::size_t{0};
    auto begin = std::size_t{0};
    while (begin <= text.size()) {
        const auto end = text.find('\n', begin);
        const auto line = trim(text.substr(begin, end - begin));
        ++line_number;
        const auto skip = line.empty() || line.starts_with('#') || line.starts_with(';');
        if (!skip && line.starts_with('[')) {
            const auto close = line.find(']');
            if (close == std::string_view::npos || !trim(line.substr(close + 1)).empty()) {
                error = {line_number, "malformed section header"};
                return std::nullopt;
            }
            section = std::string{trim(line.substr(1, close - 1))};
            if (section.empty()) {
                error = {line_number, "empty section name"};
                return std::nullopt;
            }
        } else if (!skip) {
            const auto eq = line.find('=');
            const auto key = eq == std::string_view::npos ? std::string_view{} : trim(line.substr(0, eq));
            if (eq == std::string_view::npos || key.empty()) {
                error = {line_number, "expected 'key = value'"};
                return std::nullopt;
            }
            const auto value = trim(line.substr(eq + 1));
            // 同键重复出现时由后者覆盖，并保留最后一行的行号。
            const auto same = [&](const auto &e) { return e.section == section && e.key == key; };
            if (const auto it = std::ranges::find_if(config.m_entries, same); it != config.m_entries.end()) {
                it->value = std::string{value};
                it->line = line_number;
            } else {
                config.m_entries.push_back(Entry{section, std::string{key}, std::string{value}, line_number});
            }
        }
        if (end == std::string_view::npos) {
            break;
        }
        begin = end + 1;
    }
    return config;
}

auto Config::get(std::string_view section, std::string_view key) const noexcept -> std::optional<std::string_view> {
    if (const auto *entry = find(section, key); entry != nullptr) {
        return std::string_view{entry->value};
    }
    return std::nullopt;
}

auto Config::has(std::string_view section, std::string_view key) const noexcept -> bool {
    return find(section, key) != nullptr;
}

auto Config::size() const noexcept -> std::size_t { return m_entries.size(); }

auto Config::expand(std::string_view /*section*/, std::string_view /*key*/, ConfigError &error) const
    -> std::optional<std::string> {
    // TODO(appkit::config): 按 README 待实现功能点展开 ${...} 引用。
    error = ConfigError{0, "Config::expand is not implemented"};
    return std::nullopt;
}

} // namespace appkit
