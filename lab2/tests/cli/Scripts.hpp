#pragma once
#include <initializer_list>
#include <sstream>
#include <string>
namespace post::cli::test {
// Сценарий состоит из того же построчного ввода, что вводит пользователь меню.
inline std::string command(int section, int action,
                           std::initializer_list<std::string> fields = {}) {
    std::ostringstream input;
    input << section << '\n' << action << '\n';
    for (const auto& field : fields) {
        input << field << '\n';
    }
    return input.str();
}
inline std::string paid(const std::string& number = "BY-1", const std::string& bill = "i1",
                        const std::string& amount = "300") {
    return command(3, 1, {number, bill, "standard"}) +
           command(3, 3, {"A", bill, "pay-" + bill, amount, "1"}) +
           command(4, 1, {"A", number, bill});
}
inline std::string advance(const std::string& seconds) {
    return command(9, 2, {seconds});
}
inline std::string transport(const std::string& trip, const std::string& source,
                             const std::string& target) {
    return command(5, 1, {trip, source, target, "van-1", "BY-1"}) + advance("300") +
           command(5, 2, {trip}) + advance("1200") + command(5, 3, {trip, target}) +
           advance("300") + command(5, 4, {trip, target});
}
} // namespace post::cli::test
