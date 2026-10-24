#pragma once

   #include <cstdint>
   #include <iosfwd>

   /** @brief Действие, выполняемое командой машины Поста. */
   enum class Operation {
       Mark,
       Erase,
       Left,
       Right,
       Branch,
       Halt
   };

   /** @brief Команда машины Поста с номерами переходов. */
   class Instruction {
   public:
       using Label = std::int64_t;

       Instruction(Operation operation = Operation::Halt,
                   Label next = 0,
                   Label nextIfMarked = 0);

       Operation operation() const;
       Label next() const;
       Label nextIfMarked() const;

       bool operator==(const Instruction& other) const;
       bool operator!=(const Instruction& other) const;

   private:
       bool hasValidArguments() const;

       Operation operation_;
       Label next_;
       Label nextIfMarked_;
   };

std::ostream& operator<<(std::ostream& output,
                               const Instruction& instruction);

std::istream& operator>>(std::istream& input,
                               Instruction& instruction);
