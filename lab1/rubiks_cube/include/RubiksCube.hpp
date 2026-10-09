#pragma once

#include <array>
#include <cstddef>

enum class Color {
    White,
    Yellow,
    Green,
    Blue,
    Orange,
    Red
};

enum class Face {
      Up,
      Down,
      Front,
      Back,
      Left,
      Right
  };

  class RubiksCube {
     public:
         /** @brief Создаёт собранный кубик. */
         RubiksCube();

         /** @brief Проверяет, одноцветна ли каждая грань. */
         bool isSolved() const;

     private:
         static constexpr std::size_t kFaceCount = 6;
         static constexpr std::size_t kSideLength = 3;
         static constexpr std::size_t kCellsPerFace =
             kSideLength * kSideLength;

         std::array<std::array<Color, kCellsPerFace>, kFaceCount> faces_{};
     };
