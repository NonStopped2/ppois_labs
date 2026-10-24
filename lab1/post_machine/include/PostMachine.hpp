#pragma once

   #include "Program.hpp"
   #include "Tape.hpp"

   #include <cstddef>

   /** @brief Исполнитель программы машины Поста. */
   class PostMachine {
   public:
       using Label = Program::Label;

       PostMachine() = default;
       PostMachine(const Program& program, const Tape& tape = Tape{});

       const Tape& tape() const;
       const Program& program() const;
       Label currentLabel() const;
       bool isHalted() const;

       /** @brief Выполняет команду; возвращает false, если машина уже остановлена. */
       bool step();

       /** @brief Выполняет программу с ограничением количества команд. */
       std::size_t run(std::size_t maxSteps);

       bool operator==(const PostMachine& other) const;
       bool operator!=(const PostMachine& other) const;

   private:
       Label nextLabelFor(const Instruction& instruction) const;
       void applyOperation(Operation operation);

       Tape tape_;
       Program program_;
       Label currentLabel_ = 0;
       bool halted_ = false;
   };
