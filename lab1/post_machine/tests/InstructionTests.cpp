#include "Instruction.hpp"
#include "TestSupport.hpp"

#include <array>
#include <limits>
#include <stdexcept>
#include <vector>

TEST_CASE("Instruction validates every operation and jump combination", "[instruction]") {
    constexpr std::array operations{Operation::Mark,
                                    Operation::Erase,
                                    Operation::Left,
                                    Operation::Right,
                                    Operation::Branch,
                                    Operation::Halt,
                                    static_cast<Operation>(-1),
                                    static_cast<Operation>(6)};
    constexpr std::array<Instruction::Label, 6> labels{
        std::numeric_limits<Instruction::Label>::min(), -1, 0, 1, 2,
        std::numeric_limits<Instruction::Label>::max()};
    for (std::size_t index = 0; index < operations.size(); ++index) {
        for (const auto next : labels) {
            for (const auto marked : labels) {
                const bool valid = (index < 4 && next > 0 && marked == 0) ||
                                   (index == 4 && next > 0 && marked > 0) ||
                                   (index == 5 && next == 0 && marked == 0);
                if (valid) {
                    const Instruction command(operations[index], next, marked);
                    REQUIRE(command.operation() == operations[index]);
                    REQUIRE(command.next() == next);
                    REQUIRE(command.nextIfMarked() == marked);
                } else {
                    REQUIRE_THROWS_AS(Instruction(operations[index], next, marked),
                                      std::invalid_argument);
                }
            }
        }
    }
}

TEST_CASE("Instruction copying and equality include all fields", "[instruction]") {
    const Instruction stop;
    REQUIRE(stop.operation() == Operation::Halt);
    REQUIRE(stop.next() == 0);
    REQUIRE(stop.nextIfMarked() == 0);
    const Instruction original(Operation::Branch, 2, 5);
    Instruction copied(original);
    Instruction assigned;
    assigned = original;
    REQUIRE(copied == original);
    REQUIRE(assigned == original);
    copied = Instruction(Operation::Branch, 3, 5);
    assigned = Instruction(Operation::Branch, 2, 6);
    REQUIRE(copied != original);
    REQUIRE(assigned != original);
    REQUIRE(Instruction(Operation::Mark, 1) != Instruction(Operation::Erase, 1));
    REQUIRE_FALSE(original != Instruction(Operation::Branch, 2, 5));
}

TEST_CASE("Instruction streams encode all six operations", "[instruction][streams]") {
    const std::array<std::string, 6> examples{"MARK 4",  "ERASE 3", "LEFT 7",
                                              "RIGHT 2", "IF 2 5",  "HALT"};
    for (const auto& text : examples) {
        std::istringstream input(text);
        Instruction command;
        REQUIRE(static_cast<bool>(input >> command));
        std::ostringstream output;
        output << command;
        REQUIRE(output.str() == text);
        test_support::roundTrip(command);
    }
    std::istringstream chained("MARK 2 IF 3 4 HALT");
    Instruction first;
    Instruction second;
    Instruction third;
    chained >> first >> second >> third;
    REQUIRE_FALSE(chained.fail());
    REQUIRE(first == Instruction(Operation::Mark, 2));
    REQUIRE(second == Instruction(Operation::Branch, 3, 4));
    REQUIRE(third == Instruction{});
}

TEST_CASE("Instruction input errors preserve the previous command", "[instruction][streams]") {
    const Instruction original(Operation::Branch, 7, 9);
    const std::vector<std::string> invalid{"",
                                           "UNKNOWN",
                                           "mark 2",
                                           "MARK",
                                           "MARK x",
                                           "MARK 0",
                                           "MARK -1",
                                           "IF",
                                           "IF 2",
                                           "IF 2 x",
                                           "IF 0 3",
                                           "IF 2 -1",
                                           "MARK 9223372036854775808",
                                           "IF 1 -9223372036854775809"};
    for (const auto& text : invalid) {
        test_support::invalidInputPreserves(text, original);
    }
    Instruction result = original;
    std::istringstream throwing("IF 2 -1");
    throwing.exceptions(std::ios::failbit);
    REQUIRE_THROWS_AS(throwing >> result, std::ios_base::failure);
    REQUIRE(result == original);
    std::ostringstream failed;
    failed.setstate(std::ios::badbit);
    failed << original;
    REQUIRE(failed.fail());
}
