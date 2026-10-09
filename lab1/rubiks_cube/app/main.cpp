 #include "RubiksCube.hpp"

   #include <iostream>

   int main() {
       RubiksCube cube;
       const bool solved = cube.isSolved();

       std::cout << "Cube is solved: " << std::boolalpha << solved << '\n';

       return 0;
   }
