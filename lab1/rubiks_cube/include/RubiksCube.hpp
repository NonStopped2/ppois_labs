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

         /** @brief Возвращает цвет клетки по грани, строке и столбцу.
           *  @param face Грань кубика.
           *  @param row Строка от 0 до 2.
           *  @param column Столбец от 0 до 2.
           *  @throws std::out_of_range Если грань или координаты недопустимы.
           */
          Color colorAt(Face face, std::size_t row, std::size_t column) const;

        /** @brief Проверяет совпадение цветов всех соответствующих клеток. */
        bool operator==(const RubiksCube& other) const;

        /** @brief Проверяет, различаются ли состояния кубиков. */
        bool operator!=(const RubiksCube& other) const;

        /** @brief Поворачивает переднюю грань по часовой стрелке
        *  при взгляде прямо на неё.
        */
        void rotateFront();


     private:
         void rotateFaceClockwise(Face face);

         static constexpr std::size_t kFaceCount = 6;
         static constexpr std::size_t kSideLength = 3;
         static constexpr std::size_t kCellsPerFace =
             kSideLength * kSideLength;

         std::array<std::array<Color, kCellsPerFace>, kFaceCount> faces_{};
     };
