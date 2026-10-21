#include "RubiksCube.hpp"

#include <array>
#include <iostream>

int main() {
  constexpr std::size_t kQuarterTurnsPerCycle = 4;
  constexpr std::array<Face, 6> kFaces{
      Face::Up, Face::Down, Face::Front,
      Face::Back, Face::Left, Face::Right};

  std::cout << std::boolalpha;

  for (Face face : kFaces) {
    RubiksCube cube;
    const RubiksCube originalCube(cube);

    cube.rotate(face);
    const bool changed = cube != originalCube;

    for (std::size_t turn = 1; turn < kQuarterTurnsPerCycle; ++turn) {
      cube.rotate(face);
    }

    const bool restored = cube == originalCube;

    std::cout << "Face " << static_cast<std::size_t>(face)
              << ": changed=" << changed
              << ", restored=" << restored << '\n';

    if (!changed || !restored) {
      return 1;
    }
  }

  return 0;
}
