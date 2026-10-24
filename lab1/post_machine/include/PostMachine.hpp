#pragma once

#include "Program.hpp"
#include "Tape.hpp"

#include <cstddef>
#include <iosfwd>

/** @brief Исполнитель двоичной машины Поста с собственной программой и лентой.
 *
 * Начальная команда — команда с наименьшим номером. Копирование машины создаёт
 * независимую копию её состояния. Ошибочный шаг не изменяет состояние;
 * успешные шаги перед ошибкой или достижением лимита не откатываются.
 */
class PostMachine {
  public:
    using Label = Program::Label; ///< Номер команды.

    /** @brief Создаёт пустую, ещё не настроенную машину. */
    PostMachine() = default;
    /** @brief Копирует программу и ленту, устанавливает начальную команду. */
    PostMachine(const Program& program, const Tape& tape = Tape{});

    /** @brief Предоставляет рабочую ленту только для чтения. */
    const Tape& tape() const;
    /** @brief Предоставляет программу только для чтения. */
    const Program& program() const;
    /** @brief Возвращает номер следующей команды, либо 0 для пустой машины. */
    Label currentLabel() const;
    /** @brief Проверяет, выполнена ли команда HALT. */
    bool isHalted() const;

    /** @brief Выполняет одну команду, включая HALT.
     * @return false Только если машина уже остановлена.
     * @throws std::runtime_error При пустой программе, недопустимом действии
     * или отсутствующем выбранном переходе.
     * @throws std::overflow_error При выходе каретки за диапазон координат.
     */
    bool step();

    /** @brief Выполняет программу и возвращает число команд, включая HALT.
     * @throws std::runtime_error При ошибке выполнения или достижении лимита.
     * Успешные шаги сохраняются, даже если выполнение прервано исключением.
     */
    std::size_t run(std::size_t maxSteps);
    /** @brief Дополнительно записывает состояние после каждой успешной команды.
     * @throws std::runtime_error Также при ошибке записи журнала. Уже выполненный
     * шаг в этом случае не откатывается. Поток принадлежит вызывающему коду.
     */
    std::size_t run(std::size_t maxSteps, std::ostream& log);

    /** @brief Сравнивает ленту, программу, текущую команду и признак остановки. */
    bool operator==(const PostMachine& other) const;
    /** @brief Отрицает равенство состояний. */
    bool operator!=(const PostMachine& other) const;

    /** @brief Сохраняет ленту, программу и состояние исполнения. */
    friend std::ostream& operator<<(std::ostream& output, const PostMachine& machine);
    /** @brief Восстанавливает состояние; при неверном формате устанавливает
     * failbit, не изменяя исходную машину. HALTED допустим только для HALT.
     */
    friend std::istream& operator>>(std::istream& input, PostMachine& machine);

  private:
    Label nextLabelFor(const Instruction& instruction) const;
    void applyOperation(Operation operation);
    std::size_t runImpl(std::size_t maxSteps, std::ostream* log);
    void writeLog(std::ostream& log, Label executedLabel) const;

    Tape tape_;
    Program program_;
    Label currentLabel_ = 0;
    bool halted_ = false;
};
