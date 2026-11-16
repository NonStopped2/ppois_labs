#pragma once

#include "InputReader.hpp"
#include "shop/application/StoreState.hpp"
#include <istream>
#include <ostream>

namespace shop::ui {
class ConsoleApplication {
  public:
    ConsoleApplication(StoreState state, std::ostream& output);
    bool execute(std::string_view line);
    void run(std::istream& input, bool prompts);
    const StoreState& state() const noexcept;
    std::size_t errorCount() const noexcept;
    static std::string help();

  private:
    void requireOperator() const;
    const std::string& actor() const;
    Address selectedAddress(std::string_view label) const;
    void catalogCommands(std::string_view command, Arguments& args);
    void customerCommands(std::string_view command, Arguments& args);
    void cartCommands(std::string_view command, Arguments& args);
    void orderCommands(std::string_view command, Arguments& args);
    void returnCommands(Arguments& args);
    void reviewCommands(Arguments& args);
    void reportCommand(Arguments& args);
    void showOrder(const Order& order);
    void showQuote(const PriceQuote& quote);
    void showCart(const Cart& cart);
    StoreState state_;
    std::ostream& output_;
    std::string customerId_;
    std::optional<PriceQuote> quote_;
    std::optional<std::pair<std::string, DeliveryTerms>> deliveryChange_;
    std::size_t errorCount_ = 0;
};
int runProgram(const std::vector<std::string>& arguments, std::istream& input, std::ostream& output,
               std::ostream& errors);
} // namespace shop::ui
