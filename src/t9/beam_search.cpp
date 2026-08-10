#include "t9/beam_search.h"

#include <algorithm>

namespace t9 {

BeamSearch::BeamSearch(size_t beam_width)
    : beam_width_(beam_width) {}

void BeamSearch::Add(const std::string& value, double score) {
    candidates_.push_back({value, score});

    std::sort(candidates_.begin(), candidates_.end(),
              [](const BeamCandidate& a, const BeamCandidate& b) {
                  return a.score > b.score;
              });

    if (candidates_.size() > beam_width_) {
        candidates_.resize(beam_width_);
    }
}

std::vector<std::string> BeamSearch::Results() const {
    std::vector<std::string> result;
    result.reserve(candidates_.size());

    for (const auto& item : candidates_) {
        result.push_back(item.value);
    }

    return result;
}

void BeamSearch::Clear() {
    candidates_.clear();
}

} // namespace t9
