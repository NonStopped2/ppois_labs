#pragma once

#include "Program.hpp"
#include <catch2/catch_test_macros.hpp>
#include <sstream>
#include <string>

namespace test_support {
inline Program program(const std::string& text) {
    std::istringstream input(text);
    Program result;
    REQUIRE(static_cast<bool>(input >> result));
    return result;
}

template <class Value> void roundTrip(const Value& value) {
    std::stringstream stream;
    REQUIRE(&(stream << value) == &stream);
    Value restored;
    REQUIRE(&(stream >> restored) == &stream);
    REQUIRE_FALSE(stream.fail());
    REQUIRE(restored == value);
    REQUIRE_FALSE(restored != value);
}

template <class Value> void invalidInputPreserves(const std::string& text, const Value& original) {
    INFO(text);
    Value result = original;
    std::istringstream input(text);
    input >> result;
    REQUIRE(input.fail());
    REQUIRE(result == original);
}
} // namespace test_support
