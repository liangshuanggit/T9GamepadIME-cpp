#include "t9/user_dictionary.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>

namespace t9 {

namespace {
constexpr size_t kMaxCount = 1000000;
}

UserDictionary::UserDictionary(std::string path) : path_(std::move(path)) {}

bool UserDictionary::Load() {
    counts_.clear();
    if (path_.empty()) return true;

    std::ifstream in(path_, std::ios::binary);
    if (!in) return true;

    std::string line;
    while (std::getline(in, line)) {
        const size_t tab = line.find('\t');
        if (tab == std::string::npos || tab == 0) continue;
        const std::string text = line.substr(0, tab);
        try {
            const size_t count = std::stoull(line.substr(tab + 1));
            if (count > 0) counts_[text] = std::min(count, kMaxCount);
        } catch (...) {
            // Malformed user data is ignored so IME startup remains robust.
        }
    }
    return true;
}

bool UserDictionary::Save() const {
    if (path_.empty()) return true;

    const std::filesystem::path target(path_);
    if (target.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(target.parent_path(), ec);
    }

    const std::filesystem::path temp = target.string() + ".tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        for (const auto& [text, count] : counts_) {
            if (text.find('\n') != std::string::npos || text.find('\r') != std::string::npos ||
                text.find('\t') != std::string::npos) continue;
            out << text << '\t' << count << '\n';
        }
        if (!out) return false;
    }

    std::error_code ec;
    std::filesystem::remove(target, ec);
    std::filesystem::rename(temp, target, ec);
    return !ec;
}

void UserDictionary::RecordSelection(const std::string& text) {
    if (text.empty()) return;
    auto& count = counts_[text];
    if (count < kMaxCount) ++count;
    // Persist immediately so a process crash does not lose recent learning.
    Save();
}

size_t UserDictionary::GetScore(const std::string& text) const {
    const auto it = counts_.find(text);
    return it == counts_.end() ? 0 : it->second;
}

void UserDictionary::Clear() {
    counts_.clear();
    Save();
}

}  // namespace t9
