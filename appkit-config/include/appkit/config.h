#ifndef APPKIT_CONFIG_H
#define APPKIT_CONFIG_H

#include <concepts>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace appkit {

/// 解析/展开诊断：出错行号（1 基，0 表示与行无关）与原因。
struct ConfigError {
    std::size_t line = 0;
    std::string message;
};

/// 可作为节名或键名传入的文本类型。
template <class T>
concept TextArg = std::convertible_to<T, std::string_view>;

/// 只读的 INI 子集配置：由 svcd 的插件启动器在加载期解析，再注入各插件。
/// 解析完成后即为不可变快照，所有查询与展开都是 const、不改动内部状态。
class Config final {
public:
    /// 一条已解析的配置项；展开时按它取"值 + 所在节 + 行号"。
    struct Entry {
        std::string section;   ///< 所在节；全局节为空串
        std::string key;       ///< 键名
        std::string value;     ///< 原始值（已裁剪空白，引用未展开）
        std::size_t line = 0;  ///< 键所在行号（1 基）
    };

    /// 解析文本；失败时写 error 并返回 nullopt。
    [[nodiscard]] static auto parse(std::string_view text, ConfigError &error)
        -> std::optional<Config>;

    /// 取原始字符串值；不存在返回 nullopt。section 为空串表示全局节。
    [[nodiscard]] auto get(std::string_view section, std::string_view key) const noexcept
        -> std::optional<std::string_view>;

    /// 指定 (section, key) 是否存在。
    [[nodiscard]] auto has(std::string_view section,
                           std::string_view key) const noexcept -> bool;

    /// 已解析的唯一键数量。
    [[nodiscard]] auto size() const noexcept -> std::size_t;

    /// 展开值中的 ${...} 引用；失败时写 error 并返回 nullopt。
    [[nodiscard]] auto expand(std::string_view section, std::string_view key,
                              ConfigError &error) const -> std::optional<std::string>;

    /// 按 (section, key) 查找条目；未命中返回 nullptr。
    /// 返回的指针指向内部存储，在 Config 存活期内有效。
    template <TextArg S, TextArg K>
    [[nodiscard]] auto find(S section, K key) const noexcept -> const Entry * {
        const auto wanted_section = std::string_view{section};
        const auto wanted_key = std::string_view{key};
        for (const auto &entry : m_entries) {
            if (entry.section == wanted_section && entry.key == wanted_key) {
                return &entry;
            }
        }
        return nullptr;
    }

private:
    using EntryList = std::vector<Entry>;
    EntryList m_entries;
};

}  // namespace appkit

#endif  // APPKIT_CONFIG_H
