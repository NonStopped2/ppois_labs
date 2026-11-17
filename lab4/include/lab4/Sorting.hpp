#pragma once

#include <algorithm>
#include <functional>
#include <iterator>
#include <list>
#include <utility>

namespace lab4 {
namespace detail {

template <class Iterator, class Compare>
bool moveLargestRight(Iterator left, Iterator right, Compare& compare) {
    bool swapped = false;
    for (auto current = left; current != right; ++current) {
        const auto next = std::next(current);
        if (std::invoke(compare, *next, *current)) {
            std::iter_swap(current, next);
            swapped = true;
        }
    }
    return swapped;
}

template <class Iterator, class Compare>
bool moveSmallestLeft(Iterator left, Iterator right, Compare& compare) {
    bool swapped = false;
    for (auto current = right; current != left; --current) {
        const auto previous = std::prev(current);
        if (std::invoke(compare, *current, *previous)) {
            std::iter_swap(previous, current);
            swapped = true;
        }
    }
    return swapped;
}

template <class Value, class Compare>
std::list<Value> takeStrand(std::list<Value>& remaining, Compare& compare) {
    std::list<Value> strand;
    strand.splice(strand.end(), remaining, remaining.begin());
    for (auto current = remaining.begin(); current != remaining.end();) {
        const auto candidate = current++;
        // Include equal keys so a later equivalent element cannot enter an earlier strand.
        if (!std::invoke(compare, *candidate, strand.back())) {
            strand.splice(strand.end(), remaining, candidate);
        }
    }
    return strand;
}

} // namespace detail

/// Stably sort a mutable bidirectional range by swapping adjacent elements.
template <std::bidirectional_iterator Iterator, class Compare = std::less<>>
    requires std::indirect_strict_weak_order<Compare, Iterator>
void cocktailSort(Iterator first, Iterator last, Compare compare = {}) {
    if (first == last) {
        return;
    }
    auto right = std::prev(last);
    while (first != right) {
        if (!detail::moveLargestRight(first, right, compare)) {
            return;
        }
        --right;
        if (!detail::moveSmallestLeft(first, right, compare)) {
            return;
        }
        ++first;
    }
}

/// Stably sort a mutable forward range by extracting and merging sorted strands.
template <std::forward_iterator Iterator, class Compare = std::less<>>
    requires std::indirect_strict_weak_order<Compare, Iterator>
void strandSort(Iterator first, Iterator last, Compare compare = {}) {
    using Value = std::iter_value_t<Iterator>;
    std::list<Value> remaining;
    for (auto current = first; current != last; ++current) {
        remaining.emplace_back(std::ranges::iter_move(current));
    }

    std::list<Value> sorted;
    while (!remaining.empty()) {
        auto strand = detail::takeStrand(remaining, compare);
        sorted.merge(strand, std::ref(compare));
    }
    for (auto& value : sorted) {
        *first = std::move(value);
        ++first;
    }
}

} // namespace lab4
