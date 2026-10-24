#pragma once

#include <cstdint>
#include <iosfwd>

/** @brief Шесть действий машины Поста. Порядок соответствует таблице текстовых имён. */
enum class Operation {
    Mark,   ///< Поставить метку в пустую клетку.
    Erase,  ///< Стереть существующую метку.
    Left,   ///< Перейти на одну клетку влево.
    Right,  ///< Перейти на одну клетку вправо.
    Branch, ///< Выбрать переход по наличию метки.
    Halt    ///< Остановиться.
};

/** @brief Неизменяемое через публичный интерфейс описание одной команды. */
class Instruction {
  public:
    using Label = std::int64_t; ///< Положительный номер перехода, 0 — нет перехода.
    /** @brief Создаёт и проверяет команду; по умолчанию HALT.
     * @throws std::invalid_argument Для неизвестной операции или неверных переходов.
     * Обычным действиям нужен next > 0 и nextIfMarked == 0; Branch — оба > 0;
     * Halt — оба 0. Наличие номеров в программе здесь не проверяется.
     */
    Instruction(Operation operation = Operation::Halt, Label next = 0, Label nextIfMarked = 0);
    /** @brief Возвращает действие команды. */
    Operation operation() const;
    /** @brief Возвращает обычный переход, для Branch — переход при пустой клетке. */
    Label next() const;
    /** @brief Возвращает переход Branch при отмеченной клетке, иначе 0. */
    Label nextIfMarked() const;
    /** @brief Сравнивает действие и оба перехода. */
    bool operator==(const Instruction& other) const;
    /** @brief Отрицает равенство. */
    bool operator!=(const Instruction& other) const;

  private:
    bool hasValidArguments() const;
    Operation operation_;
    Label next_;
    Label nextIfMarked_;
};

/** @brief Пишет MARK n, ERASE n, LEFT n, RIGHT n, IF n m либо HALT, без перевода строки. */
std::ostream& operator<<(std::ostream& output, const Instruction& instruction);
/** @brief Читает одну команду; при неверных данных ставит failbit и сохраняет объект.
 * Читает только необходимые аргументы; проверку хвоста целого файла выполняет CLI.
 */
std::istream& operator>>(std::istream& input, Instruction& instruction);
