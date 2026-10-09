 #include "RubiksCube.hpp"

   #include <iostream>

   int main() {
       RubiksCube cube;
       const bool solved = cube.isSolved();

       const Color topLeftColor = cube.colorAt(Face::Up, 0, 0);
       const bool topLeftIsWhite = topLeftColor == Color::White;

       std::cout << std::boolalpha;
       std::cout << "Cube is solved: " << solved << '\n';
       std::cout << "Top-left cell is white: " << topLeftIsWhite << '\n';

       return 0;
   }
