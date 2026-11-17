#include "Console.hpp"

#include "lab4/DirectedGraph.hpp"
#include "lab4/Sorting.hpp"
#include "lab4/Task.hpp"

#include <algorithm>
#include <charconv>
#include <functional>
#include <iterator>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

namespace lab4 {
namespace {

using Graph = DirectedGraph<std::string>;
using Id = Graph::vertex_id;
using Input = std::istringstream;
using Output = std::ostream;

constexpr std::string_view helpText = R"(Sorting
  sort <cocktail|strand> [integers...]
  tasks <cocktail|strand>

Graph
  demo                         add a sample graph to an empty graph
  add-vertex <name>
  vertex <id>                  show a vertex value
  remove-vertex <id>
  add-arc <from> <to>
  remove-arc <from> <to>
  has-vertex <id>
  has-arc <from> <to>
  degree <id>
  arc-degree <from> <to>
  walk <vertices|arcs> [reverse]
  walk <incident|adjacent> <id> [reverse]
  show
  count
  clear

  help
  quit
)";

void expectEnd(Input& input) {
    std::string extra;
    if (input >> extra) {
        throw std::invalid_argument("unexpected argument: " + extra);
    }
}

std::string readToken(Input& input, std::string_view field) {
    std::string token;
    if (!(input >> token)) {
        throw std::invalid_argument("expected " + std::string(field));
    }
    return token;
}

template <class Number> Number parseNumber(std::string_view token, std::string_view field) {
    Number value{};
    const auto* end = token.data() + token.size();
    const auto result = std::from_chars(token.data(), end, value);
    if (result.ec != std::errc{} || result.ptr != end) {
        throw std::invalid_argument("invalid " + std::string(field) + ": " + std::string(token));
    }
    return value;
}

Id readId(Input& input) {
    return parseNumber<Id>(readToken(input, "vertex ID"), "vertex ID");
}

enum class Sort { cocktail, strand };

Sort readSort(Input& input) {
    const auto name = readToken(input, "cocktail or strand");
    if (name == "cocktail") {
        return Sort::cocktail;
    }
    if (name == "strand") {
        return Sort::strand;
    }
    throw std::invalid_argument("expected cocktail or strand");
}

void sortRange(Sort algorithm, auto first, auto last, auto compare) {
    if (algorithm == Sort::cocktail) {
        cocktailSort(first, last, compare);
    } else {
        strandSort(first, last, compare);
    }
}

void printValue(Output& out, const Graph::edge_type& edge) {
    out << edge.first << " -> " << edge.second;
}

void printValue(Output& out, const Task& task) {
    out << task.name() << " (" << task.priority() << ')';
}

void printValue(Output& out, const auto& value) {
    out << value;
}

void printRange(Output& out, std::string_view label, auto&& range) {
    out << label << ':';
    for (const auto& value : range) {
        out << ' ';
        printValue(out, value);
    }
    out << '\n';
}

void sortNumbers(const Graph&, Input& input, Output& out) {
    const auto algorithm = readSort(input);
    std::vector<int> numbers;
    for (std::string token; input >> token;) {
        numbers.push_back(parseNumber<int>(token, "integer"));
    }
    printRange(out, "Before", numbers);
    sortRange(algorithm, numbers.begin(), numbers.end(), std::less<>{});
    printRange(out, "After", numbers);
}

void sortTasks(const Graph&, Input& input, Output& out) {
    const auto algorithm = readSort(input);
    expectEnd(input);
    Task tasks[] = {{"Write tests", 2}, {"Fix build", 1}, {"Review docs", 2}, {"Ship release", 3}};
    printRange(out, "Before", tasks);
    sortRange(
        algorithm, std::begin(tasks), std::end(tasks),
        [](const Task& left, const Task& right) { return left.priority() < right.priority(); });
    printRange(out, "After", tasks);
}

void demo(Graph& graph, Input& input, Output& out) {
    expectEnd(input);
    if (!graph.empty()) {
        throw std::invalid_argument("demo requires an empty graph");
    }
    const auto alpha = graph.addVertex("Alpha");
    const auto beta = graph.addVertex("Beta");
    const auto gamma = graph.addVertex("Gamma");
    graph.addEdge(alpha, beta);
    graph.addEdge(beta, alpha);
    graph.addEdge(beta, gamma);
    out << graph;
}

void addVertex(Graph& graph, Input& input, Output& out) {
    std::string name;
    if (!std::getline(input >> std::ws, name)) {
        throw std::invalid_argument("expected a vertex name");
    }
    const auto id = graph.addVertex(std::move(name));
    out << "Vertex " << id << '\n';
}

void getVertex(const Graph& graph, Input& input, Output& out) {
    const auto id = readId(input);
    expectEnd(input);
    out << graph.value(id) << '\n';
}

void removeVertex(Graph& graph, Input& input, Output& out) {
    const auto id = readId(input);
    expectEnd(input);
    graph.removeVertex(id);
    out << "OK\n";
}

void hasVertex(const Graph& graph, Input& input, Output& out) {
    const auto id = readId(input);
    expectEnd(input);
    out << (graph.containsVertex(id) ? "yes" : "no") << '\n';
}

void showDegree(const Graph& graph, Input& input, Output& out) {
    const auto id = readId(input);
    expectEnd(input);
    const auto incoming = graph.inDegree(id);
    const auto outgoing = graph.outDegree(id);
    out << "In: " << incoming << ", out: " << outgoing << ", total: " << incoming + outgoing
        << '\n';
}

template <auto Operation, class GraphRef = Graph>
void arcCommand(GraphRef& graph, Input& input, Output& out) {
    const auto from = readId(input);
    const auto to = readId(input);
    expectEnd(input);
    using Result = std::invoke_result_t<decltype(Operation), GraphRef&, Id, Id>;
    if constexpr (std::is_void_v<Result>) {
        std::invoke(Operation, graph, from, to);
        out << "OK\n";
    } else if constexpr (std::is_same_v<Result, bool>) {
        out << (std::invoke(Operation, graph, from, to) ? "yes" : "no") << '\n';
    } else {
        out << "Arc degree: " << std::invoke(Operation, graph, from, to) << '\n';
    }
}

bool readReverse(Input& input) {
    std::string option;
    if (!(input >> option)) {
        return false;
    }
    if (option != "reverse") {
        throw std::invalid_argument("expected reverse");
    }
    expectEnd(input);
    return true;
}

void printWalk(Output& out, const std::string& name, auto range, bool reverse) {
    if (reverse) {
        printRange(out, name, std::views::reverse(range));
    } else {
        printRange(out, name, range);
    }
}

void walkGraph(const Graph& graph, Input& input, Output& out) {
    const auto name = readToken(input, "walk type");
    if (name == "vertices") {
        printWalk(out, name, graph.vertices(), readReverse(input));
    } else if (name == "arcs") {
        printWalk(out, name, graph.edges(), readReverse(input));
    } else if (name == "incident") {
        const auto id = readId(input);
        printWalk(out, name, graph.incidentEdges(id), readReverse(input));
    } else if (name == "adjacent") {
        const auto id = readId(input);
        printWalk(out, name, graph.adjacentVertices(id), readReverse(input));
    } else {
        throw std::invalid_argument("unknown walk type: " + name);
    }
}

void showGraph(const Graph& graph, Input& input, Output& out) {
    expectEnd(input);
    out << graph;
}

void countGraph(const Graph& graph, Input& input, Output& out) {
    expectEnd(input);
    out << "Vertices: " << graph.vertexCount() << ", arcs: " << graph.edgeCount() << '\n';
}

void clearGraph(Graph& graph, Input& input, Output& out) {
    expectEnd(input);
    graph.clear();
    out << "OK\n";
}

void showHelp(const Graph&, Input& input, Output& out) {
    expectEnd(input);
    out << helpText;
}

using Command = std::function<void(Graph&, Input&, Output&)>;
using Entry = std::pair<std::string_view, Command>;

void execute(Graph& graph, const std::string& name, Input& input, Output& out) {
    static const Entry commands[] = {
        {"sort", sortNumbers},
        {"tasks", sortTasks},
        {"demo", demo},
        {"add-vertex", addVertex},
        {"vertex", getVertex},
        {"remove-vertex", removeVertex},
        {"has-vertex", hasVertex},
        {"degree", showDegree},
        {"add-arc", arcCommand<&Graph::addEdge>},
        {"remove-arc", arcCommand<static_cast<void (Graph::*)(Id, Id)>(&Graph::removeEdge)>},
        {"has-arc", arcCommand<&Graph::containsEdge, const Graph>},
        {"arc-degree", arcCommand<&Graph::edgeDegree, const Graph>},
        {"walk", walkGraph},
        {"show", showGraph},
        {"count", countGraph},
        {"clear", clearGraph},
        {"help", showHelp},
    };
    const auto command = std::ranges::find(commands, name, &Entry::first);
    if (command == std::end(commands)) {
        throw std::invalid_argument("unknown command: " + name);
    }
    command->second(graph, input, out);
}

bool runLine(Graph& graph, const std::string& line, Output& out) {
    Input input(line);
    std::string name;
    if (!(input >> name)) {
        return true;
    }
    try {
        if (name == "quit") {
            expectEnd(input);
            return false;
        }
        execute(graph, name, input, out);
    } catch (const std::exception& error) {
        out << "Error: " << error.what() << '\n';
    }
    return true;
}

} // namespace

void runConsole(std::istream& input, std::ostream& output) {
    Graph graph;
    output << "Lab4: sorting and directed graphs. Type help for commands.\n";
    for (std::string line;;) {
        output << "lab4> " << std::flush;
        if (!std::getline(input, line) || !runLine(graph, line, output)) {
            return;
        }
    }
}

} // namespace lab4
