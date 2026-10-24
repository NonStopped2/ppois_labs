#include "Program.hpp"
#include "TestSupport.hpp"

#include <limits>
#include <stdexcept>
#include <vector>

TEST_CASE("Program manages stable positive labels", "[program]") {
    Program program;
    REQUIRE(program.empty());
    REQUIRE(program.size() == 0);
    REQUIRE_FALSE(program.contains(1));
    REQUIRE_THROWS_AS(program.firstLabel(), std::out_of_range);
    REQUIRE_THROWS_AS(program.instructionAt(1), std::out_of_range);
    REQUIRE_FALSE(program.remove(1));
    REQUIRE_THROWS_AS(program.add(0, Instruction{}), std::invalid_argument);
    REQUIRE_THROWS_AS(program.add(-1, Instruction{}), std::invalid_argument);
    program.add(10, Instruction{});
    program.add(2, Instruction(Operation::Right, 10));
    REQUIRE(program.firstLabel() == 2);
    REQUIRE(program.size() == 2);
    const Program before = program;
    REQUIRE_THROWS_AS(program.add(2, Instruction{}), std::invalid_argument);
    REQUIRE(program == before);
    REQUIRE_THROWS_AS(program.replace(99, Instruction{}), std::out_of_range);
    REQUIRE(program == before);
    program.replace(2, Instruction(Operation::Left, 10));
    REQUIRE(program.instructionAt(2) == Instruction(Operation::Left, 10));
    REQUIRE(program.remove(2));
    REQUIRE_FALSE(program.remove(2));
    REQUIRE(program.firstLabel() == 10);
    REQUIRE(program.contains(10));
    REQUIRE(program.remove(10));
    REQUIRE(program.empty());
}

TEST_CASE("Program copying and equality preserve labels and commands", "[program]") {
    const Program original = test_support::program("PROGRAM 2 1 MARK 3 3 HALT");
    Program copied(original);
    Program assigned;
    assigned = original;
    REQUIRE(copied == original);
    REQUIRE(assigned == original);
    copied.replace(1, Instruction(Operation::Right, 3));
    assigned.remove(3);
    REQUIRE(copied != original);
    REQUIRE(assigned != original);
    REQUIRE(original.size() == 2);
    REQUIRE(original.instructionAt(1).operation() == Operation::Mark);
    REQUIRE(test_support::program("PROGRAM 1 1 HALT") != test_support::program("PROGRAM 1 2 HALT"));
}

TEST_CASE("Program streams order labels and replace previous contents", "[program][streams]") {
    test_support::roundTrip(Program{});
    const Program program = test_support::program("PROGRAM 2 10 HALT 2 RIGHT 10");
    std::ostringstream output;
    output << program;
    REQUIRE(output.str() == "PROGRAM 2\n2 RIGHT 10\n10 HALT\n");
    test_support::roundTrip(program);
    Program result = program;
    std::istringstream chained("PROGRAM 0 PROGRAM 1 9223372036854775807 HALT NEXT");
    Program second;
    chained >> result >> second;
    std::string next;
    chained >> next;
    REQUIRE(result.empty());
    REQUIRE(second.firstLabel() == std::numeric_limits<Program::Label>::max());
    REQUIRE(next == "NEXT");
}

TEST_CASE("Program malformed input is transactional", "[program][streams]") {
    const Program original = test_support::program("PROGRAM 1 7 HALT");
    const std::vector<std::string> invalid{"",
                                           "PROGRAM",
                                           "PROG 0",
                                           "PROGRAM -1",
                                           "PROGRAM x",
                                           "PROGRAM 9223372036854775808",
                                           "PROGRAM 1",
                                           "PROGRAM 1 1",
                                           "PROGRAM 1 0 HALT",
                                           "PROGRAM 1 -1 HALT",
                                           "PROGRAM 2 1 HALT 1 MARK 2",
                                           "PROGRAM 1 1 MARK 0",
                                           "PROGRAM 1 1 IF 2",
                                           "PROGRAM 1 x HALT",
                                           "PROGRAM 1 9223372036854775808 HALT"};
    for (const auto& text : invalid) {
        test_support::invalidInputPreserves(text, original);
    }
    Program result = original;
    std::istringstream throwing("PROGRAM 2 1 HALT 1 HALT");
    throwing.exceptions(std::ios::failbit);
    REQUIRE_THROWS_AS(throwing >> result, std::ios_base::failure);
    REQUIRE(result == original);
    std::ostringstream failed;
    failed.setstate(std::ios::badbit);
    failed << original;
    REQUIRE(failed.fail());
}
