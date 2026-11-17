#include "Console.hpp"

#include <sstream>
#include <string>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

namespace {

std::string run(std::string_view commands) {
    std::istringstream input{std::string(commands)};
    std::ostringstream output;
    lab4::runConsole(input, output);
    return output.str();
}

} // namespace

TEST_CASE("Console shows both sorting algorithms and the sample task ordering", "[ui]") {
    const auto output = run("help\n\nsort cocktail 4 -2 4\nsort strand 7 1\n"
                            "tasks cocktail\ntasks strand\nsort strand\nquit\nshould be ignored\n");
    CHECK(output.find("sort <cocktail|strand>") != std::string::npos);
    CHECK(output.find("Before: 4 -2 4\nAfter: -2 4 4") != std::string::npos);
    CHECK(output.find("Before: 7 1\nAfter: 1 7") != std::string::npos);
    CHECK(output.find("After: Fix build (1) Write tests (2) Review docs (2) Ship release (3)") !=
          std::string::npos);
    CHECK(output.find("Before:\nAfter:\n") != std::string::npos);
    CHECK(output.find("Error:") == std::string::npos);
    CHECK(output.find("ignored") == std::string::npos);
}

TEST_CASE("Console edits the graph and shows all traversals and degrees", "[ui]") {
    const auto output =
        run("demo\nshow\nadd-vertex West station\nvertex 4\nhas-vertex 4\nhas-vertex 9\n"
            "has-arc 1 2\nhas-arc 3 1\nadd-arc 3 4\ncount\ndegree 2\narc-degree 1 2\n"
            "walk vertices\nwalk arcs reverse\nwalk incident 2\nwalk adjacent 2 reverse\n"
            "remove-arc 1 2\nremove-vertex 3\ncount\nclear\ncount\nquit\n");
    CHECK(output.find("Vertex 4") != std::string::npos);
    CHECK(output.find("lab4> West station") != std::string::npos);
    CHECK(output.find("lab4> yes\nlab4> no\nlab4> yes\nlab4> no") != std::string::npos);
    CHECK(output.find("Vertices: 4, arcs: 4") != std::string::npos);
    CHECK(output.find("In: 1, out: 2, total: 3") != std::string::npos);
    CHECK(output.find("Arc degree: 2") != std::string::npos);
    CHECK(output.find("vertices: Alpha Beta Gamma West station") != std::string::npos);
    CHECK(output.find("arcs: 3 -> 4 2 -> 3 2 -> 1 1 -> 2") != std::string::npos);
    CHECK(output.find("incident: 1 -> 2 2 -> 1 2 -> 3") != std::string::npos);
    CHECK(output.find("adjacent: Gamma Alpha") != std::string::npos);
    CHECK(output.find("Vertices: 3, arcs: 1") != std::string::npos);
    CHECK(output.find("Vertices: 0, arcs: 0") != std::string::npos);
    CHECK(output.find("Error:") == std::string::npos);
}

TEST_CASE("Console handles invalid input and keeps accepting commands until EOF", "[ui]") {
    const auto output =
        run("sort\nsort quick 4\nsort strand 10x\nsort cocktail 999999999999\n"
            "add-vertex\nvertex -1\nadd-arc 1\nclear extra\nwalk missing\n"
            "walk vertices sideways\nwalk arcs reverse extra\n"
            "demo\ndemo\nadd-arc 1 1\nremove-vertex 22\nunknown\nquit extra\ncount\n");
    for (const auto& expected :
         {"expected cocktail or strand", "invalid integer: 10x", "invalid integer: 999999999999",
          "expected a vertex name", "invalid vertex ID: -1", "expected vertex ID",
          "unexpected argument: extra", "unknown walk type: missing", "expected reverse",
          "demo requires an empty graph", "loops and duplicate arcs are forbidden",
          "unknown vertex ID", "unknown command: unknown"}) {
        CHECK(output.find(expected) != std::string::npos);
    }
    CHECK(output.find("Vertices: 3, arcs: 3") != std::string::npos);
}

TEST_CASE("Arc degree reports errors before writing a result", "[ui]") {
    const auto output = run("demo\narc-degree 1 3\narc-degree 1 77\narc-degree 1 2\nquit\n");
    CHECK(output.find("lab4> Error: arc does not exist\n"
                      "lab4> Error: unknown vertex ID\n"
                      "lab4> Arc degree: 2\n") != std::string::npos);
}
