#include "Console.hpp"
#include "Display.hpp"
#include <catch2/catch_test_macros.hpp>
#include <sstream>
using namespace post;
using namespace post::cli;
TEST_CASE("Console consumes complete bounded integer lines", "[console]") {
    std::istringstream input("12tail\n99999999999999999999999\n-1\n\n+2\n 2 \n");
    std::ostringstream output;
    Console console(input, output);
    REQUIRE(console.integer("Число", 0, 3) == 2);
    REQUIRE(output.str().find("Ошибка ввода") != std::string::npos);
    REQUIRE_THROWS_AS(console.integer("Число", 5, 4), std::invalid_argument);
    REQUIRE_THROWS_AS(console.text("Конец"), std::ios_base::failure);
    REQUIRE(console.ended());
}
TEST_CASE("Console handles money yes-no text whitespace and exact integer limits", "[console]") {
    std::istringstream input("\t Имя с пробелом \r\n \t \n1\n0\n0\n9223372036854775807\n");
    std::ostringstream output;
    Console console(input, output);
    REQUIRE(console.text("Имя") == "Имя с пробелом");
    REQUIRE(console.text("Пусто").empty());
    REQUIRE(console.yesNo("Да"));
    REQUIRE_FALSE(console.yesNo("Нет"));
    REQUIRE(console.money("Сумма") == Money{});
    REQUIRE(console.integer("Предел") == std::numeric_limits<std::int64_t>::max());
}
TEST_CASE("Console rejects invalid calendar dates and parses leap days", "[console]") {
    std::istringstream input("2025-02-29\n0000-01-01\n2024-13-01\n2024-1x-01\n2024-01-0x\n2024-02-"
                             "29extra\n2024-02-29\n");
    std::ostringstream output;
    Console console(input, output);
    REQUIRE(console.date("Дата") == std::chrono::year{2024} / 2 / 29);
    REQUIRE(output.str().find("недопустимая дата") != std::string::npos);
}
TEST_CASE("Console reports broken streams separately from EOF", "[console]") {
    std::istringstream input;
    input.setstate(std::ios_base::badbit);
    std::ostringstream output;
    Console console(input, output);
    REQUIRE_THROWS_AS(console.text("Ввод"), std::ios_base::failure);
    REQUIRE_FALSE(console.ended());
}
TEST_CASE("CLI formatters preserve integer money time and status names", "[display]") {
    REQUIRE(moneyText(Money(1)) == "0.01 BYN");
    REQUIRE(moneyText(Money(105)) == "1.05 BYN");
    REQUIRE(timeText(std::chrono::sys_seconds{std::chrono::seconds{0}}) ==
            "1970-01-01 00:00:00 UTC");
    REQUIRE(statusText(ShipmentStatus::Lost) == "Lost");
    REQUIRE(statusText(InvoiceStatus::Paid) == "Paid");
    REQUIRE(statusText(TripState::Completed) == "Completed");
    REQUIRE(statusText(DeliveryTaskState::ReturnedToBranch) == "ReturnedToBranch");
    REQUIRE(statusText(ReturnStatus::Started) == "Started");
    REQUIRE(statusText(SearchStatus::Found) == "Found");
    REQUIRE(statusText(CompensationStatus::Rejected) == "Rejected");
    REQUIRE(trim("\v\f id \f\v") == "id");
    REQUIRE(statusText(static_cast<ShipmentStatus>(999)) == "Unknown");
}
