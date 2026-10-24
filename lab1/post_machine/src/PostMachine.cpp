#include "PostMachine.hpp"

   #include <stdexcept>

   PostMachine::PostMachine(const Program& program, const Tape& tape)
       : tape_(tape),
         program_(program),
         currentLabel_(program.empty() ? 0 : program.firstLabel()) {
   }

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

   PostMachine::Label PostMachine::nextLabelFor(
       const Instruction& instruction) const {
       Label next = instruction.next();

       if (instruction.operation() == Operation::Branch &&
           tape_.currentCellMarked()) {
           next = instruction.nextIfMarked();
       }

       if (!program_.contains(next)) {
           throw std::runtime_error("Jump target does not exist");
       }

       return next;
   }

   void PostMachine::applyOperation(Operation operation) {
       switch (operation) {
       case Operation::Mark:
           tape_.mark();
           break;
       case Operation::Erase:
           tape_.erase();
           break;
       case Operation::Left:
           tape_.moveLeft();
           break;
       case Operation::Right:
           tape_.moveRight();
           break;
       case Operation::Branch:
       case Operation::Halt:
           break;
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
       std::size_t executed = 0;

       while (!halted_ && executed < maxSteps) {
           step();
           ++executed;
       }

       if (!halted_) {
           throw std::runtime_error("Execution step limit reached");
       }

       return executed;
   }

   bool PostMachine::operator==(const PostMachine& other) const {
       return tape_ == other.tape_ &&
              program_ == other.program_ &&
              currentLabel_ == other.currentLabel_ &&
              halted_ == other.halted_;
   }

   bool PostMachine::operator!=(const PostMachine& other) const {
       return !(*this == other);
   }
