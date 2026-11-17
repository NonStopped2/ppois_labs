#pragma once

#include <string>
#include <utility>

namespace lab4 {

/// A sample value whose priority is independent from its identity in the demo.
class Task {
  public:
    Task(std::string name, int priority) : name_(std::move(name)), priority_(priority) {}

    const std::string& name() const noexcept {
        return name_;
    }

    int priority() const noexcept {
        return priority_;
    }

    bool operator==(const Task&) const = default;

  private:
    std::string name_;
    int priority_;
};

} // namespace lab4
