#include "ConsoleApplication.hpp"
#include <iostream>

int main(int argc, char** argv) {
    const std::vector<std::string> arguments(argv + 1, argv + argc);
    return shop::ui::runProgram(arguments, std::cin, std::cout, std::cerr);
}
