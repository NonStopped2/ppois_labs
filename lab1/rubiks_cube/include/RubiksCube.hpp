#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <iosfwd> //  std::istream и std::ostream без реализации

enum class Color { White, Yellow, Green, Blue, Orange, Red };

enum class Face { Up, Down, Front, Back, Left, Right };

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
  bool operator==(const RubiksCube &other) const;

  /** @brief Проверяет, различаются ли состояния кубиков. */
  bool operator!=(const RubiksCube &other) const;

  /** @brief Поворачивает переднюю грань по часовой стрелке
   *  при взгляде прямо на неё.
   */
  void rotateFront();

  /** @brief Поворачивает выбранную грань по часовой стрелке
   *  при взгляде прямо на неё.
   *  @throws std::out_of_range Если значение грани недопустимо.
   */
  void rotate(Face face);

  /** @brief Перемешивает текущее состояние случайными поворотами.
     *  @param moveCount Количество поворотов. Ноль не изменяет кубик.
     */
    void scramble(std::size_t moveCount);

    /** @brief Перемешивает текущее состояние с заданным seed.
     *  @param moveCount Количество поворотов.
     *  @param seed Начальное значение генератора.
     */
    void scramble(std::size_t moveCount, std::uint32_t seed);

    /** @brief Записывает цвета шести граней в текстовый поток. */
    friend std::ostream& operator<<(std::ostream& output, const RubiksCube& cube);

       /** @brief Читает состояние из потока.
        *  При некорректных данных устанавливает failbit,
        *  оставляя исходный кубик неизменным.
        */
    friend std::istream& operator>>(std::istream& input, RubiksCube& cube);

private:
  void rotateFaceClockwise(Face face);

  static constexpr std::size_t kFaceCount = 6;
  static constexpr std::size_t kSideLength = 3;
  static constexpr std::size_t kCellsPerFace = kSideLength * kSideLength;

  static constexpr std::size_t kNeighborCount = 4;

  static constexpr std::size_t kCenterIndex = kCellsPerFace / 2;

    static constexpr std::array<Color, kFaceCount> kSolvedColors{
        Color::White,
        Color::Yellow,
        Color::Green,
        Color::Blue,
        Color::Orange,
        Color::Red
    };

    static constexpr std::array<char, kFaceCount> kColorSymbols{
        'W', 'Y', 'G', 'B', 'O', 'R'
    };

  struct NeighborStrip {
    Face face;
    std::array<std::size_t, kSideLength> indices;
  };

  using NeighborStrips = std::array<NeighborStrip, kNeighborCount>;

  NeighborStrips neighborStripsFor(Face face) const;
  void rotateNeighborStrips(const NeighborStrips &strips);

  static bool tryParseColor(char symbol, Color& color);
     bool readState(std::istream& input);
     bool hasValidColors() const;

  std::array<std::array<Color, kCellsPerFace>, kFaceCount> faces_{};
};
