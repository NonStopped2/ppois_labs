#include "RubiksCube.hpp"
#include <istream>
#include <ostream>
#include <random>
#include <stdexcept>

RubiksCube::RubiksCube() {
    for (std::size_t faceIndex = 0; faceIndex < kFaceCount; ++faceIndex) {
        faces_[faceIndex].fill(kSolvedColors[faceIndex]);
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

Color RubiksCube::colorAt(Face face, std::size_t row, std::size_t column) const {
    const std::size_t faceIndex = static_cast<std::size_t>(face);

    if (faceIndex >= kFaceCount || row >= kSideLength || column >= kSideLength) {
        throw std::out_of_range("Cube cell coordinates are out of range");
    }

    // Переводим строку и столбец в индекс одномерного массива грани.
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
    // Читаем из копии, чтобы новые цвета не затирали ещё не перенесённые старые.
    const auto previousFace = faces_[faceIndex];

    for (std::size_t row = 0; row < kSideLength; ++row) {
        for (std::size_t column = 0; column < kSideLength; ++column) {
            const std::size_t oldIndex = row * kSideLength + column;
            // Поворот 3x3 по часовой: (строка, столбец) -> (столбец, 2 - строка).
            const std::size_t newRow = column;
            const std::size_t newColumn = kSideLength - 1 - row;
            const std::size_t newIndex = newRow * kSideLength + newColumn;

            faces_[faceIndex][newIndex] = previousFace[oldIndex];
        }
    }
}

void RubiksCube::rotateFront() {
    rotate(Face::Front);
}

// Для каждой грани задаём четыре полосы соседей в порядке их переноса.
// Обратный порядок индексов у некоторых полос учитывает, как повёрнуты соседние грани.
const std::array<RubiksCube::NeighborStrips, RubiksCube::kFaceCount> RubiksCube::kNeighborTable{
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

RubiksCube::NeighborStrips RubiksCube::neighborStripsFor(Face face) const {
    return kNeighborTable.at(static_cast<std::size_t>(face));
}

void RubiksCube::rotateNeighborStrips(const NeighborStrips& strips) {
    // Все полосы берём из одного старого состояния, а записываем уже в новое.
    const auto previousFaces = faces_;

    for (std::size_t stripIndex = 0; stripIndex < kNeighborCount; ++stripIndex) {
        const NeighborStrip& sourceStrip = strips[stripIndex];
        // Остаток от деления замыкает круг: последняя полоса переходит в первую.
        const NeighborStrip& destinationStrip = strips[(stripIndex + 1) % kNeighborCount];

        const auto& sourceFace = previousFaces[static_cast<std::size_t>(sourceStrip.face)];
        auto& destinationFace = faces_[static_cast<std::size_t>(destinationStrip.face)];

        for (std::size_t offset = 0; offset < kSideLength; ++offset) {
            destinationFace[destinationStrip.indices[offset]] =
                sourceFace[sourceStrip.indices[offset]];
        }
    }
}
void RubiksCube::rotate(Face face) {
    // at() внутри проверит номер грани до того, как мы начнём менять кубик.
    const NeighborStrips strips = neighborStripsFor(face);

    rotateFaceClockwise(face);
    rotateNeighborStrips(strips);
}

void RubiksCube::scramble(std::size_t moveCount) {
    if (moveCount == 0) {
        return;
    }

    // Без заданного seed получаем начальное значение генератора из random_device.
    std::random_device seedSource;
    const auto seed = static_cast<std::uint32_t>(seedSource());

    scramble(moveCount, seed);
}

void RubiksCube::scramble(std::size_t moveCount, std::uint32_t seed) {
    // Одинаковый seed повторяет повороты в той же среде. Цвета отдельно не перемешиваем.
    std::mt19937 generator(seed);
    std::uniform_int_distribution<std::size_t> faceDistribution(0, kFaceCount - 1);

    for (std::size_t move = 0; move < moveCount; ++move) {
        const std::size_t faceIndex = faceDistribution(generator);
        const Face selectedFace = static_cast<Face>(faceIndex);

        rotate(selectedFace);
    }
}

bool RubiksCube::tryParseColor(char symbol, Color& color) {

    for (std::size_t index = 0; index < kColorSymbols.size(); ++index) {
        if (kColorSymbols[index] == symbol) {
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
    // Проверяем центры и по девять клеток каждого цвета, но не все правила сборки кубика.
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
    // Старый кубик заменяем только после успешного чтения и проверки временного.
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
        for (std::size_t cellIndex = 0; cellIndex < RubiksCube::kCellsPerFace; ++cellIndex) {
            if (cellIndex > 0) {
                output << ' ';
            }

            const std::size_t colorIndex = static_cast<std::size_t>(face[cellIndex]);

            output << RubiksCube::kColorSymbols[colorIndex];
        }

        output << '\n';
    }

    return output;
}
