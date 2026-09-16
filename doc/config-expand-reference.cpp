// 面试官用：appkit::config 的 expand 参考实现。
// 不在任何 CMake 目标里，不参与编译（与 src/config.cpp 的留桩版是同一个函数，同时编译会符号重复）。
// 原实现见 git 历史：曾内联在 src/config.cpp 的 #ifndef FLAG 分支里。

#include "appkit/config.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace appkit {

inline auto resloveRef(const Config &config, const Config::Entry &entry, std::vector<const Config::Entry *> &active)
    -> std::optional<std::string> {
    auto expanded = std::string{};
    auto content = std::string_view{entry.value};
    while (!content.empty()) {
        const auto dollar = content.find('$');
        if (dollar == std::string_view::npos) {
            expanded.append(content);
            break;
        }
        expanded.append(content.substr(0, dollar));

        const auto rest = content.substr(dollar);
        const auto brace_end = rest.find('}');
        if (rest.size() < 2 || rest[1] != '{' || brace_end == std::string_view::npos) {
            return std::nullopt;
        }
        const auto name = rest.substr(2, brace_end - 2);
        content = rest.substr(brace_end + 1);

        auto section = std::string_view{entry.section};
        auto key = name;
        if (const auto dot = name.find('.'); dot != std::string_view::npos) {
            section = name.substr(0, dot);
            key = name.substr(dot + 1);
            if (section.empty() || key.empty()) {
                return std::nullopt;
            }
        }

        const auto *target = config.find(section, key);
        if (target == nullptr) {
            return std::nullopt;
        }
        if (std::ranges::find(active, target) != active.end()) {
            return std::nullopt;
        }
        active.push_back(target);
        const auto value = resloveRef(config, *target, active);
        active.pop_back();
        if (!value) {
            return std::nullopt;
        }
        expanded.append(*value);
    }
    return expanded;
}

auto Config::expand(std::string_view section, std::string_view key, ConfigError &) const -> std::optional<std::string> {
    auto entry = find(section, key);
    if (!entry) {
        return std::nullopt;
    }
    auto active = std::vector<const Config::Entry *>{};
    active.push_back(entry);
    return resloveRef(*this, *entry, active);
}
} // namespace appkit
