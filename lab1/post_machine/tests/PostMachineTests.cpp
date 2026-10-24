#include "PostMachine.hpp"
#include "TestSupport.hpp"

#include <array>
#include <limits>
#include <map>
#include <random>
#include <set>
#include <stdexcept>
#include <vector>

namespace {
template <class Exception, class Action>
void rejectsWithoutChanges(PostMachine& machine, Action action) {
    const PostMachine before(machine);
    REQUIRE_THROWS_AS(action(), Exception);
    REQUIRE(machine == before);
}

struct RawRule {
    char operation;
    Program::Label next;
    Program::Label marked;
};

// Independent model: it does not use PostMachine::step or Tape actions.
struct Reference {
    std::map<Program::Label, RawRule> rules;
    std::set<Tape::Position> marks;
    Tape::Position head = 0;
    Program::Label current = 0;
    bool stopped = false;
};

enum class Outcome { Executed, Stopped, Error };

Outcome referenceStep(Reference& state) {
    if (state.stopped)
        return Outcome::Stopped;
    const RawRule rule = state.rules.at(state.current);
    const bool marked = state.marks.count(state.head) != 0;
    switch (rule.operation) {
    case 'M':
        if (marked)
            return Outcome::Error;
        state.marks.insert(state.head);
        state.current = rule.next;
        break;
    case 'E':
        if (!marked)
            return Outcome::Error;
        state.marks.erase(state.head);
        state.current = rule.next;
        break;
    case 'L':
        --state.head;
        state.current = rule.next;
        break;
    case 'R':
        ++state.head;
        state.current = rule.next;
        break;
    case '?': state.current = marked ? rule.marked : rule.next; break;
    case '!': state.stopped = true; break;
    default: throw std::logic_error("Invalid reference rule");
    }
    return Outcome::Executed;
}

void compareReference(const PostMachine& machine, const Reference& reference) {
    Tape expected;
    expected.moveTo(reference.head);
    for (const auto position : reference.marks)
        expected.setMarked(position, true);
    REQUIRE(machine.tape() == expected);
    REQUIRE(machine.currentLabel() == reference.current);
    REQUIRE(machine.isHalted() == reference.stopped);
}
} // namespace

TEST_CASE("Post machine executes a program stepwise and as a whole", "[machine]") {
    const Program program = test_support::program("PROGRAM 3 1 MARK 2 2 RIGHT 3 3 HALT");
    const Tape initial;
    PostMachine machine(program, initial);
    REQUIRE(machine.currentLabel() == 1);
    REQUIRE_FALSE(machine.isHalted());
    REQUIRE(machine.step());
    REQUIRE(machine.currentLabel() == 2);
    REQUIRE(machine.tape().isMarked(0));
    REQUIRE(machine.step());
    REQUIRE(machine.currentLabel() == 3);
    REQUIRE(machine.tape().headPosition() == 1);
    REQUIRE_FALSE(machine.isHalted());
    const PostMachine beforeHalt(machine);
    REQUIRE(machine.step());
    REQUIRE(machine.isHalted());
    REQUIRE(machine != beforeHalt);
    const PostMachine stopped(machine);
    REQUIRE_FALSE(machine.step());
    REQUIRE(machine == stopped);
    REQUIRE(machine.run(0) == 0);
    PostMachine completed(program, initial);
    REQUIRE(completed.run(3) == 3);
    REQUIRE(completed == machine);
    REQUIRE(initial == Tape{});
    REQUIRE(machine.program() == program);
}

TEST_CASE("Post machine executes all operations and both branch outcomes", "[machine]") {
    const Program program =
        test_support::program("PROGRAM 9 1 MARK 2 2 IF 3 4 3 HALT 4 RIGHT 5 5 IF 6 9 "
                              "6 LEFT 7 7 ERASE 8 8 HALT 9 HALT");
    Tape tape;
    tape.setMarked(-100, true);
    PostMachine machine(program, tape);
    REQUIRE(machine.step());
    const Tape marked = machine.tape();
    REQUIRE(machine.step());
    REQUIRE(machine.currentLabel() == 4);
    REQUIRE(machine.tape() == marked);
    REQUIRE(machine.step());
    const Tape moved = machine.tape();
    REQUIRE(machine.step());
    REQUIRE(machine.currentLabel() == 6);
    REQUIRE(machine.tape() == moved);
    REQUIRE(machine.run(3) == 3);
    REQUIRE(machine.tape() == tape);
    REQUIRE(machine.isHalted());
}

TEST_CASE("Post machine increments unary blocks including at negative coordinates", "[machine]") {
    const auto program = test_support::program("PROGRAM 4 1 IF 3 2 2 RIGHT 1 3 MARK 4 4 HALT");
    for (int length = 0; length <= 20; ++length) {
        Tape initial;
        initial.moveTo(-10);
        initial.setMarked(1000, true);
        for (int index = 0; index < length; ++index)
            initial.setMarked(-10 + index, true);
        Tape expected = initial;
        expected.moveTo(-10 + length);
        expected.mark();
        PostMachine machine(program, initial);
        const auto count = static_cast<std::size_t>(2 * length + 3);
        REQUIRE(machine.run(count) == count);
        REQUIRE(machine.tape() == expected);
        REQUIRE(machine.isHalted());
        REQUIRE_FALSE(initial.isMarked(-10 + length));
    }
}

TEST_CASE("Post machine errors do not execute a partial step", "[machine]") {
    PostMachine empty;
    rejectsWithoutChanges<std::runtime_error>(empty, [&] { empty.step(); });
    rejectsWithoutChanges<std::runtime_error>(empty, [&] { empty.run(1); });
    Tape marked;
    marked.mark();
    PostMachine repeated(test_support::program("PROGRAM 2 1 MARK 2 2 HALT"), marked);
    rejectsWithoutChanges<std::runtime_error>(repeated, [&] { repeated.step(); });
    PostMachine erased(test_support::program("PROGRAM 2 1 ERASE 2 2 HALT"));
    rejectsWithoutChanges<std::runtime_error>(erased, [&] { erased.step(); });
    for (const std::string action : {"MARK", "LEFT", "RIGHT"}) {
        PostMachine missing(test_support::program("PROGRAM 1 1 " + action + " 99"));
        rejectsWithoutChanges<std::runtime_error>(missing, [&] { missing.step(); });
    }
    const auto branch = test_support::program("PROGRAM 2 1 IF 2 99 2 HALT");
    PostMachine blank(branch);
    REQUIRE(blank.run(2) == 2);
    PostMachine badMarked(branch, marked);
    rejectsWithoutChanges<std::runtime_error>(badMarked, [&] { badMarked.step(); });
    Tape edge;
    edge.moveTo(std::numeric_limits<Tape::Position>::min());
    PostMachine left(test_support::program("PROGRAM 2 1 LEFT 2 2 HALT"), edge);
    rejectsWithoutChanges<std::overflow_error>(left, [&] { left.step(); });
    edge.moveTo(std::numeric_limits<Tape::Position>::max());
    PostMachine right(test_support::program("PROGRAM 2 1 RIGHT 2 2 HALT"), edge);
    rejectsWithoutChanges<std::overflow_error>(right, [&] { right.step(); });
}

TEST_CASE("Post machine limits retain progress and copies are independent", "[machine]") {
    PostMachine machine(test_support::program("PROGRAM 1 1 RIGHT 1"));
    rejectsWithoutChanges<std::runtime_error>(machine, [&] { machine.run(0); });
    REQUIRE_THROWS_AS(machine.run(5), std::runtime_error);
    REQUIRE(machine.tape().headPosition() == 5);
    PostMachine copied(machine);
    PostMachine assigned;
    assigned = machine;
    REQUIRE(assigned == machine);
    REQUIRE(copied == machine);
    REQUIRE(copied.step());
    REQUIRE(copied != machine);
    REQUIRE(machine.tape().headPosition() == 5);
    REQUIRE_THROWS_AS(machine.run(3), std::runtime_error);
    REQUIRE(machine.tape().headPosition() == 8);
    REQUIRE(machine != PostMachine{});
    Tape different;
    different.mark();
    REQUIRE(PostMachine(machine.program(), different) != PostMachine(machine.program()));
}

TEST_CASE("Post machine journals each completed rule including HALT", "[machine][logging]") {
    PostMachine machine(test_support::program("PROGRAM 3 1 MARK 2 2 RIGHT 3 3 HALT"));
    std::ostringstream log;
    REQUIRE(machine.run(3, log) == 3);
    REQUIRE(log.str() == "EXECUTED 1\nHEAD 0\nMARKS 1 0\nCURRENT 2\nHALTED 0\n\n"
                         "EXECUTED 2\nHEAD 1\nMARKS 1 0\nCURRENT 3\nHALTED 0\n\n"
                         "EXECUTED 3\nHEAD 1\nMARKS 1 0\nCURRENT 3\nHALTED 1\n\n");
    REQUIRE(machine.run(1, log) == 0);
    PostMachine invalid(test_support::program("PROGRAM 1 1 MARK 99"));
    std::ostringstream empty;
    REQUIRE_THROWS_AS(invalid.run(1, empty), std::runtime_error);
    REQUIRE(empty.str().empty());
    PostMachine failure(test_support::program("PROGRAM 1 1 HALT"));
    std::ostringstream broken;
    broken.setstate(std::ios::badbit);
    REQUIRE_THROWS_AS(failure.run(1, broken), std::runtime_error);
    REQUIRE(failure.isHalted());
}

TEST_CASE("Post machine streams preserve paused and stopped states", "[machine][streams]") {
    test_support::roundTrip(PostMachine{});
    PostMachine machine(test_support::program("PROGRAM 3 1 MARK 2 2 RIGHT 3 3 HALT"));
    test_support::roundTrip(machine);
    REQUIRE(machine.step());
    test_support::roundTrip(machine);
    REQUIRE(machine.run(2) == 2);
    test_support::roundTrip(machine);
    std::stringstream chained;
    chained << machine << PostMachine{};
    PostMachine first;
    PostMachine second(machine);
    chained >> first >> second;
    REQUIRE(first == machine);
    REQUIRE(second == PostMachine{});
    std::ostringstream alpha;
    alpha << std::boolalpha << machine;
    REQUIRE(alpha.str().find("HALTED 1") != std::string::npos);
}

TEST_CASE("Post machine rejects invalid control state transactionally", "[machine][streams]") {
    const PostMachine original(test_support::program("PROGRAM 1 1 HALT"));
    const std::string prefix = "HEAD 0 MARKS 0 PROGRAM 1 1 HALT ";
    for (const std::string tail :
         {"", "CURRENT", "CURRENT 1 HALTED", "CURRENT 1 HALTED 2", "CURRENT 1 HALTED -1",
          "CURRENT 2 HALTED 0", "PC 1 HALTED 0", "CURRENT 1 STOP 0"}) {
        test_support::invalidInputPreserves(prefix + tail, original);
    }
    for (const std::string text :
         {"HEAD 0 MARKS 0 PROGRAM 0 CURRENT 0 HALTED 1",
          "HEAD 0 MARKS 0 PROGRAM 0 CURRENT 1 HALTED 0",
          "HEAD 0 MARKS 0 PROGRAM 1 1 RIGHT 1 CURRENT 1 HALTED 1", "HEAD x"}) {
        test_support::invalidInputPreserves(text, original);
    }
    PostMachine result = original;
    std::istringstream throwing(prefix + "CURRENT 2 HALTED 0");
    throwing.exceptions(std::ios::failbit);
    REQUIRE_THROWS_AS(throwing >> result, std::ios_base::failure);
    REQUIRE(result == original);
}

TEST_CASE("Post machine matches an independent interpreter on 1000 programs",
          "[machine][reference]") {
    std::mt19937 generator(0xC0FFEE);
    auto choose = [&](int low, int high) {
        return std::uniform_int_distribution<int>(low, high)(generator);
    };
    constexpr std::array operations{Operation::Mark,  Operation::Erase,  Operation::Left,
                                    Operation::Right, Operation::Branch, Operation::Halt};
    constexpr std::array codes{'M', 'E', 'L', 'R', '?', '!'};
    for (int sample = 0; sample < 1000; ++sample) {
        INFO(sample);
        const int count = choose(1, 10);
        Reference reference;
        Program program;
        for (int index = count - 1; index >= 0; --index) {
            const auto label = static_cast<Program::Label>(index * 3 + 2);
            const int operation = choose(0, 5);
            const Program::Label next = operation == 5 ? 0 : choose(0, count - 1) * 3 + 2;
            const Program::Label marked = operation == 4 ? choose(0, count - 1) * 3 + 2 : 0;
            program.add(label, Instruction(operations[operation], next, marked));
            reference.rules.emplace(label, RawRule{codes[operation], next, marked});
        }
        reference.current = reference.rules.begin()->first;
        reference.head = choose(-3, 3);
        for (int position = -5; position <= 5; ++position) {
            if (choose(0, 1))
                reference.marks.insert(position);
        }
        reference.marks.insert(-1000000000000LL);
        reference.marks.insert(1000000000000LL);
        Tape tape;
        tape.moveTo(reference.head);
        for (const auto position : reference.marks)
            tape.setMarked(position, true);
        PostMachine machine(program, tape);
        compareReference(machine, reference);
        for (int step = 0; step < 64; ++step) {
            const PostMachine before(machine);
            const Outcome outcome = referenceStep(reference);
            bool error = false;
            bool executed = false;
            try {
                executed = machine.step();
            } catch (const std::runtime_error&) {
                error = true;
            }
            REQUIRE(error == (outcome == Outcome::Error));
            REQUIRE(executed == (outcome == Outcome::Executed));
            if (error)
                REQUIRE(machine == before);
            compareReference(machine, reference);
            REQUIRE(machine.program() == program);
            if (outcome != Outcome::Executed)
                break;
        }
    }
}
