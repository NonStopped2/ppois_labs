  #pragma once

   #include <cstdint>
   #include <set>

   /** @brief Лента машины Поста с кареткой и отмеченными клетками. */
   class Tape {
   public:
       using Position = std::int64_t;

       Tape() = default;

       Position headPosition() const;
       bool isMarked(Position position) const;
       bool currentCellMarked() const;

       /** @brief Устанавливает значение произвольной клетки при редактировании. */
       void setMarked(Position position, bool marked);

       /** @brief Перемещает каретку в заданную позицию при настройке ленты. */
       void moveTo(Position position);

       void moveLeft();
       void moveRight();

       /** @brief Ставит метку в текущую пустую клетку. */
       void mark();

       /** @brief Стирает метку из текущей отмеченной клетки. */
       void erase();

       bool operator==(const Tape& other) const;
       bool operator!=(const Tape& other) const;

   private:
       Position headPosition_ = 0;
       std::set<Position> markedCells_;
   };
