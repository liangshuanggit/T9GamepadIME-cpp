#include "t9/t9_trie.h"

namespace t9 {

T9Trie::T9Trie() : root_(std::make_unique<Node>()) {}

void T9Trie::Insert(const std::string& key, const std::string& value) {
    Node* node = root_.get();

    for (char c : key) {
        auto& child = node->children[c];
        if (!child) {
            child = std::make_unique<Node>();
        }
        node = child.get();
    }

    node->terminal = true;
    node->values.push_back(value);
}

const T9Trie::Node* T9Trie::MatchPrefix(const Node* node, char key) const {
    if (!node) {
        return nullptr;
    }

    auto it = node->children.find(key);
    if (it == node->children.end()) {
        return nullptr;
    }

    return it->second.get();
}

} // namespace t9
