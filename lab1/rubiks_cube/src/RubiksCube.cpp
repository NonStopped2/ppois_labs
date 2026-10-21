#include "RubiksCube.hpp"
#include <stdexcept>
#include <random>
#include <istream>
#include <ostream>

RubiksCube::RubiksCube() {
       for (std::size_t faceIndex = 0; faceIndex < kFaceCount; ++faceIndex) {
           faces_[faceIndex].fill(kSolvedColors[faceIndex]);
       }
   }

bool RubiksCube::isSolved() const {
  for (const auto &face : faces_) {
    for (Color cellColor : face) {
      if (cellColor != face[0]) {
        return false;
      }
    }
  }

  return true;
}

Color RubiksCube::colorAt(Face face, std::size_t row,
                          std::size_t column) const {
  const std::size_t faceIndex = static_cast<std::size_t>(face);

  if (faceIndex >= kFaceCount || row >= kSideLength || column >= kSideLength) {
    throw std::out_of_range("Cube cell coordinates are out of range");
  }

  const std::size_t cellIndex = row * kSideLength + column;

  return faces_[faceIndex][cellIndex];
}

bool RubiksCube::operator==(const RubiksCube &other) const {
  return faces_ == other.faces_;
}

bool RubiksCube::operator!=(const RubiksCube &other) const {
  return !(*this == other);
}

void RubiksCube::rotateFaceClockwise(Face face) {
  const std::size_t faceIndex = static_cast<std::size_t>(face);
  const auto previousFace = faces_[faceIndex];

  for (std::size_t row = 0; row < kSideLength; ++row) {
    for (std::size_t column = 0; column < kSideLength; ++column) {
      const std::size_t oldIndex = row * kSideLength + column;
      const std::size_t newRow = column;
      const std::size_t newColumn = kSideLength - 1 - row;
      const std::size_t newIndex = newRow * kSideLength + newColumn;

      faces_[faceIndex][newIndex] = previousFace[oldIndex];
    }
  }
}

void RubiksCube::rotateFront() { rotate(Face::Front); }

RubiksCube::NeighborStrips RubiksCube::neighborStripsFor(Face face) const {
  static constexpr std::array<NeighborStrips, kFaceCount> kNeighborTable{
      {NeighborStrips{{{Face::Front, {0, 1, 2}},
                       {Face::Left, {0, 1, 2}},
                       {Face::Back, {0, 1, 2}},
                       {Face::Right, {0, 1, 2}}}},
       NeighborStrips{{{Face::Front, {6, 7, 8}},
                       {Face::Right, {6, 7, 8}},
                       {Face::Back, {6, 7, 8}},
                       {Face::Left, {6, 7, 8}}}},
       NeighborStrips{{{Face::Up, {6, 7, 8}},
                       {Face::Right, {0, 3, 6}},
                       {Face::Down, {2, 1, 0}},
                       {Face::Left, {8, 5, 2}}}},
       NeighborStrips{{{Face::Up, {0, 1, 2}},
                       {Face::Left, {6, 3, 0}},
                       {Face::Down, {8, 7, 6}},
                       {Face::Right, {2, 5, 8}}}},
       NeighborStrips{{{Face::Up, {0, 3, 6}},
                       {Face::Front, {0, 3, 6}},
                       {Face::Down, {0, 3, 6}},
                       {Face::Back, {8, 5, 2}}}},
       NeighborStrips{{{Face::Up, {2, 5, 8}},
                       {Face::Back, {6, 3, 0}},
                       {Face::Down, {2, 5, 8}},
                       {Face::Front, {2, 5, 8}}}}}};

  return kNeighborTable.at(static_cast<std::size_t>(face));
}

void RubiksCube::rotateNeighborStrips(const NeighborStrips &strips) {
  const auto previousFaces = faces_;

  for (std::size_t stripIndex = 0; stripIndex < kNeighborCount; ++stripIndex) {
    const NeighborStrip &sourceStrip = strips[stripIndex];
    const NeighborStrip &destinationStrip =
        strips[(stripIndex + 1) % kNeighborCount];

    const auto &sourceFace =
        previousFaces[static_cast<std::size_t>(sourceStrip.face)];
    auto &destinationFace =
        faces_[static_cast<std::size_t>(destinationStrip.face)];

    for (std::size_t offset = 0; offset < kSideLength; ++offset) {
      destinationFace[destinationStrip.indices[offset]] =
          sourceFace[sourceStrip.indices[offset]];
    }
  }
}
void RubiksCube::rotate(Face face) {
  const NeighborStrips strips = neighborStripsFor(face);

  rotateFaceClockwise(face);
  rotateNeighborStrips(strips);
}

void RubiksCube::scramble(std::size_t moveCount) {
     if (moveCount == 0) {
         return;
     }

     std::random_device seedSource;
     const auto seed = static_cast<std::uint32_t>(seedSource());

     scramble(moveCount, seed);
 }

 void RubiksCube::scramble(std::size_t moveCount, std::uint32_t seed) {
     std::mt19937 generator(seed);
     std::uniform_int_distribution<std::size_t> faceDistribution(
         0, kFaceCount - 1
     );

     for (std::size_t move = 0; move < moveCount; ++move) {
         const std::size_t faceIndex = faceDistribution(generator);
         const Face selectedFace = static_cast<Face>(faceIndex);

         rotate(selectedFace);
     }
 }


 bool RubiksCube::tryParseColor (char symbol, Color& color){

     for (std::size_t index = 0; index < kColorSymbols.size(); ++index){
         if (kColorSymbols[index] == symbol){
             color = static_cast<Color>(index);
             return true;
         }
     }
     return false;
 }

 bool RubiksCube::readState(std::istream& input) {
        for (auto& face : faces_) {
            for (Color& color : face) {
                char symbol{};

                if (!(input >> symbol) || !tryParseColor(symbol, color)) {
                    return false;
                }
            }
        }

        return true;
    }


    bool RubiksCube::hasValidColors() const {
          std::array<std::size_t, kFaceCount> colorCounts{};

          for (std::size_t faceIndex = 0; faceIndex < kFaceCount; ++faceIndex) {
              const auto& face = faces_[faceIndex];

              if (face[kCenterIndex] != kSolvedColors[faceIndex]) {
                  return false;
              }

              for (Color color : face) {
                  ++colorCounts[static_cast<std::size_t>(color)];
              }
          }

          for (std::size_t count : colorCounts) {
              if (count != kCellsPerFace) {
                  return false;
              }
          }

          return true;
      }

      std::istream& operator>>(std::istream& input, RubiksCube& cube) {
             RubiksCube candidate;

             if (!candidate.readState(input) || !candidate.hasValidColors()) {
                 input.setstate(std::ios::failbit);
                 return input;
             }

             cube = candidate;
             return input;
         }

         std::ostream& operator<<(std::ostream& output, const RubiksCube& cube) {
               for (const auto& face : cube.faces_) {
                   for (std::size_t cellIndex = 0;
                        cellIndex < RubiksCube::kCellsPerFace;
                        ++cellIndex) {
                       if (cellIndex > 0) {
                           output << ' ';
                       }

                       const std::size_t colorIndex =
                           static_cast<std::size_t>(face[cellIndex]);

                       output << RubiksCube::kColorSymbols[colorIndex];
                   }

                   output << '\n';
               }

               return output;
           }
