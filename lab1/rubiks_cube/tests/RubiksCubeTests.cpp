#include "RubiksCube.hpp"

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {
struct Vector {
    int x;
    int y;
    int z;
    bool operator==(const Vector&) const = default;
};
constexpr std::array<Vector, 6> kNormals{
    {{0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}, {-1, 0, 0}, {1, 0, 0}}};
constexpr std::array<Vector, 6> kRight{
    {{1, 0, 0}, {1, 0, 0}, {1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}}};
constexpr std::array<Vector, 6> kDown{
    {{0, 0, 1}, {0, 0, -1}, {0, -1, 0}, {0, -1, 0}, {0, -1, 0}, {0, -1, 0}}};

int dot(Vector a, Vector b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
Vector cross(Vector a, Vector b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
Vector positionOf(std::size_t face, int row, int column) {
    const auto normal = kNormals[face];
    const auto right = kRight[face];
    const auto down = kDown[face];
    return {normal.x + right.x * (column - 1) + down.x * (row - 1),
            normal.y + right.y * (column - 1) + down.y * (row - 1),
            normal.z + right.z * (column - 1) + down.z * (row - 1)};
}
// -90 degrees around the outward normal: clockwise when looking at the face.
Vector clockwise(Vector value, Vector axis) {
    const auto product = cross(axis, value);
    const int projection = dot(axis, value);
    return {-product.x + axis.x * projection, -product.y + axis.y * projection,
            -product.z + axis.z * projection};
}
std::size_t targetIndex(Vector normal, Vector position) {
    for (std::size_t face = 0; face < kNormals.size(); ++face) {
        if (normal == kNormals[face]) {
            const int row = 1 + dot(position, kDown[face]);
            const int column = 1 + dot(position, kRight[face]);
            return face * 9 + static_cast<std::size_t>(row * 3 + column);
        }
    }
    throw std::logic_error("Unknown reference normal");
}
std::array<Color, 54> geometricRotation(const RubiksCube& cube, Face selected) {
    std::array<Color, 54> result{};
    const auto axis = kNormals[static_cast<std::size_t>(selected)];
    for (std::size_t face = 0; face < kNormals.size(); ++face) {
        for (int cell = 0; cell < 9; ++cell) {
            auto normal = kNormals[face];
            auto position = positionOf(face, cell / 3, cell % 3);
            if (dot(position, axis) == 1) {
                normal = clockwise(normal, axis);
                position = clockwise(position, axis);
            }
            result[targetIndex(normal, position)] =
                cube.colorAt(static_cast<Face>(face), cell / 3, cell % 3);
        }
    }
    return result;
}
std::string compactState(const RubiksCube& cube) {
    std::ostringstream output;
    output << cube;
    std::istringstream input(output.str());
    std::string result;
    char symbol{};
    while (input >> symbol)
        result += symbol;
    return result;
}
void invalidStatePreserves(const std::string& text, const RubiksCube& original) {
    INFO(text);
    RubiksCube result = original;
    std::istringstream input(text);
    input >> result;
    REQUIRE(input.fail());
    REQUIRE(result == original);
}
} // namespace

TEST_CASE("Cube starts solved with all expected colors", "[cube]") {
    const RubiksCube cube;
    REQUIRE(cube.isSolved());
    for (std::size_t face = 0; face < 6; ++face) {
        for (std::size_t cell = 0; cell < 9; ++cell) {
            REQUIRE(cube.colorAt(static_cast<Face>(face), cell / 3, cell % 3) ==
                    static_cast<Color>(face));
        }
    }
    REQUIRE_THROWS_AS(cube.colorAt(static_cast<Face>(6), 0, 0), std::out_of_range);
    REQUIRE_THROWS_AS(cube.colorAt(Face::Up, 3, 0), std::out_of_range);
    REQUIRE_THROWS_AS(cube.colorAt(Face::Up, 0, 3), std::out_of_range);
}

TEST_CASE("Cube rotations agree with independent three dimensional geometry", "[cube][geometry]") {
    for (std::uint32_t seed = 0; seed < 8; ++seed) {
        RubiksCube original;
        original.scramble(50 + seed, seed);
        for (std::size_t face = 0; face < 6; ++face) {
            const Face selected = static_cast<Face>(face);
            const auto expected = geometricRotation(original, selected);
            RubiksCube actual = original;
            actual.rotate(selected);
            for (std::size_t index = 0; index < expected.size(); ++index) {
                CAPTURE(seed, face, index);
                REQUIRE(actual.colorAt(static_cast<Face>(index / 9), (index % 9) / 3, index % 3) ==
                        expected[index]);
            }
        }
    }
}

TEST_CASE("Cube turns are reversible and copies independent", "[cube]") {
    RubiksCube original;
    original.scramble(30, 42);
    for (std::size_t face = 0; face < 6; ++face) {
        RubiksCube copy(original);
        for (int turn = 0; turn < 4; ++turn)
            copy.rotate(static_cast<Face>(face));
        REQUIRE(copy == original);
    }
    RubiksCube front;
    RubiksCube generic;
    front.rotateFront();
    generic.rotate(Face::Front);
    REQUIRE(front == generic);
    REQUIRE_FALSE(front.isSolved());
    REQUIRE(front != RubiksCube{});
    RubiksCube assigned;
    assigned = original;
    REQUIRE(assigned == original);
    assigned.rotate(Face::Right);
    REQUIRE(assigned != original);
    const RubiksCube before = assigned;
    REQUIRE_THROWS_AS(assigned.rotate(static_cast<Face>(-1)), std::out_of_range);
    REQUIRE(assigned == before);
}

TEST_CASE("Cube scrambling is reproducible and zero preserves current state", "[cube]") {
    RubiksCube first;
    first.scramble(100, 123);
    RubiksCube second;
    second.scramble(100, 123);
    REQUIRE(first == second);
    const RubiksCube before = first;
    first.scramble(0);
    first.scramble(0, 999);
    REQUIRE(first == before);
    RubiksCube random;
    random.scramble(1);
    REQUIRE_FALSE(random.isSolved());
}

TEST_CASE("Cube stream round trips and chaining preserve states", "[cube][streams]") {
    RubiksCube original;
    original.scramble(30, 7);
    std::stringstream stream;
    REQUIRE(&(stream << original << RubiksCube{}) == &stream);
    RubiksCube restored;
    RubiksCube solved;
    REQUIRE(&(stream >> restored >> solved) == &stream);
    REQUIRE_FALSE(stream.fail());
    REQUIRE(restored == original);
    REQUIRE(solved.isSolved());
    REQUIRE(compactState(solved) == std::string(9, 'W') + std::string(9, 'Y') +
                                        std::string(9, 'G') + std::string(9, 'B') +
                                        std::string(9, 'O') + std::string(9, 'R'));
    std::ostringstream broken;
    broken.setstate(std::ios::badbit);
    broken << original;
    REQUIRE(broken.fail());
}

TEST_CASE("Cube rejects unknown colors truncation counts and centers", "[cube][streams]") {
    RubiksCube original;
    original.scramble(12, 8);
    const std::string solved = compactState(RubiksCube{});
    invalidStatePreserves("", original);
    invalidStatePreserves(solved.substr(0, 53), original);
    std::string unknown = solved;
    unknown[0] = 'X';
    invalidStatePreserves(unknown, original);
    std::string counts = solved;
    counts[0] = 'Y';
    invalidStatePreserves(counts, original);
    std::string centers = solved;
    std::swap(centers[4], centers[13]);
    invalidStatePreserves(centers, original);
    std::istringstream throwing(unknown);
    throwing.exceptions(std::ios::failbit);
    RubiksCube result = original;
    REQUIRE_THROWS_AS(throwing >> result, std::ios_base::failure);
    REQUIRE(result == original);
}
