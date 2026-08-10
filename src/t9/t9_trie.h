#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace t9 {

// T9 拼音搜索 Trie。
// 第一阶段先作为独立组件引入，不改变 T9Engine 对外接口。
// 后续用于替换音节 DFS 搜索。
class T9Trie {
public:
    struct Node {
        bool terminal = false;
        std::vector<std::string> values;
        std::unordered_map<char, std::unique_ptr<Node>> children;
    };

    T9Trie();

    void Insert(const std::string& key, const std::string& value);

    const Node* Root() const { return root_.get(); }

    const Node* MatchPrefix(const Node* node, char key) const;

private:
    std::unique_ptr<Node> root_;
};

} // namespace t9
