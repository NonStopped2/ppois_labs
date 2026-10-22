 #include "Tape.hpp"

   #include <limits>
   #include <stdexcept>

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
       return headPosition_ == other.headPosition_ &&
              markedCells_ == other.markedCells_;
   }

   bool Tape::operator!=(const Tape& other) const {
       return !(*this == other);
   }
