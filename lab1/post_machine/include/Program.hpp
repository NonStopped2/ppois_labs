#pragma once

   #include "Instruction.hpp"

   #include <cstddef>
   #include <iosfwd>
   #include <map>

   /** @brief Программа машины Поста: команды с положительными номерами. */
   class Program {
   public:
       using Label = Instruction::Label;

       Program() = default;

       bool empty() const;
       std::size_t size() const;
       bool contains(Label label) const;

       Label firstLabel() const;
       const Instruction& instructionAt(Label label) const;

       void add(Label label, const Instruction& instruction);
       void replace(Label label, const Instruction& instruction);
       bool remove(Label label);

       bool operator==(const Program& other) const;
       bool operator!=(const Program& other) const;

       /** @brief Записывает количество команд и команды с номерами. */
       friend std::ostream& operator<<(std::ostream& output, const Program& program);

       /** @brief Загружает программу; при некорректном вводе сохраняет исходную. */
       friend std::istream& operator>>(std::istream& input, Program& program);

   private:
       std::map<Label, Instruction> instructions_;
   };
