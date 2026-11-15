#include "Application.hpp"
#include <iostream>
#include <string_view>

// Точка входа отвечает за аргументы и фатальные ошибки, не за почтовые правила.
int main(int argc, char* argv[]) {
    if (argc == 2 && std::string_view(argv[1]) == "--help") {
        std::cout << "post_cli [--demo | --help]\nРусское интерактивное меню почты. --demo "
                     "добавляет клиентов и два черновика.\nВвод: одна строка на поле; деньги в "
                     "копейках BYN; время учебное, UTC.\nДанные не сохраняются между запусками.\n";
        return 0;
    }
    const bool demo = argc == 2 && std::string_view(argv[1]) == "--demo";
    if (argc != 1 && !demo) {
        std::cerr << "Неизвестные аргументы. Используйте post_cli --help.\n";
        return 2;
    }
    try {
        post::cli::Console console(std::cin, std::cout);
        post::cli::Application application(console);
        return application.run(demo);
    } catch (const std::exception& error) {
        std::cerr << "Не удалось запустить приложение: " << error.what() << '\n';
        return 1;
    }
}
