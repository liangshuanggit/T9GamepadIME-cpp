#pragma once

#include <string>
#include <vector>

namespace t9 {

struct BeamCandidate {
    std::string value;
    double score = 0.0;
};

// 保留 Top-K 路径的轻量 Beam Search。
// 用于替代 T9 展开阶段的全量组合生成。
class BeamSearch {
public:
    explicit BeamSearch(size_t beam_width = 16);

    void Add(const std::string& value, double score);

    std::vector<std::string> Results() const;

    void Clear();

private:
    size_t beam_width_;
    std::vector<BeamCandidate> candidates_;
};

} // namespace t9
