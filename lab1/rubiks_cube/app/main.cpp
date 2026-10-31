#include "RubiksCube.hpp"

#include <array>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>

// Здесь диалог с пользователем и файлы. Сам RubiksCube ничего не знает о меню.
namespace {
constexpr std::size_t kSideLength = 3;
constexpr std::size_t kQuarterTurnsPerCycle = 4;
constexpr std::size_t kMaxScrambleMoves = 10000;
constexpr std::size_t kInitialScrambleMoves = 30;
constexpr std::array<const char*, 6> kFaceNames{"Верхняя (U)", "Нижняя (D)", "Передняя (F)",
                                                "Задняя (B)",  "Левая (L)",  "Правая (R)"};
constexpr std::array<char, 6> kColorSymbols{'W', 'Y', 'G', 'B', 'O', 'R'};

enum class MenuCommand { Exit, Show, Rotate, Scramble, Check, Save, Load, Reset, NewGame };

RubiksCube createScrambledCube() {
    RubiksCube cube;
    cube.scramble(kInitialScrambleMoves);
    return cube;
}

// nullopt означает, что ввод закончился или не удался; меню тогда завершает работу.
std::optional<std::string> readLine(const std::string& prompt) {
    std::cout << prompt;
    std::string line;
    if (!std::getline(std::cin, line)) {
        return std::nullopt;
    }
    return line;
}

std::optional<std::size_t> readNumber(const std::string& prompt, std::size_t minimum,
                                      std::size_t maximum) {
    while (const auto line = readLine(prompt)) {
        std::istringstream parser(*line);
        long long number{};
        std::string extra;
        // Принимаем одно целое число в диапазоне. Лишний текст вроде "2abc" не подходит.
        if ((parser >> number) && !(parser >> extra) && number >= static_cast<long long>(minimum) &&
            number <= static_cast<long long>(maximum)) {
            return static_cast<std::size_t>(number);
        }
        std::cout << "Введите целое число от " << minimum << " до " << maximum << ".\n";
    }
    return std::nullopt;
}

// Путь читаем целой строкой, поэтому пробелы в именах папок не мешают.
std::optional<std::filesystem::path> readPath(const std::string& prompt) {
    while (const auto line = readLine(prompt)) {
        if (!line->empty()) {
            return std::filesystem::path(*line);
        }
        std::cout << "Путь не должен быть пустым.\n";
    }
    return std::nullopt;
}

void showFaceRow(const RubiksCube& cube, Face face, std::size_t row) {
    for (std::size_t column = 0; column < kSideLength; ++column) {
        const Color color = cube.colorAt(face, row, column);
        std::cout << kColorSymbols.at(static_cast<std::size_t>(color)) << ' ';
    }
    std::cout << '\n';
}

void showCube(const RubiksCube& cube) {
    std::cout << '\n';
    for (std::size_t faceIndex = 0; faceIndex < kFaceNames.size(); ++faceIndex) {
        std::cout << kFaceNames[faceIndex] << ":\n";
        const Face face = static_cast<Face>(faceIndex);
        for (std::size_t row = 0; row < kSideLength; ++row) {
            showFaceRow(cube, face, row);
        }
        std::cout << '\n';
    }
}

void showStatus(const RubiksCube& cube) {
    std::cout << (cube.isSolved() ? "Кубик собран.\n" : "Кубик ещё не собран.\n");
}

void showFaceChoices() {
    for (std::size_t faceIndex = 0; faceIndex < kFaceNames.size(); ++faceIndex) {
        std::cout << faceIndex + 1 << ". " << kFaceNames[faceIndex] << '\n';
    }
}

void showMenu() {
    std::cout << "\n=== Кубик Рубика ===\n"
                 "1. Показать кубик\n"
                 "2. Повернуть грань\n"
                 "3. Перемешать\n"
                 "4. Проверить, собран ли кубик\n"
                 "5. Сохранить в файл\n"
                 "6. Загрузить из файла\n"
                 "7. Сбросить в собранное состояние\n"
                 "8. Новая случайная игра\n"
                 "0. Выход\n";
}

std::optional<Face> readFace() {
    showFaceChoices();
    const auto choice = readNumber("Грань: ", 1, kFaceNames.size());
    if (!choice) {
        return std::nullopt;
    }
    return static_cast<Face>(*choice - 1);
}

std::optional<std::size_t> readTurnCount() {
    std::cout << "Направление при взгляде прямо на выбранную грань:\n"
                 "1. По часовой стрелке\n2. Против часовой стрелки\n";
    const auto direction = readNumber("Направление: ", 1, 2);
    if (!direction) {
        return std::nullopt;
    }
    // Три поворота по часовой дают один против часовой, отдельный алгоритм не нужен.
    return *direction == 1 ? 1 : kQuarterTurnsPerCycle - 1;
}

bool rotateCube(RubiksCube& cube) {
    const auto face = readFace();
    if (!face) {
        return false;
    }
    const auto turns = readTurnCount();
    if (!turns) {
        return false;
    }
    for (std::size_t turn = 0; turn < *turns; ++turn) {
        cube.rotate(*face);
    }
    showCube(cube);
    showStatus(cube);
    return true;
}

bool scrambleCube(RubiksCube& cube) {
    const auto moves = readNumber("Количество поворотов: ", 0, kMaxScrambleMoves);
    if (!moves) {
        return false;
    }
    cube.scramble(*moves);
    std::cout << "Выполнено случайных поворотов: " << *moves << ".\n";
    showCube(cube);
    showStatus(cube);
    return true;
}

void writeCube(const std::filesystem::path& path, const RubiksCube& cube) {
    std::ofstream output(path);
    if (!output) {
        throw std::runtime_error("Не удалось открыть файл для записи");
    }
    output << cube;
    // Запись может дать ошибку при закрытии файла, поэтому проверяем поток и после close().
    output.close();
    if (!output) {
        throw std::runtime_error("Не удалось записать состояние кубика");
    }
}

RubiksCube loadCube(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Не удалось открыть файл для чтения");
    }
    RubiksCube cube;
    if (!(input >> cube)) {
        throw std::runtime_error("Некорректное состояние кубика в файле");
    }
    // После состояния разрешены только пробелы, а не второй кубик или лишний текст.
    input >> std::ws;
    if (input.bad() || !input.eof()) {
        throw std::runtime_error("После состояния кубика найдены лишние данные");
    }
    return cube;
}

// Три результата: true — можно писать, false — отмена, nullopt — ввод закончился.
std::optional<bool> confirmOverwrite(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        return true;
    }
    if (!std::filesystem::is_regular_file(path)) {
        throw std::runtime_error("Указанный путь не является обычным файлом");
    }
    const auto answer = readNumber("Файл существует. Перезаписать? 1 — да, 0 — нет: ", 0, 1);
    if (!answer) {
        return std::nullopt;
    }
    return *answer == 1;
}

bool saveCube(RubiksCube& cube) {
    const auto path = readPath("Путь к файлу для сохранения: ");
    if (!path) {
        return false;
    }
    const auto approved = confirmOverwrite(*path);
    if (!approved) {
        return false;
    }
    if (!*approved) {
        std::cout << "Сохранение отменено.\n";
        return true;
    }
    writeCube(*path, cube);
    std::cout << "Состояние сохранено: " << *path << '\n';
    return true;
}

bool loadCubeFromMenu(RubiksCube& cube) {
    const auto path = readPath("Путь к файлу для загрузки: ");
    if (!path) {
        return false;
    }
    const RubiksCube loadedCube = loadCube(*path);
    cube = loadedCube;
    std::cout << "Состояние загружено.\n";
    showCube(cube);
    showStatus(cube);
    return true;
}

bool resetCube(RubiksCube& cube) {
    const auto answer = readNumber("Сбросить в собранное состояние? 1 — да, 0 — нет: ", 0, 1);
    if (!answer) {
        return false;
    }
    if (*answer == 1) {
        cube = RubiksCube{};
        std::cout << "Кубик сброшен в собранное состояние.\n";
        showCube(cube);
    }
    return true;
}

bool startNewGame(RubiksCube& cube) {
    const auto answer = readNumber("Начать новую случайную игру? 1 — да, 0 — нет: ", 0, 1);
    if (!answer) {
        return false;
    }
    if (*answer == 1) {
        cube = createScrambledCube();
        std::cout << "Начата новая игра: выполнено " << kInitialScrambleMoves
                  << " случайных поворотов.\n";
        showCube(cube);
        showStatus(cube);
    }
    return true;
}

// true оставляет меню открытым, false завершает его (выход или конец ввода).
bool executeCommand(MenuCommand command, RubiksCube& cube) {
    switch (command) {
    case MenuCommand::Exit: return false;
    case MenuCommand::Show: showCube(cube); return true;
    case MenuCommand::Rotate: return rotateCube(cube);
    case MenuCommand::Scramble: return scrambleCube(cube);
    case MenuCommand::Check: showStatus(cube); return true;
    case MenuCommand::Save: return saveCube(cube);
    case MenuCommand::Load: return loadCubeFromMenu(cube);
    case MenuCommand::Reset: return resetCube(cube);
    case MenuCommand::NewGame: return startNewGame(cube);
    }
    throw std::logic_error("Неизвестная команда меню");
}

void runMenu(RubiksCube& cube) {
    while (true) {
        showMenu();
        const auto choice =
            readNumber("Выбор: ", 0, static_cast<std::size_t>(MenuCommand::NewGame));
        if (!choice) {
            break;
        }
        try {
            if (!executeCommand(static_cast<MenuCommand>(*choice), cube)) {
                break;
            }
        } catch (const std::exception& error) {
            // Ошибка одного действия не закрывает игру: можно выбрать другую команду.
            std::cerr << "Ошибка: " << error.what() << '\n';
        }
    }
    std::cout << "\nРабота завершена.\n";
}
} // namespace

// arguments[0] — путь программы, arguments[1], если есть, — файл кубика.
int main(int argumentCount, char* arguments[]) {
    if (argumentCount > 2) {
        std::cerr << "Использование: " << arguments[0] << " [файл кубика]\n";
        return 1;
    }
    try {
        // Файл загружаем как есть. Только запуск без файла создаёт случайную игру.
        RubiksCube cube = argumentCount == 2 ? loadCube(arguments[1]) : createScrambledCube();
        std::cout << "Цвета: W — белый, Y — жёлтый, G — зелёный, "
                     "B — синий, O — оранжевый, R — красный.\n";
        showCube(cube);
        runMenu(cube);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Ошибка: " << error.what() << '\n';
        return 1;
    }
}
