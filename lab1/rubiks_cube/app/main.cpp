#include "RubiksCube.hpp"

#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {
constexpr std::size_t kScrambleMoveCount = 30;
constexpr std::uint32_t kDemoSeed = 42;
const std::string kWrongCentersState =
    "W W W W Y W W W W\n"
    "Y Y Y Y W Y Y Y Y\n"
    "G G G G G G G G G\n"
    "B B B B B B B B B\n"
    "O O O O O O O O O\n"
    "R R R R R R R R R\n";

std::string serialize(const RubiksCube &cube) {
  std::ostringstream output;
  output << cube;
  return output.str();
}

void saveCube(const std::filesystem::path &path, const RubiksCube &cube) {
  if (std::filesystem::exists(path)) {
    throw std::runtime_error("Demo will not overwrite an existing file");
  }

  std::ofstream output(path);
  if (!output) {
    throw std::runtime_error("Cannot open cube file for writing");
  }

  output << cube;
  output.close();
  if (!output) {
    throw std::runtime_error("Cannot write cube state");
  }
}

RubiksCube loadCube(const std::filesystem::path &path) {
  std::ifstream input(path);
  RubiksCube cube;
  if (!(input >> cube)) {
    throw std::runtime_error("Cannot read a valid cube state from file");
  }

  input >> std::ws;
  if (input.bad() || !input.eof()) {
    throw std::runtime_error("Unexpected data after cube state");
  }

  return cube;
}

bool demonstrateFileRoundTrip(const RubiksCube &originalCube,
                              const std::filesystem::path &path) {
  saveCube(path, originalCube);
  const RubiksCube loadedCube = loadCube(path);
  const bool restored = loadedCube == originalCube;

  std::cout << "Saved cube file: " << path << '\n';
  std::cout << "File round-trip preserves state: " << restored << '\n';
  return restored;
}

bool demonstrateRejectedInput(const std::string &name, const std::string &text,
                              const RubiksCube &originalCube) {
  RubiksCube cube(originalCube);
  std::istringstream input(text);
  input >> cube;

  const bool rejected = input.fail();
  const bool preserved = cube == originalCube;
  std::cout << name << ": rejected=" << rejected
            << ", original preserved=" << preserved << '\n';

  return rejected && preserved;
}

bool demonstrateInvalidInputs(const RubiksCube &originalCube) {
  const std::string solvedState = serialize(RubiksCube{});
  std::string unknownSymbol = solvedState;
  unknownSymbol.front() = 'X';
  std::string wrongColorCounts = solvedState;
  wrongColorCounts.front() = 'Y';

  const bool unknownRejected =
      demonstrateRejectedInput("Unknown color", unknownSymbol, originalCube);
  const bool truncatedRejected = demonstrateRejectedInput(
      "Truncated state", solvedState.substr(0, solvedState.size() / 2), originalCube);
  const bool countsRejected =
      demonstrateRejectedInput("Wrong color counts", wrongColorCounts, originalCube);
  const bool centersRejected =
      demonstrateRejectedInput("Wrong centers", kWrongCentersState, originalCube);

  return unknownRejected && truncatedRejected && countsRejected && centersRejected;
}
} // namespace

int main(int argumentCount, char *arguments[]) {
  if (argumentCount != 2) {
    std::cerr << "Usage: " << arguments[0] << " <new-cube-file>\n";
    return 1;
  }

  try {
    RubiksCube cube;
    cube.scramble(kScrambleMoveCount, kDemoSeed);
    std::cout << std::boolalpha;

    const bool fileRoundTrip = demonstrateFileRoundTrip(cube, arguments[1]);
    const bool invalidInputsRejected = demonstrateInvalidInputs(cube);
    return fileRoundTrip && invalidInputsRejected ? 0 : 1;
  } catch (const std::exception &error) {
    std::cerr << "Error: " << error.what() << '\n';
    return 1;
  }
}
