#pragma once

#include <util/assert.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Columnar::Exec {

// Wrapper around std::boyer_moore_horspool_searcher.
// Stores needle alongside searcher because searcher holds raw pointers into it.
class BmhSearcher {
public:
    explicit BmhSearcher(std::string needle)
        : needle_(std::move(needle)),
          inner_(needle_.data(), needle_.data() + needle_.size()) {
    }

    BmhSearcher(const BmhSearcher& other)
        : needle_(other.needle_),
          inner_(needle_.data(), needle_.data() + needle_.size()) {
    }

    BmhSearcher(BmhSearcher&& other) noexcept
        : needle_(std::move(other.needle_)),
          inner_(needle_.data(), needle_.data() + needle_.size()) {
    }

    BmhSearcher& operator=(BmhSearcher&& other) noexcept {
        needle_ = std::move(other.needle_);
        inner_ = std::boyer_moore_horspool_searcher<const char*>(
            needle_.data(), needle_.data() + needle_.size());
        return *this;
    }

    std::string_view Needle() const {
        return needle_;
    }

    bool Contains(std::string_view hay) const {
        return std::search(hay.begin(), hay.end(), inner_) != hay.end();
    }

    // Returns position AFTER the found entry (for MULTI_CONTAINS chain).
    // Returns string::npos if not found.
    size_t FindFrom(std::string_view hay, size_t start) const {
        auto it = std::search(hay.begin() + start, hay.end(), inner_);
        if (it == hay.end())
            return std::string::npos;
        return static_cast<size_t>(it - hay.begin()) + needle_.size();
    }

private:
    std::string needle_;
    std::boyer_moore_horspool_searcher<const char*> inner_;
};

// NFA for patterns with '_' (exactly one any character).
// Simulates NFA via active states bitset.
//
// States: 0..m, where m = pattern length.
// Transitions:
//   '%' at pattern[i]: self-loop (stay at i on any char) + epsilon to i+1
//   '_' at pattern[i]: advance to i+1 on any char
//   'c' at pattern[i]: advance to i+1 only if input char == 'c'
// Accept state: m.
class NfaMatcher {
public:
    static constexpr size_t kPatternSizeThreshold = 64;

    explicit NfaMatcher(std::string pattern)
        : pattern_(std::move(pattern)) {
    }

    bool Match(std::string_view str) const {
        return pattern_.size() < kPatternSizeThreshold
                   ? MatchBitset(str)
                   : MatchVector(str);
    }

private:
    // Epsilon-closure: if state i is active and pattern[i]=='%',
    // state i+1 becomes active for free (% can match zero characters).
    // Loops until stable to handle consecutive '%%'.
    uint64_t BitsetClosure(uint64_t states) const {
        uint64_t prev = 0;
        while (prev != states) {
            prev = states;
            for (size_t i = 0; i < pattern_.size(); ++i)
                if (((states >> i) & 1) && pattern_[i] == '%')
                    states |= (1ULL << (i + 1));
        }
        return states;
    }

    // Bug fixed: loop bound was states.size() (= m+1), accessing pattern_[m] → OOB.
    void VectorClosure(std::vector<uint8_t>& states) const {
        for (size_t i = 0; i < pattern_.size(); ++i)
            if (states[i] && pattern_[i] == '%')
                states[i + 1] = 1;
    }

    bool MatchBitset(std::string_view str) const {
        const size_t m = pattern_.size();
        uint64_t activeStates = BitsetClosure(1ULL);

        for (char ch : str) {
            uint64_t next = 0;
            for (size_t i = 0; i < m; ++i) {
                if (!((activeStates >> i) & 1))
                    continue;
                const char p = pattern_[i];
                if (p == '%') {
                    next |= (1ULL << i);
                    next |= (1ULL << (i + 1));
                } else if (p == '_' || p == ch) {
                    next |= (1ULL << (i + 1));
                }
            }
            activeStates = BitsetClosure(next);
            if (!activeStates)
                return false;
        }
        // Bug fixed: was (activeStates >> str.size()) — accept state is at bit m, not str.size().
        return (activeStates >> m) & 1;
    }

    bool MatchVector(std::string_view str) const {
        const size_t m = pattern_.size();
        std::vector<uint8_t> activeStates(m + 1, 0);
        activeStates[0] = 1;
        VectorClosure(activeStates);

        for (char ch : str) {
            std::vector<uint8_t> next(m + 1, 0);
            for (size_t i = 0; i < m; ++i) {
                if (!activeStates[i])
                    continue;
                const char p = pattern_[i];
                if (p == '%') {
                    next[i] = 1;
                    next[i + 1] = 1;
                } else if (p == '_' || p == ch) {
                    next[i + 1] = 1;
                }
            }
            VectorClosure(next);
            activeStates = std::move(next);
            if (std::none_of(activeStates.begin(), activeStates.end(), [](uint8_t b) { return b; }))
                return false;
        }
        return activeStates[m];
    }

    std::string pattern_;
};

// Compiled SQL LIKE pattern. Built once in constructor, Match() has no allocations.
class CompiledPattern {
public:
    enum class Kind {
        AlwaysTrue,     // '%'
        Exact,          // 'foo'
        Prefix,         // 'foo%'
        Suffix,         // '%foo'
        Contains,       // '%foo%'
        PrefixSuffix,   // 'foo%bar'
        MultiContains,  // '%a%b%', 'a%b%c'
        General,        // contains '_' → NFA
    };

    explicit CompiledPattern(std::string sql) {
        if (sql.find('_') != std::string_view::npos) {
            kind_ = Kind::General;
            nfa_.emplace(std::string(sql));
            return;
        }

        hasLeadingPct_ = sql.starts_with('%');
        hasTrailingPct_ = sql.ends_with('%');

        std::vector<std::string> segs;
        size_t pos = 0;
        while (pos <= sql.size()) {
            const size_t pct = sql.find('%', pos);
            const size_t segEnd = (pct == std::string_view::npos) ? sql.size() : pct;
            if (segEnd > pos)
                segs.emplace_back(sql.substr(pos, segEnd - pos));
            if (pct == std::string_view::npos)
                break;
            pos = pct + 1;
        }

        if (segs.empty()) {
            kind_ = Kind::AlwaysTrue;
            return;
        }

        if (!hasLeadingPct_ && !hasTrailingPct_) {
            if (segs.size() == 1) {
                kind_ = Kind::Exact;
                prefix_ = segs[0];
                return;
            }
            if (segs.size() == 2) {
                kind_ = Kind::PrefixSuffix;
                prefix_ = segs[0];
                suffix_ = segs[1];
                return;
            }
        }
        if (!hasLeadingPct_ && hasTrailingPct_ && segs.size() == 1) {
            kind_ = Kind::Prefix;
            prefix_ = segs[0];
            return;
        }
        if (hasLeadingPct_ && !hasTrailingPct_ && segs.size() == 1) {
            kind_ = Kind::Suffix;
            suffix_ = segs[0];
            return;
        }
        if (hasLeadingPct_ && hasTrailingPct_ && segs.size() == 1) {
            kind_ = Kind::Contains;
            searchers_.emplace_back(segs[0]);
            return;
        }

        kind_ = Kind::MultiContains;
        for (auto& s : segs)
            searchers_.emplace_back(std::move(s));
    }

    bool Match(std::string_view str) const {
        switch (kind_) {
            case Kind::AlwaysTrue:
                return true;
            case Kind::Exact:
                return str == prefix_;
            case Kind::Prefix:
                return str.starts_with(prefix_);
            case Kind::Suffix:
                return str.ends_with(suffix_);
            case Kind::Contains:
                return searchers_[0].Contains(str);
            case Kind::PrefixSuffix:
                return str.size() >= prefix_.size() + suffix_.size() && str.starts_with(prefix_) && str.ends_with(suffix_);
            case Kind::MultiContains:
                return MatchMulti(str);
            case Kind::General:
                return nfa_->Match(str);
        }
        std::unreachable();
    }

private:
    bool MatchMulti(std::string_view str) const {
        size_t pos = 0;
        size_t end = str.size();
        size_t first = 0;
        size_t last = searchers_.size();

        if (!hasLeadingPct_) {
            if (!str.starts_with(searchers_[0].Needle()))
                return false;
            pos = searchers_[0].Needle().size();
            first = 1;
        }

        if (!hasTrailingPct_) {
            const std::string_view backNeedle = searchers_.back().Needle();
            if (!str.ends_with(backNeedle))
                return false;
            end = str.size() - backNeedle.size();
            last = searchers_.size() - 1;
        }

        if (first >= last)
            return pos <= end;

        for (size_t i = first; i < last; ++i) {
            const size_t found = searchers_[i].FindFrom(str, pos);
            if (found == std::string::npos || found > end)
                return false;
            pos = found;
        }
        return pos <= end;
    }

    Kind kind_ = Kind::AlwaysTrue;
    bool hasLeadingPct_ = false;
    bool hasTrailingPct_ = false;

    std::string prefix_;
    std::string suffix_;

    std::vector<BmhSearcher> searchers_;
    std::optional<NfaMatcher> nfa_;
};

}  // namespace Columnar::Exec
