#include "Instruction.hpp"

   #include <array>
   #include <istream>
   #include <ostream>
   #include <stdexcept>
   #include <string>

   namespace {
      constexpr std::array<const char*, 6> kOperationNames{
          "MARK", "ERASE", "LEFT", "RIGHT", "IF", "HALT"
      };

      bool tryParseOperation(const std::string& name, Operation& operation) {
          for (std::size_t index = 0; index < kOperationNames.size(); ++index) {
              if (name == kOperationNames[index]) {
                  operation = static_cast<Operation>(index);
                  return true;
              }
          }

          return false;
      }
      }

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

   std::ostream& operator<<(std::ostream& output,
                               const Instruction& instruction) {
          const auto operation = instruction.operation();

          output << kOperationNames.at(static_cast<std::size_t>(operation));

          if (operation != Operation::Halt) {
              output << ' ' << instruction.next();
          }

          if (operation == Operation::Branch) {
              output << ' ' << instruction.nextIfMarked();
          }

          return output;
      }

      std::istream& operator>>(std::istream& input,
                                Instruction& instruction) {
           std::string name;
           Operation operation{};

           if (!(input >> name) || !tryParseOperation(name, operation)) {
               input.setstate(std::ios::failbit);
               return input;
           }

           Instruction::Label next{};
           Instruction::Label nextIfMarked{};

           if (operation != Operation::Halt) {
               input >> next;
           }

           if (operation == Operation::Branch) {
               input >> nextIfMarked;
           }

           if (!input) {
               return input;
           }

           try {
               instruction = Instruction(operation, next, nextIfMarked);
           } catch (const std::invalid_argument&) {
               input.setstate(std::ios::failbit);
           }

           return input;
       }
