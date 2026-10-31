#include "PostMachine.hpp"

#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>

// Меню и файлы находятся здесь, а классы ленты, команд и машины отвечают за саму модель.
namespace {
// В файловом режиме лимит фиксирован, в меню пользователь выбирает его сам.
constexpr std::size_t kDefaultStepLimit = 10000;
constexpr std::int64_t kMaxInteractiveSteps = 1000000;

enum class Command {
    Exit,
    State,
    Program,
    Add,
    Replace,
    Remove,
    Cell,
    Head,
    Step,
    Run,
    Restart,
    Load,
    Save
};

// nullopt — ввод закончился или не удался. Это не число 0 и не пустая строка.
std::optional<std::string> readLine(const std::string& prompt) {
    std::cout << prompt;
    std::string line;
    if (!std::getline(std::cin, line)) {
        return std::nullopt;
    }
    return line;
}

std::optional<std::int64_t> readNumber(const std::string& prompt, std::int64_t minimum,
                                       std::int64_t maximum) {
    while (const auto line = readLine(prompt)) {
        std::istringstream parser(*line);
        std::int64_t number{};
        std::string extra;
        // Проверяем всю строку: одно целое число, без лишнего текста, в нужном диапазоне.
        if ((parser >> number) && !(parser >> extra) && number >= minimum && number <= maximum) {
            return number;
        }
        std::cout << "Введите целое число от " << minimum << " до " << maximum << ".\n";
    }
    return std::nullopt;
}

// getline сохраняет пробелы внутри пути, в отличие от чтения через cin >> строка.
std::optional<std::filesystem::path> readPath() {
    while (const auto line = readLine("Путь к файлу: ")) {
        if (!line->empty()) {
            return std::filesystem::path(*line);
        }
        std::cout << "Путь не должен быть пустым.\n";
    }
    return std::nullopt;
}

std::optional<Instruction> readInstruction() {
    while (const auto line =
               readLine("Команда (MARK n, ERASE n, LEFT n, RIGHT n, IF n m, HALT): ")) {
        std::istringstream parser(*line);
        Instruction instruction;
        if (parser >> instruction) {
            // Оператор >> читает одну команду, а здесь ещё запрещаем лишние аргументы.
            parser >> std::ws;
            if (parser.eof()) {
                return instruction;
            }
        }
        std::cout << "Некорректная команда или лишние аргументы.\n";
    }
    return std::nullopt;
}

void showState(const PostMachine& machine) {
    std::cout << "\nЛента (* — метка, отсутствующие координаты пусты):\n"
              << machine.tape() << "Текущая команда: " << machine.currentLabel() << '\n'
              << (machine.isHalted() ? "Машина остановлена.\n" : "Машина не остановлена.\n");
}

void showMenu() {
    std::cout << "\n=== Машина Поста ===\n"
                 "1. Показать состояние\n2. Показать программу\n"
                 "3. Добавить команду\n4. Заменить команду\n5. Удалить команду\n"
                 "6. Изменить клетку\n7. Переместить каретку\n8. Выполнить шаг\n"
                 "9. Выполнить программу\n10. Перезапустить с исходной лентой\n"
                 "11. Загрузить состояние\n12. Сохранить состояние\n0. Выход\n";
}

// Учебный пример: пройти блок меток вправо и добавить метку в первую пустую клетку.
PostMachine makeExampleMachine() {
    Program program;
    program.add(1, Instruction(Operation::Branch, 3, 2));
    program.add(2, Instruction(Operation::Right, 1));
    program.add(3, Instruction(Operation::Mark, 4));
    program.add(4, Instruction{});
    return PostMachine(program);
}

// После редактирования начинаем с первой команды и обновляем снимок для перезапуска.
void reconfigure(PostMachine& machine, PostMachine& initial, const Program& program,
                 const Tape& tape) {
    const PostMachine candidate(program, tape);
    machine = candidate;
    initial = candidate;
    std::cout << "Настройки изменены. Исполнение начнётся с первой команды.\n";
}

bool editInstruction(PostMachine& machine, PostMachine& initial, bool replace) {
    const auto label = readNumber("Номер команды: ", 1, std::numeric_limits<Program::Label>::max());
    if (!label) {
        return false;
    }
    const auto instruction = readInstruction();
    if (!instruction) {
        return false;
    }
    // Правим копию: если номер не подходит, рабочая программа останется прежней.
    Program program = machine.program();
    if (replace) {
        program.replace(*label, *instruction);
    } else {
        program.add(*label, *instruction);
    }
    reconfigure(machine, initial, program, machine.tape());
    return true;
}

bool removeInstruction(PostMachine& machine, PostMachine& initial) {
    const auto label = readNumber("Номер команды: ", 1, std::numeric_limits<Program::Label>::max());
    if (!label) {
        return false;
    }
    Program program = machine.program();
    if (program.remove(*label)) {
        reconfigure(machine, initial, program, machine.tape());
    } else {
        std::cout << "Команда не найдена.\n";
    }
    return true;
}

std::optional<Tape::Position> readPosition() {
    return readNumber("Координата клетки: ", std::numeric_limits<Tape::Position>::min(),
                      std::numeric_limits<Tape::Position>::max());
}

bool editCell(PostMachine& machine, PostMachine& initial) {
    const auto position = readPosition();
    if (!position) {
        return false;
    }
    const auto marked = readNumber("Значение (0 — пусто, 1 — метка): ", 0, 1);
    if (!marked) {
        return false;
    }
    Tape tape = machine.tape();
    tape.setMarked(*position, *marked == 1);
    reconfigure(machine, initial, machine.program(), tape);
    return true;
}

bool moveHead(PostMachine& machine, PostMachine& initial) {
    const auto position = readPosition();
    if (!position) {
        return false;
    }
    Tape tape = machine.tape();
    tape.moveTo(*position);
    reconfigure(machine, initial, machine.program(), tape);
    return true;
}

void stepMachine(PostMachine& machine) {
    if (!machine.step()) {
        std::cout << "Машина уже остановлена. Перезапустите её для повторного выполнения.\n";
    }
    showState(machine);
}

bool runMachine(PostMachine& machine) {
    const auto limit = readNumber("Максимум команд: ", 1, kMaxInteractiveSteps);
    if (!limit) {
        return false;
    }
    const auto logging = readNumber("Журнал после каждой команды? 1 — да, 0 — нет: ", 0, 1);
    if (!logging) {
        return false;
    }
    const auto maximum = static_cast<std::size_t>(*limit);
    const auto executed = *logging == 1 ? machine.run(maximum, std::cout) : machine.run(maximum);
    std::cout << "Выполнено команд: " << executed << '\n';
    showState(machine);
    return true;
}

bool restartMachine(PostMachine& machine, const PostMachine& initial) {
    const auto answer =
        readNumber("Восстановить исходную ленту и начать сначала? 1 — да, 0 — нет: ", 0, 1);
    if (!answer) {
        return false;
    }
    if (*answer == 1) {
        machine = initial;
        showState(machine);
    }
    return true;
}

PostMachine loadMachine(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Не удалось открыть файл для чтения");
    }
    PostMachine candidate;
    if (!(input >> candidate)) {
        throw std::runtime_error("Некорректное состояние машины в файле");
    }
    // Проверяем весь файл: после снимка допустимы только пробелы.
    input >> std::ws;
    if (input.bad() || !input.eof()) {
        throw std::runtime_error("После состояния машины найдены лишние данные");
    }
    return candidate;
}

bool loadFromMenu(PostMachine& machine, PostMachine& initial) {
    const auto path = readPath();
    if (!path) {
        return false;
    }
    const PostMachine candidate = loadMachine(*path);
    // Продолжаем с сохранённой команды, но перезапуск пойдёт с начала с загруженной лентой.
    const PostMachine start(candidate.program(), candidate.tape());
    machine = candidate;
    initial = start;
    std::cout << "Состояние загружено.\n";
    showState(machine);
    return true;
}

// false означает отказ, nullopt — конец ввода. Это разные ситуации для меню.
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

void writeMachine(const std::filesystem::path& path, const PostMachine& machine) {
    std::ofstream output(path);
    if (!output) {
        throw std::runtime_error("Не удалось открыть файл для записи");
    }
    output << machine;
    // Проверяем и закрытие: ошибка записи может появиться только при сбросе буфера.
    output.close();
    if (!output) {
        throw std::runtime_error("Не удалось записать состояние машины");
    }
}

bool saveFromMenu(const PostMachine& machine) {
    const auto path = readPath();
    if (!path) {
        return false;
    }
    const auto approved = confirmOverwrite(*path);
    if (!approved) {
        return false;
    }
    if (*approved) {
        writeMachine(*path, machine);
        std::cout << "Состояние сохранено.\n";
    } else {
        std::cout << "Сохранение отменено.\n";
    }
    return true;
}

// true — продолжить меню, false — выйти. Отмена действия не обязательно означает выход.
bool executeCommand(Command command, PostMachine& machine, PostMachine& initial) {
    switch (command) {
    case Command::Exit: return false;
    case Command::State: showState(machine); return true;
    case Command::Program: std::cout << machine.program(); return true;
    case Command::Add: return editInstruction(machine, initial, false);
    case Command::Replace: return editInstruction(machine, initial, true);
    case Command::Remove: return removeInstruction(machine, initial);
    case Command::Cell: return editCell(machine, initial);
    case Command::Head: return moveHead(machine, initial);
    case Command::Step: stepMachine(machine); return true;
    case Command::Run: return runMachine(machine);
    case Command::Restart: return restartMachine(machine, initial);
    case Command::Load: return loadFromMenu(machine, initial);
    case Command::Save: return saveFromMenu(machine);
    }
    throw std::logic_error("Неизвестная команда меню");
}

void runMenu(PostMachine& machine) {
    // Этот снимок нужен для перезапуска. Обычные шаги меняют machine, но не initial.
    PostMachine initial(machine);
    while (true) {
        showMenu();
        const auto choice = readNumber("Выбор: ", 0, static_cast<std::int64_t>(Command::Save));
        if (!choice) {
            break;
        }
        try {
            if (!executeCommand(static_cast<Command>(*choice), machine, initial)) {
                break;
            }
        } catch (const std::exception& error) {
            // Ошибка команды не закрывает меню; успешные предыдущие шаги сохраняются.
            std::cerr << "Ошибка: " << error.what() << '\n';
        }
    }
    std::cout << "Работа завершена.\n";
}

// Файловый запуск сразу выполняет программу и завершается, меню здесь не открываем.
int runFile(const std::filesystem::path& path, bool logging) {
    PostMachine machine = loadMachine(path);
    const auto executed =
        logging ? machine.run(kDefaultStepLimit, std::cout) : machine.run(kDefaultStepLimit);
    std::cout << "Выполнено команд: " << executed << '\n';
    showState(machine);
    return 0;
}

void showUsage(const char* executable) {
    std::cout << "Использование: " << executable
              << " [файл машины [-log]]\n"
                 "Без аргументов открывается меню с учебным примером.\n"
                 "С файлом программа выполняется сразу; лимит — "
              << kDefaultStepLimit << " команд.\n";
}
} // namespace

// Первый аргумент — сама программа; дальше возможны файл и необязательный -log.
int main(int argumentCount, char* arguments[]) {
    try {
        if (argumentCount == 2 && std::string(arguments[1]) == "--help") {
            showUsage(arguments[0]);
            return 0;
        }
        if (argumentCount == 1) {
            PostMachine machine = makeExampleMachine();
            std::cout << "Учебный пример: добавление одной метки справа от блока меток.\n";
            showState(machine);
            runMenu(machine);
            return 0;
        }
        if (argumentCount == 2 || (argumentCount == 3 && std::string(arguments[2]) == "-log")) {
            return runFile(arguments[1], argumentCount == 3);
        }
        showUsage(arguments[0]);
        return 1;
    } catch (const std::exception& error) {
        std::cerr << "Ошибка: " << error.what() << '\n';
        return 1;
    }
}
