// C3 线性化器 - 计算 MRO（Method Resolution Order）
// 对齐 spec 4.1 / design 2.1.4.1
// 算法：L[C] = C + merge(L[B1], L[B2], ..., L[Bn], B1 B2 ... Bn)
// merge：取第一个列表的头部，若该头部不在其他任何列表的尾部（即不在非头部位置），
//        则加入结果并从所有列表中移除；否则取下一个列表的头部。
//        若所有头部都不合法，则抛出 SEM006 MRO 冲突。
#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <stdexcept>
#include <sstream>

namespace next11 {

// C3 MRO 计算异常：线性化失败（继承层次冲突）
class C3LinearizationError : public std::runtime_error {
public:
    std::string conflict_class;
    explicit C3LinearizationError(const std::string& cls, const std::string& msg)
        : std::runtime_error(msg), conflict_class(cls) {}
};

class C3Linearizer {
public:
    // 计算类 cls 的 MRO
    // cls：待线性化的类名
    // bases_mros：每个基类的 MRO 列表（顺序与 bases 一致）
    // bases：基类名列表（用于追加到 merge 输入末尾）
    // 返回：cls 的 MRO 列表（首元素为 cls 自身）
    static std::vector<std::string> linearize(
        const std::string& cls,
        const std::vector<std::vector<std::string>>& bases_mros,
        const std::vector<std::string>& bases) {

        // 结果以 cls 自身开头
        std::vector<std::string> result;
        result.push_back(cls);

        // 构造 merge 输入：所有基类的 MRO + 基类列表本身
        std::vector<std::vector<std::string>> lists;
        for (const auto& mro : bases_mros) {
            if (!mro.empty()) lists.push_back(mro);
        }
        if (!bases.empty()) lists.push_back(bases);

        // 执行 merge
        std::vector<std::string> merged = merge(lists, cls);
        for (auto& name : merged) result.push_back(std::move(name));

        return result;
    }

    // C3 merge 算法
    // lists：待合并的列表集合
    // conflict_cls：用于错误报告的类名
    static std::vector<std::string> merge(
        std::vector<std::vector<std::string>> lists,
        const std::string& conflict_cls) {

        std::vector<std::string> result;

        // 移除空列表的辅助
        auto remove_empty = [](std::vector<std::vector<std::string>>& ls) {
            ls.erase(std::remove_if(ls.begin(), ls.end(),
                [](const std::vector<std::string>& l) { return l.empty(); }), ls.end());
        };

        remove_empty(lists);

        while (!lists.empty()) {
            bool found = false;
            // 尝试每个列表的头部
            for (size_t i = 0; i < lists.size(); ++i) {
                if (lists[i].empty()) continue;
                const std::string head = lists[i].front();
                // 检查 head 是否在任何其他列表的尾部（非头部位置）
                if (!in_tail(head, lists)) {
                    // 合法的头部，加入结果
                    result.push_back(head);
                    // 从所有列表中移除 head
                    for (auto& l : lists) {
                        if (!l.empty() && l.front() == head) l.erase(l.begin());
                    }
                    remove_empty(lists);
                    found = true;
                    break;
                }
            }
            if (!found) {
                // MRO 冲突：构造冲突信息
                std::ostringstream oss;
                oss << "无法为类 '" << conflict_cls << "' 构造一致的 MRO（C3 线性化失败）";
                throw C3LinearizationError(conflict_cls, oss.str());
            }
        }
        return result;
    }

    // 检查 cls 是否在任何列表的尾部（非头部位置）
    // 若 cls 出现在某列表的第 2 个或更后位置，则返回 true
    static bool in_tail(const std::string& cls, const std::vector<std::vector<std::string>>& lists) {
        for (const auto& l : lists) {
            if (l.size() < 2) continue;
            for (size_t i = 1; i < l.size(); ++i) {
                if (l[i] == cls) return true;
            }
        }
        return false;
    }

    // 便捷接口：基于类名->基类列表的映射，递归计算 MRO
    // class_hierarchy：类名 -> 直接基类名列表
    // cls：待计算的类名
    // 返回：cls 的 MRO
    static std::vector<std::string> compute_mro(
        const std::string& cls,
        const std::unordered_map<std::string, std::vector<std::string>>& class_hierarchy) {

        std::unordered_map<std::string, std::vector<std::string>> cache;
        std::vector<std::string> stack;
        std::unordered_set<std::string> in_stack;

        stack.push_back(cls);
        in_stack.insert(cls);

        while (!stack.empty()) {
            const std::string cur = stack.back();

            if (cache.count(cur) > 0) {
                stack.pop_back();
                in_stack.erase(cur);
                continue;
            }

            auto it = class_hierarchy.find(cur);
            if (it == class_hierarchy.end() || it->second.empty()) {
                cache[cur] = {cur};
                stack.pop_back();
                in_stack.erase(cur);
                continue;
            }

            const auto& bases = it->second;
            bool all_bases_cached = true;
            for (const auto& base : bases) {
                if (cache.count(base) == 0) {
                    if (in_stack.count(base) > 0) {
                        throw C3LinearizationError(base,
                            "检测到循环继承，类 '" + base + "'");
                    }
                    stack.push_back(base);
                    in_stack.insert(base);
                    all_bases_cached = false;
                }
            }

            if (!all_bases_cached) continue;

            std::vector<std::vector<std::string>> bases_mros;
            for (const auto& base : bases) {
                bases_mros.push_back(cache[base]);
            }
            cache[cur] = linearize(cur, bases_mros, bases);
            stack.pop_back();
            in_stack.erase(cur);
        }

        return cache[cls];
    }

private:
};

} // namespace next11