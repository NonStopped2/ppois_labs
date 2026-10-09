#include "RubiksCube.hpp"

   #include <iostream>

   int main() {
       RubiksCube cube;
       RubiksCube copiedCube(cube);

       RubiksCube assignedCube;
       assignedCube = cube;

       const bool solved = cube.isSolved();

       const Color topLeftColor = cube.colorAt(Face::Up, 0, 0);
       const bool topLeftIsWhite = topLeftColor == Color::White;

       const bool copyEqualsOriginal = copiedCube == cube;
       const bool copyDiffersFromOriginal = copiedCube != cube;
       const bool assignedEqualsOriginal = assignedCube == cube;

       std::cout << std::boolalpha;
       std::cout << "Cube is solved: " << solved << '\n';
       std::cout << "Top-left cell is white: " << topLeftIsWhite << '\n';
       std::cout << "Copy equals original: " << copyEqualsOriginal << '\n';
       std::cout << "Copy differs from original: " << copyDiffersFromOriginal << '\n';
       std::cout << "Assigned cube equals original: " << assignedEqualsOriginal << '\n';

       return 0;
   }
