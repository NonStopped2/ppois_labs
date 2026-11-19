#pragma once

#include <algorithm>
#include <compare>
#include <concepts>
#include <cstddef>
#include <functional>
#include <iterator>
#include <limits>
#include <ostream>
#include <ranges>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace lab4 {

/// A directed graph whose vertex values can be changed independently of its arcs.
template <std::copy_constructible T> class DirectedGraph {
  public:
    using value_type = T;
    using size_type = std::size_t;
    using vertex_id = size_type;
    using edge_type = std::pair<vertex_id, vertex_id>;

  private:
    struct Vertex {
        vertex_id id{};
        T value;
    };

    using Matrix = std::vector<unsigned char>;
    enum class Walk { vertices, edges, incident, adjacent };

    template <Walk Kind, bool IsConst> class Iterator {
        friend class DirectedGraph;
        template <Walk, bool> friend class Iterator;

        using Graph = std::conditional_t<IsConst, const DirectedGraph, DirectedGraph>;
        static constexpr bool yieldsEdge = Kind == Walk::edges || Kind == Walk::incident;
        using VertexReference = std::conditional_t<IsConst, const T&, T&>;

      public:
        using value_type = std::conditional_t<yieldsEdge, edge_type, T>;
        using reference = std::conditional_t<yieldsEdge, edge_type, VertexReference>;
        using difference_type = std::ptrdiff_t;
        using iterator_concept = std::bidirectional_iterator_tag;
        // Edge descriptors are produced by value. Legacy bidirectional iterators
        // require lvalue references, while the C++20 concept allows value results.
        using iterator_category =
            std::conditional_t<yieldsEdge, std::input_iterator_tag, iterator_concept>;

        Iterator() = default;

        template <bool OtherConst>
            requires(IsConst && !OtherConst)
        // A mutable iterator converts to its read-only counterpart, as in STL containers.
        // cppcheck-suppress noExplicitConstructor
        Iterator(const Iterator<Kind, OtherConst>& other) noexcept
            : graph_(other.graph_), slot_(other.slot_), pivot_(other.pivot_),
              revision_(other.revision_) {}

        reference operator*() const noexcept {
            if constexpr (yieldsEdge) {
                const auto [source, target] = endpoints();
                return {graph_->vertices_[source].id, graph_->vertices_[target].id};
            } else {
                return graph_->vertices_[slot_].value;
            }
        }

        Iterator& operator++() noexcept {
            ++slot_;
            seekForward();
            return *this;
        }

        Iterator operator++(int) noexcept {
            const auto previous = *this;
            ++*this;
            return previous;
        }

        Iterator& operator--() noexcept {
            do {
                --slot_;
            } while (!selected());
            return *this;
        }

        Iterator operator--(int) noexcept {
            const auto previous = *this;
            --*this;
            return previous;
        }

        bool operator==(const Iterator&) const = default;

      private:
        Iterator(Graph& graph, size_type slot, size_type pivot) noexcept
            : graph_(&graph), slot_(slot), pivot_(pivot), revision_(graph.revision_) {
            seekForward();
        }

        std::pair<size_type, size_type> endpoints() const noexcept {
            const auto count = graph_->vertexCount();
            if constexpr (Kind == Walk::edges) {
                return {slot_ / count, slot_ % count};
            } else {
                // Visit the incoming column first, then the outgoing row.
                if (slot_ < count) {
                    return {slot_, pivot_};
                }
                return {pivot_, slot_ - count};
            }
        }

        bool selected() const noexcept {
            if constexpr (Kind == Walk::vertices) {
                return true;
            } else if constexpr (Kind == Walk::adjacent) {
                return graph_->hasArc(pivot_, slot_);
            } else {
                const auto [source, target] = endpoints();
                return graph_->hasArc(source, target);
            }
        }

        void seekForward() noexcept {
            const auto limit = graph_->template walkLimit<Kind>();
            while (slot_ < limit && !selected()) {
                ++slot_;
            }
        }

        Graph* graph_ = nullptr;
        size_type slot_ = 0;
        size_type pivot_ = 0;
        size_type revision_ = 0;
    };

  public:
    using vertex_iterator = Iterator<Walk::vertices, false>;
    using const_vertex_iterator = Iterator<Walk::vertices, true>;
    using edge_iterator = Iterator<Walk::edges, false>;
    using const_edge_iterator = Iterator<Walk::edges, true>;
    using incident_iterator = Iterator<Walk::incident, false>;
    using const_incident_iterator = Iterator<Walk::incident, true>;
    using adjacent_iterator = Iterator<Walk::adjacent, false>;
    using const_adjacent_iterator = Iterator<Walk::adjacent, true>;

    DirectedGraph() = default;
    DirectedGraph(const DirectedGraph&) = default;

    DirectedGraph& operator=(const DirectedGraph& other) {
        if (this != &other) {
            DirectedGraph replacement(other);
            vertices_.swap(replacement.vertices_);
            matrix_.swap(replacement.matrix_);
            edgeCount_ = replacement.edgeCount_;
            nextId_ = replacement.nextId_;
            ++revision_;
        }
        return *this;
    }

    bool empty() const noexcept {
        return vertices_.empty();
    }

    size_type vertexCount() const noexcept {
        return vertices_.size();
    }

    size_type edgeCount() const noexcept {
        return edgeCount_;
    }

    /// Remove all vertices and arcs, invalidating all iterators.
    void clear() noexcept {
        vertices_.clear();
        matrix_.clear();
        edgeCount_ = 0;
        ++revision_;
    }

    bool containsVertex(vertex_id id) const noexcept {
        return findIndex(id) != vertexCount();
    }

    bool containsEdge(vertex_id from, vertex_id to) const noexcept {
        const auto source = findIndex(from);
        const auto target = findIndex(to);
        const auto count = vertexCount();
        return source < count && target < count && hasArc(source, target);
    }

    T& value(vertex_id id) {
        return vertices_[indexOf(id)].value;
    }

    const T& value(vertex_id id) const {
        return vertices_[indexOf(id)].value;
    }

    vertex_id vertexId(const_vertex_iterator vertex) const {
        const auto index = checkedSlot(vertex);
        return vertices_[index].id;
    }

    /// Append a vertex. Its ID is independent of its position and value.
    vertex_id addVertex(T payload) {
        if (nextId_ == std::numeric_limits<vertex_id>::max()) {
            throw std::length_error("vertex IDs exhausted");
        }
        auto expanded = expandedMatrix();
        vertices_.push_back({nextId_, std::move(payload)});
        matrix_.swap(expanded);
        ++revision_;
        return nextId_++;
    }

    void addEdge(vertex_id from, vertex_id to) {
        const auto source = indexOf(from);
        const auto target = indexOf(to);
        if (source == target || hasArc(source, target)) {
            throw std::invalid_argument("loops and duplicate arcs are forbidden");
        }
        matrix_[offset(source, target)] = 1;
        ++edgeCount_;
        ++revision_;
    }

    void removeVertex(vertex_id id) {
        eraseVertexAt(indexOf(id));
    }

    void removeVertex(const_vertex_iterator vertex) {
        eraseVertexAt(checkedSlot(vertex));
    }

    void removeEdge(vertex_id from, vertex_id to) {
        eraseArcAt(indexOf(from), indexOf(to));
    }

    void removeEdge(const_edge_iterator edge) {
        checkedSlot(edge);
        const auto [source, target] = edge.endpoints();
        eraseArcAt(source, target);
    }

    size_type inDegree(vertex_id id) const {
        return countArcs<true>(indexOf(id));
    }

    size_type outDegree(vertex_id id) const {
        return countArcs<false>(indexOf(id));
    }

    size_type degree(vertex_id id) const {
        return degreeAt(indexOf(id));
    }

    /// Count other arcs sharing either endpoint, counting the reciprocal arc once.
    size_type edgeDegree(vertex_id from, vertex_id to) const {
        const auto source = indexOf(from);
        const auto target = indexOf(to);
        requireArc(source, target);
        return degreeAt(source) + degreeAt(target) - 2 - hasArc(target, source);
    }

    bool operator==(const DirectedGraph& other) const
        requires std::equality_comparable<T>
    {
        return std::ranges::equal(vertices_, other.vertices_, std::equal_to<>{}, &Vertex::value,
                                  &Vertex::value) &&
               matrix_ == other.matrix_;
    }

    std::weak_ordering operator<=>(const DirectedGraph& other) const
        requires requires(const T& left, const T& right) {
            { left < right } -> std::convertible_to<bool>;
        }
    {
        const auto compareValues = [](const Vertex& left, const Vertex& right) {
            if (left.value < right.value) {
                return std::weak_ordering::less;
            }
            return right.value < left.value ? std::weak_ordering::greater
                                            : std::weak_ordering::equivalent;
        };
        const auto order = std::lexicographical_compare_three_way(
            vertices_.begin(), vertices_.end(), other.vertices_.begin(), other.vertices_.end(),
            compareValues);
        return order == 0 ? std::weak_ordering(matrix_ <=> other.matrix_) : order;
    }

    vertex_iterator begin() noexcept {
        return vertices().begin();
    }

    vertex_iterator end() noexcept {
        return vertices().end();
    }

    const_vertex_iterator begin() const noexcept {
        return vertices().begin();
    }

    const_vertex_iterator end() const noexcept {
        return vertices().end();
    }

    auto vertices() noexcept {
        return walk<Walk::vertices>(*this);
    }

    auto vertices() const noexcept {
        return walk<Walk::vertices>(*this);
    }

    auto edges() noexcept {
        return walk<Walk::edges>(*this);
    }

    auto edges() const noexcept {
        return walk<Walk::edges>(*this);
    }

    auto incidentEdges(vertex_id id) {
        return walk<Walk::incident>(*this, indexOf(id));
    }

    auto incidentEdges(vertex_id id) const {
        return walk<Walk::incident>(*this, indexOf(id));
    }

    auto adjacentVertices(vertex_id id) {
        return walk<Walk::adjacent>(*this, indexOf(id));
    }

    auto adjacentVertices(vertex_id id) const {
        return walk<Walk::adjacent>(*this, indexOf(id));
    }

    friend std::ostream& operator<<(std::ostream& out, const DirectedGraph& graph)
        requires requires(std::ostream& stream, const T& value) { stream << value; }
    {
        out << "Vertices:\n";
        const auto vertexRange = graph.vertices();
        auto position = vertexRange.begin();
        std::for_each(vertexRange.begin(), vertexRange.end(), [&](const T& payload) {
            out << "  " << graph.vertexId(position++) << ": " << payload << '\n';
        });
        out << "Arcs:\n";
        const auto edgeRange = graph.edges();
        std::for_each(edgeRange.begin(), edgeRange.end(), [&](const edge_type& edge) {
            out << "  " << edge.first << " -> " << edge.second << '\n';
        });
        return out;
    }

  private:
    template <Walk Kind> size_type walkLimit() const noexcept {
        if constexpr (Kind == Walk::edges) {
            return matrix_.size();
        } else if constexpr (Kind == Walk::incident) {
            return 2 * vertexCount();
        } else {
            return vertexCount();
        }
    }

    template <Walk Kind, class Graph> static auto walk(Graph& graph, size_type pivot = 0) noexcept {
        using Cursor = Iterator<Kind, std::is_const_v<Graph>>;
        return std::ranges::subrange(Cursor(graph, 0, pivot),
                                     Cursor(graph, graph.template walkLimit<Kind>(), pivot));
    }

    template <Walk Kind> size_type checkedSlot(Iterator<Kind, true> iterator) const {
        if (iterator.graph_ != this || iterator.revision_ != revision_ ||
            iterator.slot_ >= walkLimit<Kind>()) {
            throw std::invalid_argument("foreign, stale or end iterator");
        }
        return iterator.slot_;
    }

    size_type findIndex(vertex_id id) const noexcept {
        return std::ranges::find(vertices_, id, &Vertex::id) - vertices_.begin();
    }

    size_type indexOf(vertex_id id) const {
        const auto index = findIndex(id);
        if (index == vertexCount()) {
            throw std::out_of_range("unknown vertex ID");
        }
        return index;
    }

    size_type offset(size_type source, size_type target) const noexcept {
        return source * vertexCount() + target;
    }

    bool hasArc(size_type source, size_type target) const noexcept {
        return matrix_[offset(source, target)] != 0;
    }

    void requireArc(size_type source, size_type target) const {
        if (!hasArc(source, target)) {
            throw std::invalid_argument("arc does not exist");
        }
    }

    template <bool Incoming> size_type countArcs(size_type index) const noexcept {
        size_type count = 0;
        for (size_type neighbor = 0; neighbor < vertexCount(); ++neighbor) {
            count += Incoming ? hasArc(neighbor, index) : hasArc(index, neighbor);
        }
        return count;
    }

    size_type degreeAt(size_type index) const noexcept {
        return countArcs<true>(index) + countArcs<false>(index);
    }

    Matrix expandedMatrix() const {
        const auto count = vertexCount();
        const auto expanded = count + 1;
        if (expanded > matrix_.max_size() / expanded) {
            throw std::length_error("adjacency matrix too large");
        }
        Matrix result(expanded * expanded);
        for (size_type row = 0; row < count; ++row) {
            std::copy_n(matrix_.begin() + row * count, count, result.begin() + row * expanded);
        }
        return result;
    }

    Matrix matrixWithout(size_type removed) const {
        const auto count = vertexCount() - 1;
        Matrix result(count * count);
        auto cell = result.begin();
        for (size_type row = 0; row < vertexCount(); ++row) {
            for (size_type column = 0; column < vertexCount(); ++column) {
                if (row != removed && column != removed) {
                    *cell++ = matrix_[offset(row, column)];
                }
            }
        }
        return result;
    }

    void eraseVertexAt(size_type index) {
        auto reduced = matrixWithout(index);
        std::vector<Vertex> survivors;
        survivors.reserve(vertexCount() - 1);
        const auto removed = vertices_.begin() + index;
        std::copy(vertices_.begin(), removed, std::back_inserter(survivors));
        std::copy(std::next(removed), vertices_.end(), std::back_inserter(survivors));

        // Both allocations and all user-value copies have succeeded.
        edgeCount_ -= degreeAt(index);
        vertices_.swap(survivors);
        matrix_.swap(reduced);
        ++revision_;
    }

    void eraseArcAt(size_type source, size_type target) {
        requireArc(source, target);
        matrix_[offset(source, target)] = 0;
        --edgeCount_;
        ++revision_;
    }

    std::vector<Vertex> vertices_;
    Matrix matrix_;
    size_type edgeCount_ = 0;
    vertex_id nextId_ = 1;
    size_type revision_ = 0;
};

} // namespace lab4
