 #include "RubiksCube.hpp"

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
