#include "Instruction.hpp"

   #include <stdexcept>

   Instruction::Instruction(Operation operation,
                            Label next,
                            Label nextIfMarked)
       : operation_(operation),
         next_(next),
         nextIfMarked_(nextIfMarked) {
       if (!hasValidArguments()) {
           throw std::invalid_argument("Invalid instruction or jump targets");
       }
   }

   Operation Instruction::operation() const {
       return operation_;
   }

   Instruction::Label Instruction::next() const {
       return next_;
   }

   Instruction::Label Instruction::nextIfMarked() const {
       return nextIfMarked_;
   }

   bool Instruction::hasValidArguments() const {
       switch (operation_) {
       case Operation::Halt:
           return next_ == 0 && nextIfMarked_ == 0;

       case Operation::Branch:
           return next_ > 0 && nextIfMarked_ > 0;

       case Operation::Mark:
       case Operation::Erase:
       case Operation::Left:
       case Operation::Right:
           return next_ > 0 && nextIfMarked_ == 0;
       }

       return false;
   }

   bool Instruction::operator==(const Instruction& other) const {
       return operation_ == other.operation_ &&
              next_ == other.next_ &&
              nextIfMarked_ == other.nextIfMarked_;
   }

   bool Instruction::operator!=(const Instruction& other) const {
       return !(*this == other);
   }
