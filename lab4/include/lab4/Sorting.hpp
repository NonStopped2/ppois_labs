#pragma once

#include <cstddef>
#include <functional>
#include <iterator>
#include <list>
#include <utility>
#include <vector>

namespace lab4 {
namespace detail {

template <class Iterator, class Compare>
bool moveLargestRight(Iterator left, Iterator right, Compare& compare) {
    bool swapped = false;
    for (auto current = left; current != right; ++current) {
        const auto next = std::ranges::next(current);
        if (std::invoke(compare, *next, *current)) {
            std::ranges::iter_swap(current, next);
            swapped = true;
        }
    }
    return swapped;
}

template <class Iterator, class Compare>
bool moveSmallestLeft(Iterator left, Iterator right, Compare& compare) {
    bool swapped = false;
    for (auto current = right; current != left; --current) {
        const auto previous = std::ranges::prev(current);
        if (std::invoke(compare, *current, *previous)) {
            std::ranges::iter_swap(previous, current);
            swapped = true;
        }
    }
    return swapped;
}

template <class Compare>
std::list<std::size_t> takeStrand(std::list<std::size_t>& remaining, Compare& compare) {
    std::list<std::size_t> strand;
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

template <class Iterator>
void applyOrder(const std::vector<Iterator>& positions, const std::list<std::size_t>& order) {
    std::vector<std::size_t> destinations(positions.size());
    std::size_t target = 0;
    for (const auto source : order) {
        destinations[source] = target++;
    }

    // Swapping an item also moves its destination. Every swap places one item for good.
    for (std::size_t source = 0; source < positions.size(); ++source) {
        while (destinations[source] != source) {
            const auto destination = destinations[source];
            std::ranges::iter_swap(positions[source], positions[destination]);
            std::swap(destinations[source], destinations[destination]);
        }
    }
}

} // namespace detail

/// Stably sort a mutable bidirectional range by swapping adjacent elements.
template <std::bidirectional_iterator Iterator, class Compare = std::less<>>
    requires std::indirect_strict_weak_order<Compare, Iterator> &&
             std::indirectly_swappable<Iterator>
void cocktailSort(Iterator first, Iterator last, Compare compare = {}) {
    if (first == last) {
        return;
    }
    auto right = std::ranges::prev(last);
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
    requires std::indirect_strict_weak_order<Compare, Iterator> &&
             std::indirectly_swappable<Iterator>
void strandSort(Iterator first, Iterator last, Compare compare = {}) {
    std::vector<Iterator> positions;
    std::list<std::size_t> remaining;
    for (auto current = first; current != last; ++current) {
        remaining.push_back(positions.size());
        positions.push_back(current);
    }

    // Order positions first so allocation and comparison failures leave the input intact.
    const auto byValue = [&](std::size_t left, std::size_t right) {
        return std::invoke(compare, *positions[left], *positions[right]);
    };
    std::list<std::size_t> sorted;
    while (!remaining.empty()) {
        auto strand = detail::takeStrand(remaining, byValue);
        sorted.merge(strand, byValue);
    }
    detail::applyOrder(positions, sorted);
}

} // namespace lab4
