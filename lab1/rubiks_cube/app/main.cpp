#include "RubiksCube.hpp"

#include <cstdint>
#include <exception>
#include <iostream>

namespace {
constexpr std::size_t kScrambleMoveCount = 30;
constexpr std::uint32_t kDemoSeed = 42;

void demonstrateRandomScramble() {
  RubiksCube cube;
  cube.scramble(kScrambleMoveCount);

  const bool solved = cube.isSolved();
  std::cout << "Random scramble completed: " << kScrambleMoveCount << " moves\n";
  std::cout << "Random cube is solved: " << solved << '\n';
}

bool demonstrateRepeatableScramble() {
  RubiksCube firstCube;
  RubiksCube secondCube;

  firstCube.scramble(kScrambleMoveCount, kDemoSeed);
  secondCube.scramble(kScrambleMoveCount, kDemoSeed);

  const bool repeatable = firstCube == secondCube;
  std::cout << "Same seed produces equal states: " << repeatable << '\n';

  return repeatable;
}

bool demonstrateZeroMoveScramble() {
  RubiksCube cube;
  cube.scramble(kScrambleMoveCount, kDemoSeed);
  const RubiksCube originalCube(cube);

  cube.scramble(0);
  const bool randomVersionPreserved = cube == originalCube;

  cube.scramble(0, kDemoSeed);
  const bool seededVersionPreserved = cube == originalCube;

  std::cout << "Zero random moves preserve state: " << randomVersionPreserved << '\n';
  std::cout << "Zero seeded moves preserve state: " << seededVersionPreserved << '\n';

  return randomVersionPreserved && seededVersionPreserved;
}
} // namespace

int main() {
  try {
    std::cout << std::boolalpha;
    demonstrateRandomScramble();

    const bool repeatable = demonstrateRepeatableScramble();
    const bool zeroMovesPreserveState = demonstrateZeroMoveScramble();

    return repeatable && zeroMovesPreserveState ? 0 : 1;
  } catch (const std::exception &error) {
    std::cerr << "Error: " << error.what() << '\n';
    return 1;
  }
}
