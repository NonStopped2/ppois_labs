 #include "RubiksCube.hpp"

   #include <iostream>

   int main() {
       constexpr std::size_t kQuarterTurnsPerCycle = 4;

       RubiksCube cube;
       const RubiksCube originalCube(cube);

       cube.rotateFront();

       const bool solvedAfterTurn = cube.isSolved();
       const bool equalsOriginalAfterTurn = cube == originalCube;

       const Color upperBottomLeftColor = cube.colorAt(Face::Up, 2, 0);
       const bool upperCellIsOrange = upperBottomLeftColor == Color::Orange;

       std::cout << std::boolalpha;
       std::cout << "Solved after one turn: " << solvedAfterTurn << '\n';
       std::cout << "Equals original after one turn: "
                 << equalsOriginalAfterTurn << '\n';
       std::cout << "Upper bottom-left cell is orange: "
                 << upperCellIsOrange << '\n';

       for (std::size_t turn = 1; turn < kQuarterTurnsPerCycle; ++turn) {
           cube.rotateFront();
       }

       const bool restored = cube == originalCube;
       std::cout << "Restored after four turns: " << restored << '\n';

       return 0;
   }
