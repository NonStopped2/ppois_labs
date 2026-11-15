#pragma once

#include "Instruction.hpp"

#include <cstddef>
#include <iosfwd>
#include <map>

/** @brief Хранит команды по номерам; позволяет добавлять, заменять и удалять их.
 * Номера положительные и не повторяются. Переходы могут вести к ещё не добавленным
 * командам: выбранный переход проверит PostMachine при выполнении.
 * При удалении остальные команды не перенумеровываются.
 */
class Program {
  public:
    using Label = Instruction::Label; ///< Тип номера команды.
    /** @brief Создаёт пустую программу. */
    Program() = default;
    /** @brief Проверяет отсутствие команд. */
    bool empty() const;
    /** @brief Возвращает количество команд. */
    std::size_t size() const;
    /** @brief Проверяет наличие номера. */
    bool contains(Label label) const;
    /** @brief Возвращает наименьший номер.
     * @throws std::out_of_range Для пустой программы.
     */
    Label firstLabel() const;
    /** @brief Возвращает команду только для чтения.
     * Ссылка действительна до удаления этой команды или уничтожения программы.
     * @throws std::out_of_range Если номер отсутствует.
     */
    const Instruction& instructionAt(Label label) const;
    /** @brief Добавляет независимую копию команды.
     * @throws std::invalid_argument Если номер неположительный или занят.
     */
    void add(Label label, const Instruction& instruction);
    /** @brief Заменяет существующую команду.
     * @throws std::out_of_range Если номер отсутствует.
     */
    void replace(Label label, const Instruction& instruction);
    /** @brief Удаляет команду; возвращает false, если номер отсутствовал. */
    bool remove(Label label);
    /** @brief Сравнивает номера и содержимое всех команд. */
    bool operator==(const Program& other) const;
    /** @brief Отрицает равенство. */
    bool operator!=(const Program& other) const;
    /** @brief Пишет PROGRAM, количество и команды по возрастанию номера. */
    friend std::ostream& operator<<(std::ostream& output, const Program& program);
    /** @brief Читает программу; при неверных данных ставит failbit без изменения объекта.
     * Дублирующиеся номера и отрицательное количество недопустимы.
     */
    friend std::istream& operator>>(std::istream& input, Program& program);

  private:
    // Ключ — номер команды, значение — сама команда. map хранит их по возрастанию номера.
    std::map<Label, Instruction> instructions_;
};
