#include "ConsoleApplication.hpp"
#include "DemoData.hpp"
#include "SnapshotStore.hpp"
#include <fstream>
#include <sstream>

namespace shop::ui {
int runProgram(const std::vector<std::string>& arguments, std::istream& input, std::ostream& output,
               std::ostream& errors) {
    try {
        bool help = false;
        bool demo = false;
        std::optional<std::string> script;
        std::optional<std::string> load;
        for (std::size_t index = 0; index < arguments.size(); ++index) {
            const auto& flag = arguments[index];
            if (flag == "--help") {
                help = true;
            } else if (flag == "--demo") {
                demo = true;
            } else if (flag == "--script" || flag == "--load") {
                if (++index == arguments.size()) {
                    throw InputError("После " + flag + " нужен путь");
                }
                auto& selected = flag == "--script" ? script : load;
                if (selected) {
                    throw InputError("Параметр повторён: " + flag);
                }
                selected = arguments[index];
            } else {
                throw InputError("Неизвестный параметр: " + flag);
            }
        }
        if (help) {
            output << "shop_cli [--help] [--demo | --script FILE|-] [--load SNAPSHOT]\n"
                   << ConsoleApplication::help();
            return 0;
        }
        if (demo && (script || load)) {
            throw InputError("--demo не совмещается с --script/--load");
        }
        auto state = makeDemoStore();
        if (load) {
            SnapshotStore::load(*load, state);
        }
        ConsoleApplication application(std::move(state), output);
        if (demo) {
            output << "Воспроизводимая демонстрация\n";
            std::istringstream commands(demoScript());
            application.run(commands, false);
        } else if (script && *script != "-") {
            std::ifstream commands(*script);
            if (!commands) {
                throw InputError("Не удалось открыть сценарий");
            }
            application.run(commands, false);
        } else {
            application.run(input, !script);
        }
        return (script || demo) && application.errorCount() ? 1 : 0;
    } catch (const std::exception& error) {
        errors << "Ошибка запуска: " << error.what() << '\n';
        return 1;
    }
}
} // namespace shop::ui
