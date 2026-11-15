#include "Tape.hpp"
#include "TestSupport.hpp"

#include <limits>
#include <stdexcept>
#include <vector>

TEST_CASE("Tape starts empty and supports independent copying", "[tape]") {
    Tape tape;
    REQUIRE(tape.headPosition() == 0);
    REQUIRE_FALSE(tape.currentCellMarked());
    REQUIRE_FALSE(tape.isMarked(-100));
    tape.mark();
    Tape copied(tape);
    Tape assigned;
    assigned = tape;
    REQUIRE(copied == tape);
    REQUIRE(assigned == tape);
    copied.erase();
    assigned.moveRight();
    REQUIRE(copied != tape);
    REQUIRE(assigned != tape);
    REQUIRE(tape.currentCellMarked());
    REQUIRE(tape.headPosition() == 0);
}

TEST_CASE("Tape editing is idempotent and movement preserves cells", "[tape]") {
    Tape tape;
    tape.setMarked(-1000000000000LL, true);
    tape.setMarked(-1000000000000LL, true);
    tape.moveLeft();
    REQUIRE(tape.headPosition() == -1);
    tape.mark();
    tape.moveRight();
    REQUIRE_FALSE(tape.currentCellMarked());
    REQUIRE(tape.isMarked(-1));
    tape.moveTo(-1000000000000LL);
    REQUIRE(tape.currentCellMarked());
    tape.erase();
    tape.setMarked(-1, false);
    tape.setMarked(-1, false);
    REQUIRE_FALSE(tape.isMarked(-1));
    REQUIRE_FALSE(tape.currentCellMarked());
}

TEST_CASE("Tape rejects invalid actions and overflow without changes", "[tape]") {
    Tape tape;
    const Tape empty = tape;
    REQUIRE_THROWS_AS(tape.erase(), std::runtime_error);
    REQUIRE(tape == empty);
    tape.mark();
    const Tape marked = tape;
    REQUIRE_THROWS_AS(tape.mark(), std::runtime_error);
    REQUIRE(tape == marked);
    tape.moveTo(std::numeric_limits<Tape::Position>::min());
    const Tape left = tape;
    REQUIRE_THROWS_AS(tape.moveLeft(), std::overflow_error);
    REQUIRE(tape == left);
    tape.moveTo(std::numeric_limits<Tape::Position>::max());
    const Tape right = tape;
    REQUIRE_THROWS_AS(tape.moveRight(), std::overflow_error);
    REQUIRE(tape == right);
}

TEST_CASE("Tape streams preserve empty and extreme states", "[tape][streams]") {
    test_support::roundTrip(Tape{});
    Tape tape;
    tape.moveTo(std::numeric_limits<Tape::Position>::min());
    tape.mark();
    tape.setMarked(std::numeric_limits<Tape::Position>::max(), true);
    test_support::roundTrip(tape);
    tape.moveTo(std::numeric_limits<Tape::Position>::max());
    test_support::roundTrip(tape);
    std::ostringstream output;
    output << Tape{};
    REQUIRE(output.str() == "HEAD 0\nMARKS 0\n");
    std::istringstream input("HEAD -1 MARKS 2 5 -2 HEAD 0 MARKS 0");
    Tape second;
    input >> tape >> second;
    REQUIRE_FALSE(input.fail());
    REQUIRE(tape.headPosition() == -1);
    REQUIRE(tape.isMarked(-2));
    REQUIRE(tape.isMarked(5));
    REQUIRE(second == Tape{});
}

TEST_CASE("Tape rejects malformed input transactionally", "[tape][streams]") {
    Tape original;
    original.mark();
    const std::vector<std::string> invalid{"",
                                           "HEAD",
                                           "HEAD 0",
                                           "HEAD x MARKS 0",
                                           "HED 0 MARKS 0",
                                           "HEAD 0 MARK 0",
                                           "HEAD 0 MARKS -1",
                                           "HEAD 0 MARKS x",
                                           "HEAD 0 MARKS 1",
                                           "HEAD 0 MARKS 2 1 1",
                                           "HEAD 0 MARKS 2 3",
                                           "HEAD 0 MARKS 1 x",
                                           "HEAD 9223372036854775808 MARKS 0",
                                           "HEAD -9223372036854775809 MARKS 0",
                                           "HEAD 0 MARKS 9223372036854775808",
                                           "HEAD 0 MARKS 1 9223372036854775808"};
    for (const auto& text : invalid) {
        test_support::invalidInputPreserves(text, original);
    }
    std::istringstream throwing("HEAD 0 MARKS 2 1 1");
    throwing.exceptions(std::ios::failbit);
    Tape result = original;
    REQUIRE_THROWS_AS(throwing >> result, std::ios_base::failure);
    REQUIRE(result == original);
    std::ostringstream failed;
    failed.setstate(std::ios::badbit);
    failed << original;
    REQUIRE(failed.fail());
}
