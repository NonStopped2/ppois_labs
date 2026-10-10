 #include "RubiksCube.hpp"
 #include <stdexcept>


   RubiksCube::RubiksCube() {
       const std::array<Color, kFaceCount> solvedColors{
           Color::White,
           Color::Yellow,
           Color::Green,
           Color::Blue,
           Color::Orange,
           Color::Red
       };

       for (std::size_t faceIndex = 0; faceIndex < kFaceCount; ++faceIndex) {
           faces_[faceIndex].fill(solvedColors[faceIndex]);
       }
   }

   bool RubiksCube::isSolved() const {
       for (const auto& face : faces_) {
           for (Color cellColor : face) {
               if (cellColor != face[0]) {
                   return false;
               }
           }
       }

       return true;
   }

   Color RubiksCube::colorAt(
         Face face,
         std::size_t row,
         std::size_t column
     ) const {
         const std::size_t faceIndex = static_cast<std::size_t>(face);

         if (faceIndex >= kFaceCount ||
             row >= kSideLength ||
             column >= kSideLength) {
             throw std::out_of_range("Cube cell coordinates are out of range");
         }

         const std::size_t cellIndex = row * kSideLength + column;

         return faces_[faceIndex][cellIndex];
     }

     bool RubiksCube::operator==(const RubiksCube& other) const {
          return faces_ == other.faces_;
      }

      bool RubiksCube::operator!=(const RubiksCube& other) const {
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

         void RubiksCube::rotateFront() {
               rotateFaceClockwise(Face::Front);

               auto& upperFace = faces_[static_cast<std::size_t>(Face::Up)];
               auto& rightFace = faces_[static_cast<std::size_t>(Face::Right)];
               auto& lowerFace = faces_[static_cast<std::size_t>(Face::Down)];
               auto& leftFace = faces_[static_cast<std::size_t>(Face::Left)];

               const std::size_t lastCoordinate = kSideLength - 1;

               for (std::size_t offset = 0; offset < kSideLength; ++offset) {
                   const std::size_t upperIndex = lastCoordinate * kSideLength + offset;
                   const std::size_t rightIndex = offset * kSideLength;
                   const std::size_t lowerIndex = lastCoordinate - offset;
                   const std::size_t leftIndex =
                       (lastCoordinate - offset) * kSideLength + lastCoordinate;

                   const Color previousUpperColor = upperFace[upperIndex];

                   upperFace[upperIndex] = leftFace[leftIndex];
                   leftFace[leftIndex] = lowerFace[lowerIndex];
                   lowerFace[lowerIndex] = rightFace[rightIndex];
                   rightFace[rightIndex] = previousUpperColor;
               }
           }
