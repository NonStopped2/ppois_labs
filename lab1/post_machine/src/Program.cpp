#include "Program.hpp"

#include <cstdint>
#include <istream>
#include <ostream>
#include <stdexcept>
#include <string>

namespace {
const std::string kProgramKeyword = "PROGRAM";
}

bool Program::empty() const {
    return instructions_.empty();
}

std::size_t Program::size() const {
    return instructions_.size();
}

bool Program::contains(Label label) const {
    return instructions_.contains(label);
}

Program::Label Program::firstLabel() const {
    if (empty()) {
        throw std::out_of_range("Program is empty");
    }

    // map уже отсортирован: у первого элемента берём ключ, то есть номер команды.
    return instructions_.begin()->first;
}

const Instruction& Program::instructionAt(Label label) const {
    // at() сообщает об отсутствующем номере исключением, а не добавляет новую команду.
    return instructions_.at(label);
}

void Program::add(Label label, const Instruction& instruction) {
    if (label <= 0 || contains(label)) {
        throw std::invalid_argument("Instruction label must be positive and unique");
    }

    // Здесь не требуем готовых переходов: нужную команду могут добавить позже.
    instructions_.emplace(label, instruction);
}

void Program::replace(Label label, const Instruction& instruction) {
    instructions_.at(label) = instruction;
}

bool Program::remove(Label label) {
    // erase возвращает число удалённых элементов. Остальные номера не сдвигаем.
    return instructions_.erase(label) != 0;
}

bool Program::operator==(const Program& other) const {
    return instructions_ == other.instructions_;
}

bool Program::operator!=(const Program& other) const {
    return !(*this == other);
}

std::ostream& operator<<(std::ostream& output, const Program& program) {
    output << kProgramKeyword << ' ' << program.size() << '\n';
    for (const auto& entry : program.instructions_) {
        output << entry.first << ' ' << entry.second << '\n';
    }
    return output;
}

std::istream& operator>>(std::istream& input, Program& program) {
    std::string keyword;
    std::int64_t instructionCount{};
    if (!(input >> keyword >> instructionCount) || keyword != kProgramKeyword ||
        instructionCount < 0) {
        input.setstate(std::ios::failbit);
        return input;
    }

    // Собираем программу отдельно: add() заодно проверит положительность и уникальность.
    Program candidate;
    for (std::int64_t index = 0; index < instructionCount; ++index) {
        Program::Label label{};
        Instruction instruction;
        if (!(input >> label >> instruction)) {
            return input;
        }
        try {
            candidate.add(label, instruction);
        } catch (const std::invalid_argument&) {
            input.setstate(std::ios::failbit);
            return input;
        }
    }
    program = candidate;
    return input;
}
