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
