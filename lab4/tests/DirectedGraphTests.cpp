#include "lab4/DirectedGraph.hpp"

#include <concepts>
#include <iterator>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

namespace {

using Graph = lab4::DirectedGraph<std::string>;
using Edge = Graph::edge_type;

static_assert(std::bidirectional_iterator<Graph::vertex_iterator>);
static_assert(std::bidirectional_iterator<Graph::const_vertex_iterator>);
static_assert(std::bidirectional_iterator<Graph::edge_iterator>);
static_assert(std::bidirectional_iterator<Graph::const_edge_iterator>);
static_assert(std::bidirectional_iterator<Graph::incident_iterator>);
static_assert(std::bidirectional_iterator<Graph::const_incident_iterator>);
static_assert(std::bidirectional_iterator<Graph::adjacent_iterator>);
static_assert(std::bidirectional_iterator<Graph::const_adjacent_iterator>);
static_assert(std::same_as<std::iter_reference_t<Graph::vertex_iterator>, std::string&>);
static_assert(
    std::same_as<std::iter_reference_t<Graph::const_vertex_iterator>, const std::string&>);
static_assert(
    std::same_as<std::iter_reference_t<Graph::const_adjacent_iterator>, const std::string&>);

auto collect(std::ranges::common_range auto&& range) {
    using Value = std::ranges::range_value_t<decltype(range)>;
    return std::vector<Value>(range.begin(), range.end());
}

void checkWalk(auto range, const auto& expected) {
    CHECK(collect(range) == expected);
    CHECK(collect(std::views::reverse(range)) ==
          decltype(collect(range))(expected.rbegin(), expected.rend()));
    auto last = range.end();
    const auto pastEnd = last--;
    CHECK(pastEnd == range.end());
    CHECK(*last == expected.back());
    CHECK(last++ != range.end());
    CHECK(last == range.end());
}

struct Sample {
    Graph graph;
    Graph::vertex_id alpha = graph.addVertex("alpha");
    Graph::vertex_id beta = graph.addVertex("beta");
    Graph::vertex_id gamma = graph.addVertex("gamma");
    Graph::vertex_id isolated = graph.addVertex("isolated");

    Sample() {
        graph.addEdge(alpha, beta);
        graph.addEdge(beta, alpha);
        graph.addEdge(beta, gamma);
        graph.addEdge(gamma, alpha);
    }
};

struct ThrowingValue {
    static inline bool throwOnCopy = false;
    int number;

    explicit ThrowingValue(int initial) : number(initial) {}
    ThrowingValue(const ThrowingValue& other) : number(other.number) {
        if (throwOnCopy) {
            throw std::runtime_error("copy failed");
        }
    }
    ThrowingValue(ThrowingValue&&) noexcept = default;
    ThrowingValue& operator=(const ThrowingValue&) = default;
    ThrowingValue& operator=(ThrowingValue&&) noexcept = default;
};

} // namespace

TEST_CASE("Empty graph and isolated vertex have empty traversal ranges", "[core]") {
    lab4::DirectedGraph<int> graph;
    CHECK(graph.empty());
    CHECK(graph.vertexCount() == 0);
    CHECK(graph.edgeCount() == 0);
    CHECK(graph.begin() == graph.end());
    CHECK(graph.edges().empty());
    CHECK(std::views::reverse(graph.edges()).empty());
    const auto first = graph.addVertex(8);
    CHECK_FALSE(graph.empty());
    CHECK(graph.containsVertex(first));
    CHECK_FALSE(graph.containsVertex(first + 1));
    CHECK(graph.degree(first) == 0);
    CHECK(graph.incidentEdges(first).empty());
    CHECK(graph.adjacentVertices(first).empty());
    graph.removeVertex(first);
    CHECK(graph.empty());
    const auto second = graph.addVertex(8);
    CHECK(second != first);
    graph.clear();
    CHECK(graph.empty());
    CHECK(graph.edgeCount() == 0);
}

TEST_CASE("Arc direction, degrees and matrix resizing preserve surviving arcs", "[core]") {
    Sample sample;
    auto& [graph, alpha, beta, gamma, isolated] = sample;
    CHECK(graph.vertexCount() == 4);
    CHECK(graph.edgeCount() == 4);
    CHECK(graph.containsEdge(alpha, beta));
    CHECK_FALSE(graph.containsEdge(alpha, gamma));
    CHECK(graph.containsEdge(gamma, alpha));
    CHECK(graph.inDegree(alpha) == 2);
    CHECK(graph.outDegree(alpha) == 1);
    CHECK(graph.degree(alpha) == 3);
    CHECK(graph.edgeDegree(alpha, beta) == 3);
    CHECK(graph.edgeDegree(gamma, alpha) == 3);
    graph.removeVertex(beta);
    CHECK(graph.vertexCount() == 3);
    CHECK(graph.edgeCount() == 1);
    CHECK(graph.containsEdge(gamma, alpha));
    CHECK_FALSE(graph.containsEdge(alpha, beta));
    CHECK(graph.value(isolated) == "isolated");
    const auto added = graph.addVertex("new");
    CHECK(graph.containsEdge(gamma, alpha));
    graph.addEdge(added, alpha);
    graph.removeEdge(gamma, alpha);
    CHECK(graph.edgeCount() == 1);
    CHECK(graph.containsEdge(added, alpha));
    graph.clear();
    CHECK(graph.empty());
    CHECK(graph.edgeCount() == 0);
}

TEST_CASE("Graph copies are independent and all relations compare values followed by arcs",
          "[core]") {
    Sample sample;
    auto& [graph, alpha, beta, gamma, isolated] = sample;
    Graph copy(graph);
    CHECK(copy == graph);
    CHECK(copy <= graph);
    CHECK(copy >= graph);
    copy.value(alpha) = "a different value";
    CHECK(copy != graph);
    CHECK(copy < graph);
    CHECK(graph > copy);
    CHECK(std::as_const(graph).value(alpha) == "alpha");
    copy = graph;
    auto& sameCopy = copy;
    copy = sameCopy;
    CHECK(copy == graph);
    copy.removeEdge(gamma, alpha);
    CHECK(copy < graph);
    CHECK(graph.edgeCount() == 4);

    Graph shiftedIds;
    const auto removed = shiftedIds.addVertex("removed");
    shiftedIds.removeVertex(removed);
    const auto shiftedAlpha = shiftedIds.addVertex("alpha");
    shiftedIds.addVertex("beta");
    Graph sameValues;
    const auto expectedAlpha = sameValues.addVertex("alpha");
    sameValues.addVertex("beta");
    CHECK(shiftedAlpha != expectedAlpha);
    CHECK(shiftedIds == sameValues);
    CHECK(sameValues < graph);
}

TEST_CASE("Four graph traversal ranges support const, mutable and reverse iteration", "[core]") {
    Sample sample;
    auto& [graph, alpha, beta, gamma, isolated] = sample;
    const auto& immutable = graph;
    const std::vector<std::string> vertices = {"alpha", "beta", "gamma", "isolated"};
    const std::vector<Edge> arcs = {{alpha, beta}, {beta, alpha}, {beta, gamma}, {gamma, alpha}};
    const std::vector<Edge> incident = {{alpha, beta}, {beta, alpha}, {beta, gamma}};
    const std::vector<std::string> adjacent = {"alpha", "gamma"};

    checkWalk(graph.vertices(), vertices);
    checkWalk(immutable.vertices(), vertices);
    checkWalk(graph.edges(), arcs);
    checkWalk(immutable.edges(), arcs);
    checkWalk(graph.incidentEdges(beta), incident);
    checkWalk(immutable.incidentEdges(beta), incident);
    checkWalk(graph.adjacentVertices(beta), adjacent);
    checkWalk(immutable.adjacentVertices(beta), adjacent);
    CHECK(immutable.incidentEdges(isolated).empty());
    CHECK(std::views::reverse(immutable.adjacentVertices(isolated)).empty());

    auto first = graph.begin();
    CHECK(graph.vertexId(first) == alpha);
    *first = "alpha updated";
    *graph.adjacentVertices(beta).begin() = "alpha twice";
    CHECK(graph.value(alpha) == "alpha twice");
    CHECK(graph.containsEdge(beta, alpha));
    CHECK(immutable.begin() == first);
    CHECK(std::ranges::distance(immutable) == 4);
}

TEST_CASE("Erasing by iterator rejects foreign, end and invalidated iterators", "[core]") {
    Sample sample;
    auto& [graph, alpha, beta, gamma, isolated] = sample;
    Graph other(graph);
    CHECK_THROWS_AS(graph.removeVertex(graph.end()), std::invalid_argument);
    CHECK_THROWS_AS(graph.removeEdge(graph.edges().end()), std::invalid_argument);
    CHECK_THROWS_AS(graph.removeVertex(other.begin()), std::invalid_argument);
    CHECK_THROWS_AS(graph.removeEdge(other.edges().begin()), std::invalid_argument);

    const auto staleVertex = graph.begin();
    auto edge = std::as_const(graph).edges().begin();
    ++edge;
    graph.removeEdge(edge);
    CHECK_FALSE(graph.containsEdge(beta, alpha));
    CHECK(graph.containsEdge(alpha, beta));
    CHECK_THROWS_AS(graph.removeEdge(edge), std::invalid_argument);
    CHECK_THROWS_AS(graph.removeVertex(staleVertex), std::invalid_argument);

    const auto vertex = std::next(graph.begin());
    graph.removeVertex(vertex);
    CHECK_FALSE(graph.containsVertex(beta));
    CHECK(graph.containsEdge(gamma, alpha));
    CHECK(graph.containsVertex(isolated));
    const auto beforeAssign = graph.begin();
    graph = other;
    CHECK_THROWS_AS(graph.removeVertex(beforeAssign), std::invalid_argument);
}

TEST_CASE("Invalid graph operations fail without changing counts or iterators", "[core]") {
    Sample sample;
    auto& [graph, alpha, beta, gamma, isolated] = sample;
    const auto expected = graph;
    const auto cursor = graph.begin();
    constexpr Graph::vertex_id missing = 0;
    CHECK_THROWS_AS(graph.addEdge(alpha, alpha), std::invalid_argument);
    CHECK_THROWS_AS(graph.addEdge(alpha, beta), std::invalid_argument);
    CHECK_THROWS_AS(graph.addEdge(missing, beta), std::out_of_range);
    CHECK_THROWS_AS(graph.removeEdge(alpha, gamma), std::invalid_argument);
    CHECK_THROWS_AS(graph.edgeDegree(alpha, gamma), std::invalid_argument);
    CHECK_THROWS_AS(graph.removeVertex(missing), std::out_of_range);
    CHECK_THROWS_AS(graph.value(missing), std::out_of_range);
    CHECK_THROWS_AS(graph.incidentEdges(missing), std::out_of_range);
    CHECK_THROWS_AS(graph.adjacentVertices(missing), std::out_of_range);
    CHECK_THROWS_AS(graph.degree(missing), std::out_of_range);
    CHECK(graph == expected);
    CHECK(graph.vertexId(cursor) == alpha);
}

TEST_CASE("Graph retains its arcs, values and iterators when a vertex copy fails", "[core]") {
    lab4::DirectedGraph<ThrowingValue> original;
    const auto first = original.addVertex(ThrowingValue(5));
    const auto second = original.addVertex(ThrowingValue(9));
    original.addEdge(first, second);
    const auto cursor = original.begin();
    lab4::DirectedGraph<ThrowingValue> other;
    other.addVertex(ThrowingValue(12));

    ThrowingValue::throwOnCopy = true;
    CHECK_THROWS_AS(original.removeVertex(second), std::runtime_error);
    CHECK_THROWS_AS(original = other, std::runtime_error);
    ThrowingValue::throwOnCopy = false;

    CHECK(original.vertexCount() == 2);
    CHECK(original.value(first).number == 5);
    CHECK(original.value(second).number == 9);
    CHECK(original.containsEdge(first, second));
    CHECK(original.vertexId(cursor) == first);
}

TEST_CASE("Graph stream output lists values and directed arcs using IDs", "[core]") {
    Graph graph;
    const auto first = graph.addVertex("alpha");
    const auto second = graph.addVertex("beta");
    graph.addEdge(second, first);
    std::ostringstream out;
    out << graph;
    CHECK(out.str() == "Vertices:\n  1: alpha\n  2: beta\nArcs:\n  2 -> 1\n");
}
