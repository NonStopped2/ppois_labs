#include "lab4/Sorting.hpp"
#include "lab4/Task.hpp"

#include <algorithm>
#include <functional>
#include <iterator>
#include <memory>
#include <vector>

#include <catch2/catch_test_macros.hpp>

namespace {

// Use the same contract tests for both public sorting algorithms.
const auto cocktail = [](auto first, auto last, auto compare) {
    lab4::cocktailSort(first, last, compare);
};
const auto strand = [](auto first, auto last, auto compare) {
    lab4::strandSort(first, last, compare);
};

void checkNumberSorting(auto sort) {
    int numbers[] = {4, -2, 4, 1, 0, -2};
    sort(std::begin(numbers), std::end(numbers), std::less<>{});
    CHECK(std::vector<int>(std::begin(numbers), std::end(numbers)) ==
          std::vector<int>{-2, -2, 0, 1, 4, 4});

    const std::vector<std::vector<int>> inputs = {
        {}, {7}, {2, 1}, {1, 2}, {1, 2, 3, 4}, {4, 3, 2, 1}, {2, 2, 2}, {3, 0, -1, 3, 4, 2}};
    for (auto values : inputs) {
        auto expected = values;
        std::stable_sort(expected.begin(), expected.end(), std::greater<>{});
        sort(values.begin(), values.end(), std::greater<>{});
        CHECK(values == expected);
    }
}

void checkTaskSorting(auto sort) {
    lab4::Task tasks[] = {{"first", 2}, {"second", 1}, {"third", 2}, {"fourth", 1}};
    const auto byPriority = [](const lab4::Task& left, const lab4::Task& right) {
        return left.priority() < right.priority();
    };
    auto values = std::vector<lab4::Task>(std::begin(tasks), std::end(tasks));
    auto expected = values;
    std::stable_sort(expected.begin(), expected.end(), byPriority);
    sort(std::begin(tasks), std::end(tasks), byPriority);
    sort(values.begin(), values.end(), byPriority);
    CHECK(std::equal(std::begin(tasks), std::end(tasks), expected.begin(), expected.end()));
    CHECK(values == expected);
    CHECK(values.front().name() == "second");
}

void checkMoveOnlySorting(auto sort) {
    std::vector<std::unique_ptr<int>> values;
    values.push_back(std::make_unique<int>(9));
    values.push_back(std::make_unique<int>(3));
    auto* original = values.back().get();
    sort(values.begin(), values.end(),
         [](const auto& left, const auto& right) { return *left < *right; });
    CHECK(values.front().get() == original);
    CHECK(*values.back() == 9);
}

} // namespace

TEST_CASE("Cocktail sort works on arrays and vectors and preserves equal-key order", "[core]") {
    checkNumberSorting(cocktail);
    checkTaskSorting(cocktail);
    checkMoveOnlySorting(cocktail);
}

TEST_CASE("Strand sort works on arrays and vectors and preserves equal-key order", "[core]") {
    checkNumberSorting(strand);
    checkTaskSorting(strand);
    checkMoveOnlySorting(strand);
}

TEST_CASE("Default comparison sorts only the requested subrange and supports bit vectors",
          "[core]") {
    int numbers[] = {10, 4, -1, 9};
    lab4::cocktailSort(std::begin(numbers) + 1, std::end(numbers) - 1);
    CHECK(std::vector<int>(std::begin(numbers), std::end(numbers)) ==
          std::vector<int>{10, -1, 4, 9});

    std::vector<bool> flags = {true, false, true, false};
    const std::vector<bool> expected = {false, false, true, true};
    lab4::strandSort(flags.begin(), flags.end());
    CHECK(flags == expected);
    flags = {true, true, false, false};
    lab4::cocktailSort(flags.begin(), flags.end());
    CHECK(flags == expected);
}
