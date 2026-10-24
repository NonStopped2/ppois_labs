#include "Tape.hpp"

#include <istream>
#include <limits>
#include <ostream>
#include <stdexcept>
#include <string>

namespace {
const std::string kHeadKeyword = "HEAD";
const std::string kMarksKeyword = "MARKS";
} // namespace

Tape::Position Tape::headPosition() const {
    return headPosition_;
}

bool Tape::isMarked(Position position) const {
    return markedCells_.contains(position);
}

bool Tape::currentCellMarked() const {
    return isMarked(headPosition_);
}

void Tape::setMarked(Position position, bool marked) {
    if (marked) {
        markedCells_.insert(position);
    } else {
        markedCells_.erase(position);
    }
}

void Tape::moveTo(Position position) {
    headPosition_ = position;
}

void Tape::moveLeft() {
    if (headPosition_ == std::numeric_limits<Position>::min()) {
        throw std::overflow_error("Tape position is below the supported range");
    }

    --headPosition_;
}

void Tape::moveRight() {
    if (headPosition_ == std::numeric_limits<Position>::max()) {
        throw std::overflow_error("Tape position exceeds the supported range");
    }

    ++headPosition_;
}

void Tape::mark() {
    if (currentCellMarked()) {
        throw std::runtime_error("Cannot mark an already marked cell");
    }

    setMarked(headPosition_, true);
}

void Tape::erase() {
    if (!currentCellMarked()) {
        throw std::runtime_error("Cannot erase an empty cell");
    }

    setMarked(headPosition_, false);
}

bool Tape::operator==(const Tape& other) const {
    return headPosition_ == other.headPosition_ && markedCells_ == other.markedCells_;
}

bool Tape::operator!=(const Tape& other) const {
    return !(*this == other);
}

std::ostream& operator<<(std::ostream& output, const Tape& tape) {
    output << kHeadKeyword << ' ' << tape.headPosition_ << '\n';
    output << kMarksKeyword << ' ' << tape.markedCells_.size();

    for (Tape::Position position : tape.markedCells_) {
        output << ' ' << position;
    }

    output << '\n';
    return output;
}

std::istream& operator>>(std::istream& input, Tape& tape) {
    Tape candidate;
    std::string headKeyword;
    std::string marksKeyword;
    std::int64_t markCount{};

    if (!(input >> headKeyword >> candidate.headPosition_ >> marksKeyword >> markCount) ||
        headKeyword != kHeadKeyword || marksKeyword != kMarksKeyword || markCount < 0) {
        input.setstate(std::ios::failbit);
        return input;
    }

    for (std::int64_t index = 0; index < markCount; ++index) {
        Tape::Position position{};

        if (!(input >> position) || candidate.isMarked(position)) {
            input.setstate(std::ios::failbit);
            return input;
        }

        candidate.setMarked(position, true);
    }

    tape = candidate;
    return input;
}
