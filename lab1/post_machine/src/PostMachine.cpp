#include "PostMachine.hpp"

#include <istream>
#include <ostream>
#include <stdexcept>
#include <string>

namespace {
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
    return program.contains(current) &&
           (!halted || program.instructionAt(current).operation() == Operation::Halt);
}
} // namespace

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

    if (instruction.operation() == Operation::Halt) {
        halted_ = true;
        return true;
    }

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
    std::size_t executed = 0;
    while (!halted_ && executed < maxSteps) {
        const Label executedLabel = currentLabel_;
        step();
        ++executed;
        if (log != nullptr) {
            writeLog(*log, executedLabel);
        }
    }
    if (!halted_) {
        throw std::runtime_error("Execution step limit reached");
    }
    return executed;
}

void PostMachine::writeLog(std::ostream& log, Label executedLabel) const {
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
    PostMachine candidate;
    if (!(input >> candidate.tape_ >> candidate.program_) ||
        !readControlState(input, candidate.program_, candidate.currentLabel_, candidate.halted_)) {
        input.setstate(std::ios::failbit);
        return input;
    }
    machine = candidate;
    return input;
}
