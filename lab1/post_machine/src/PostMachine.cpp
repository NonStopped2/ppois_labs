#include "PostMachine.hpp"

#include <istream>
#include <ostream>
#include <stdexcept>
#include <string>

namespace {
// Проверяем, что сохранённая текущая команда и флаг остановки подходят к программе.
bool readControlState(std::istream& input, const Program& program, PostMachine::Label& current,
                      bool& halted) {
    std::string currentKeyword;
    std::string haltedKeyword;
    int flag{};
    if (!(input >> currentKeyword >> current >> haltedKeyword >> flag) ||
        currentKeyword != "CURRENT" || haltedKeyword != "HALTED" || (flag != 0 && flag != 1)) {
        return false;
    }
    halted = flag == 1;
    if (program.empty()) {
        return current == 0 && !halted;
    }
    // Остановленная машина должна стоять на HALT, а не на обычной команде.
    return program.contains(current) &&
           (!halted || program.instructionAt(current).operation() == Operation::Halt);
}
} // namespace

// Ссылки нужны для передачи без лишней копии, но в полях сохраняются свои копии данных.
PostMachine::PostMachine(const Program& program, const Tape& tape)
    : tape_(tape), program_(program), currentLabel_(program.empty() ? 0 : program.firstLabel()) {}

const Tape& PostMachine::tape() const {
    return tape_;
}

const Program& PostMachine::program() const {
    return program_;
}

PostMachine::Label PostMachine::currentLabel() const {
    return currentLabel_;
}

bool PostMachine::isHalted() const {
    return halted_;
}

PostMachine::Label PostMachine::nextLabelFor(const Instruction& instruction) const {
    Label next = instruction.next();

    if (instruction.operation() == Operation::Branch && tape_.currentCellMarked()) {
        next = instruction.nextIfMarked();
    }

    // Проверяем только выбранный переход: другая ветка IF сейчас не выполняется.
    if (!program_.contains(next)) {
        throw std::runtime_error("Jump target does not exist");
    }

    return next;
}

void PostMachine::applyOperation(Operation operation) {
    switch (operation) {
    case Operation::Mark: tape_.mark(); break;
    case Operation::Erase: tape_.erase(); break;
    case Operation::Left: tape_.moveLeft(); break;
    case Operation::Right: tape_.moveRight(); break;
    // IF выбирает следующую команду, HALT останавливает: сами ленту они не меняют.
    case Operation::Branch:
    case Operation::Halt: break;
    }
}

bool PostMachine::step() {
    if (halted_) {
        return false;
    }

    if (program_.empty()) {
        throw std::runtime_error("Cannot execute an empty program");
    }

    const Instruction& instruction = program_.instructionAt(currentLabel_);

    // HALT тоже считается выполненным шагом; его номер оставляем текущим.
    if (instruction.operation() == Operation::Halt) {
        halted_ = true;
        return true;
    }

    // Сначала проверяем переход, затем действие. При ошибке этот шаг не меняет машину.
    const Label next = nextLabelFor(instruction);
    applyOperation(instruction.operation());
    currentLabel_ = next;
    return true;
}

std::size_t PostMachine::run(std::size_t maxSteps) {
    return runImpl(maxSteps, nullptr);
}

std::size_t PostMachine::run(std::size_t maxSteps, std::ostream& log) {
    return runImpl(maxSteps, &log);
}

std::size_t PostMachine::runImpl(std::size_t maxSteps, std::ostream* log) {
    // Общий цикл для двух run(): nullptr означает, что журнал не нужен.
    std::size_t executed = 0;
    while (!halted_ && executed < maxSteps) {
        // Запоминаем номер до шага: после него currentLabel_ уже может измениться.
        const Label executedLabel = currentLabel_;
        step();
        ++executed;
        if (log != nullptr) {
            writeLog(*log, executedLabel);
        }
    }
    // Лимит защищает от зацикливания. Сделанные шаги остаются, выполнение можно продолжить.
    if (!halted_) {
        throw std::runtime_error("Execution step limit reached");
    }
    return executed;
}

void PostMachine::writeLog(std::ostream& log, Label executedLabel) const {
    // Пишем состояние после успешного шага, в том числе после HALT.
    log << "EXECUTED " << executedLabel << '\n'
        << tape_ << "CURRENT " << currentLabel_ << "\nHALTED " << static_cast<int>(halted_)
        << "\n\n";
    if (!log) {
        throw std::runtime_error("Failed to write execution log");
    }
}

bool PostMachine::operator==(const PostMachine& other) const {
    return tape_ == other.tape_ && program_ == other.program_ &&
           currentLabel_ == other.currentLabel_ && halted_ == other.halted_;
}

bool PostMachine::operator!=(const PostMachine& other) const {
    return !(*this == other);
}

std::ostream& operator<<(std::ostream& output, const PostMachine& machine) {
    return output << machine.tape_ << machine.program_ << "CURRENT " << machine.currentLabel_
                  << "\nHALTED " << static_cast<int>(machine.halted_) << '\n';
}

std::istream& operator>>(std::istream& input, PostMachine& machine) {
    // Снимок хранит не только ленту и программу, но и место продолжения исполнения.
    // Заменяем машину только после проверки всех частей снимка.
    PostMachine candidate;
    if (!(input >> candidate.tape_ >> candidate.program_) ||
        !readControlState(input, candidate.program_, candidate.currentLabel_, candidate.halted_)) {
        input.setstate(std::ios::failbit);
        return input;
    }
    machine = candidate;
    return input;
}
