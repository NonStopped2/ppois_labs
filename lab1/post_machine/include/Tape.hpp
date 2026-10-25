#pragma once

#include <cstdint>
#include <iosfwd>
#include <set>

/** @brief Лента машины: хранит метки и положение каретки.
 * Запоминаем только координаты отмеченных клеток, остальные считаем пустыми.
 * Каретка ходит в обе стороны в пределах int64_t. Копии ленты независимы.
 */
class Tape {
  public:
    using Position = std::int64_t; ///< Знаковая координата клетки.
    /** @brief Создаёт пустую ленту с кареткой в нуле. */
    Tape() = default;
    /** @brief Возвращает положение каретки. */
    Position headPosition() const;
    /** @brief Проверяет наличие метки по произвольной координате. */
    bool isMarked(Position position) const;
    /** @brief Проверяет клетку под кареткой. */
    bool currentCellMarked() const;
    /** @brief Редактирует клетку; повторная установка значения допустима. */
    void setMarked(Position position, bool marked);
    /** @brief Устанавливает каретку в произвольную допустимую координату. */
    void moveTo(Position position);
    /** @brief Сдвигает каретку влево.
     * @throws std::overflow_error На минимальной координате, без изменения ленты.
     */
    void moveLeft();
    /** @brief Сдвигает каретку вправо.
     * @throws std::overflow_error На максимальной координате, без изменения ленты.
     */
    void moveRight();
    /** @brief Ставит метку под кареткой.
     * @throws std::runtime_error Если клетка уже отмечена, без изменения ленты.
     */
    void mark();
    /** @brief Стирает метку под кареткой.
     * @throws std::runtime_error Если клетка пуста, без изменения ленты.
     */
    void erase();
    /** @brief Сравнивает все метки и положение каретки. */
    bool operator==(const Tape& other) const;
    /** @brief Отрицает равенство. */
    bool operator!=(const Tape& other) const;
    /** @brief Пишет HEAD, координату, MARKS, количество и координаты меток. */
    friend std::ostream& operator<<(std::ostream& output, const Tape& tape);
    /** @brief Читает ленту; дубликаты координат недопустимы.
     * При некорректных данных устанавливает failbit, не изменяя исходную ленту.
     * Допускает последующие объекты в том же потоке.
     */
    friend std::istream& operator>>(std::istream& input, Tape& tape);

  private:
    Position headPosition_ = 0;
    // set не допускает повторов и сам управляет памятью для координат.
    std::set<Position> markedCells_;
};
