#include "Tape.hpp"

#include <exception>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
constexpr Tape::Position kFirstVisiblePosition = -2;
constexpr Tape::Position kLastVisiblePosition = 2;
constexpr Tape::Position kDistantPosition = 1'000'000'000'000;

void showTape(const Tape &tape) {
  std::cout << "Head position: " << tape.headPosition() << '\n';
  std::cout << "Cells " << kFirstVisiblePosition << ".." << kLastVisiblePosition << ": ";
  for (Tape::Position position = kFirstVisiblePosition;
       position <= kLastVisiblePosition; ++position) {
    const bool selected = position == tape.headPosition();
    std::cout << (selected ? '[' : ' ')
              << (tape.isMarked(position) ? '*' : '.')
              << (selected ? ']' : ' ') << ' ';
  }
  std::cout << '\n';
}

bool demonstrateMovement() {
  Tape tape;
  tape.mark();
  tape.moveRight();
  tape.mark();
  const bool twoMarks = tape.headPosition() == 1 && tape.isMarked(0) && tape.isMarked(1);
  showTape(tape);

  tape.moveLeft();
  tape.moveLeft();
  tape.mark();
  const bool negativePosition = tape.headPosition() == -1 && tape.currentCellMarked();
  const bool oldMarksPreserved = tape.isMarked(0) && tape.isMarked(1);
  showTape(tape);

  const bool correct = twoMarks && negativePosition && oldMarksPreserved;
  std::cout << "Movement and negative coordinates: " << correct << '\n';
  return correct;
}

bool demonstrateEditing() {
  Tape tape;
  tape.moveTo(kDistantPosition);
  tape.setMarked(kDistantPosition, true);
  tape.setMarked(kDistantPosition, true);
  tape.setMarked(-kDistantPosition, true);
  const bool marked = tape.currentCellMarked() && tape.isMarked(-kDistantPosition);

  tape.erase();
  const bool erased = !tape.currentCellMarked() && tape.isMarked(-kDistantPosition);
  tape.setMarked(-kDistantPosition, false);
  tape.setMarked(-kDistantPosition, false);
  const bool removed = !tape.isMarked(-kDistantPosition);

  const bool correct = marked && erased && removed;
  std::cout << "Editing distant cells: " << correct << '\n';
  return correct;
}

bool demonstrateCopying() {
  Tape original;
  original.mark();
  original.moveRight();
  original.mark();
  Tape copied(original);
  Tape assigned;
  assigned = original;
  const bool equal = copied == original && assigned == original;

  copied.erase();
  assigned.moveLeft();
  const bool independent = original.isMarked(1) && original.headPosition() == 1 &&
                           copied != original && assigned != original;
  std::cout << "Copying preserves state: " << equal << '\n';
  std::cout << "Copies are independent: " << independent << '\n';
  return equal && independent;
}

bool demonstrateRepeatedMarkError() {
  Tape tape;
  tape.mark();
  const Tape original(tape);
  try {
    tape.mark();
  } catch (const std::runtime_error &) {
    const bool preserved = tape == original;
    std::cout << "Repeated mark rejected without state changes: " << preserved << '\n';
    return preserved;
  }
  return false;
}

bool demonstrateEmptyEraseError() {
  Tape tape;
  const Tape original(tape);
  try {
    tape.erase();
  } catch (const std::runtime_error &) {
    const bool preserved = tape == original;
    std::cout << "Empty cell erase rejected without state changes: " << preserved << '\n';
    return preserved;
  }
  return false;
}

bool demonstrateMovementBoundary(Tape::Position position, bool rightward) {
  Tape tape;
  tape.moveTo(position);
  const Tape original(tape);
  try {
    if (rightward) {
      tape.moveRight();
    } else {
      tape.moveLeft();
    }
  } catch (const std::overflow_error &) {
    const bool preserved = tape == original;
    std::cout << (rightward ? "Right" : "Left")
              << " boundary rejected without state changes: " << preserved << '\n';
    return preserved;
  }
  return false;
}
} // namespace

int main() {
  try {
    std::cout << std::boolalpha;
    const bool movement = demonstrateMovement();
    const bool editing = demonstrateEditing();
    const bool copying = demonstrateCopying();
    const bool repeatedMark = demonstrateRepeatedMarkError();
    const bool emptyErase = demonstrateEmptyEraseError();
    const bool leftBoundary = demonstrateMovementBoundary(
        std::numeric_limits<Tape::Position>::min(), false);
    const bool rightBoundary = demonstrateMovementBoundary(
        std::numeric_limits<Tape::Position>::max(), true);

    return movement && editing && copying && repeatedMark && emptyErase &&
                   leftBoundary && rightBoundary
               ? 0 : 1;
  } catch (const std::exception &error) {
    std::cerr << "Error: " << error.what() << '\n';
    return 1;
  }
}
