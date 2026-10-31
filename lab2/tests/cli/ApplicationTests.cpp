#include "Application.hpp"
#include "Display.hpp"
#include "Scripts.hpp"
#include <catch2/catch_test_macros.hpp>
#include <sstream>
using namespace post::cli;
namespace {
std::string run(const std::string& script, bool demo = true) {
    std::istringstream input(script);
    std::ostringstream output;
    Console console(input, output);
    Application app(console);
    REQUIRE(app.run(demo) == 0);
    return output.str();
}
} // namespace
TEST_CASE("CLI recovers an expired unstarted delivery without restarting the session",
          "[application]") {
    using namespace post::cli::test;
    auto script = paid("BY-2", "i2", "550") + command(6, 1, {"d1", "A", "BY-2", "0", "60"});
    script += advance("61") + command(6, 2, {"d1"}) + command(6, 7, {"d1"});
    script += command(6, 5, {"d1", "d1-BY-2", "d2", "0", "60"}) + command(6, 2, {"d2"});
    script += command(6, 3,
                      {"d2", "d1-BY-2", "done", "1", "local", "1", "AB123", "Вера Местная",
                       "2020-01-01", "2030-12-31", ""});
    script += command(6, 4, {"d2"}) + command(6, 6) + command(2, 4, {"BY-2"}) + "0\n";
    const auto output = run(script);
    REQUIRE(output.find("Delivery task not ready at departure") != std::string::npos);
    REQUIRE(output.find("Курьерский план отменён") != std::string::npos);
    REQUIRE(output.find("state=Cancelled") != std::string::npos);
    REQUIRE(output.find("отменён=1") != std::string::npos);
    REQUIRE(output.find("Parcel | Delivered") != std::string::npos);
    REQUIRE(output.find("попыток=1") != std::string::npos);
}
TEST_CASE("CLI returns expired in-progress cargo and replans without inventing an attempt",
          "[application]") {
    using namespace post::cli::test;
    auto script = paid("BY-2", "i2", "550") + command(6, 1, {"d1", "A", "BY-2", "0", "60"}) +
                  command(6, 2, {"d1"});
    script += advance("61") + command(6, 4, {"d1"}) + command(6, 8, {"d1"}) + command(6, 6);
    script += command(6, 5, {"d1", "d1-BY-2", "d2", "0", "60"}) + command(6, 2, {"d2"});
    script += command(6, 3,
                      {"d2", "d1-BY-2", "done", "1", "local", "1", "AB123", "Вера Местная",
                       "2020-01-01", "2030-12-31", ""});
    script += command(6, 4, {"d2"}) + command(6, 6) + command(2, 4, {"BY-2"}) + "0\n";
    const auto output = run(script);
    REQUIRE(output.find("Курьерский маршрут прерван") != std::string::npos);
    REQUIRE(output.find("state=ReturnedToBranch | попыток=0") != std::string::npos);
    REQUIRE(output.find("Parcel | Delivered") != std::string::npos);
    REQUIRE(output.find("попыток=1") != std::string::npos);
    REQUIRE(output.find("DeliveryFailed") == std::string::npos);
}
TEST_CASE("CLI configuration describes the policy actually used by the application",
          "[application]") {
    const auto output = run(post::cli::test::command(9, 1) + "0\n");
    const auto policy = defaultStoragePolicy();
    REQUIRE(output.find("хранение " + std::to_string(policy.maxStorageDays())) !=
            std::string::npos);
    REQUIRE(output.find("бесплатно " + std::to_string(policy.freeStorageDays())) !=
            std::string::npos);
    REQUIRE(output.find(moneyText(policy.dailyFee()) + "/день") != std::string::npos);
    REQUIRE(output.find("попыток " + std::to_string(policy.maxDeliveryAttempts())) !=
            std::string::npos);
}
TEST_CASE("CLI shows demo cards and distinct draft shipment kinds", "[application]") {
    const auto output = run("1\n2\n2\n3\n9\n1\n0\n");
    REQUIRE(output.find("sender | Анна Отправитель") != std::string::npos);
    REQUIRE(output.find("BY-1 | Letter | Draft") != std::string::npos);
    REQUIRE(output.find("BY-2 | Parcel | Draft") != std::string::npos);
    REQUIRE(output.find("2026-01-01 00:00:00 UTC") != std::string::npos);
}
TEST_CASE("CLI pays accepts and preserves state after repeated acceptance", "[application]") {
    const auto output = run("3\n1\nBY-2\ni2\nstandard\n3\n3\nA\ni2\np2\n550\n1\n4\n1\nA\nBY-"
                            "2\ni2\n4\n1\nA\nBY-2\ni2\n4\n2\nA\n2\n4\nBY-2\n0\n");
    REQUIRE(output.find("BY-2 | Parcel | AwaitingPickup") != std::string::npos);
    REQUIRE(output.find("Ошибка операции") != std::string::npos);
    REQUIRE(output.find("AwaitingPickup | A") != std::string::npos);
}
TEST_CASE("CLI EOF rolls back a partially assembled multi-object command", "[application]") {
    std::istringstream input("1\n1\nnew\nНовый клиент\n220000\nГород\n");
    std::ostringstream output;
    Console console(input, output);
    Application app(console);
    REQUIRE(app.run() == 0);
    REQUIRE(output.str().find("Незавершённая команда отменена") != std::string::npos);
    input.clear();
    input.str("1\n2\n0\n");
    output.str("");
    REQUIRE(app.run() == 0);
    REQUIRE(output.str().find("new | Новый клиент") == std::string::npos);
}
TEST_CASE(
    "CLI handles back invalid menu input unknown IDs and duplicate demo without resetting data",
    "[application]") {
    const auto output = run("99\nx\n1\n0\n2\n4\nmissing\n1\n3\n2\n3\n0\n");
    REQUIRE(output.find("Ошибка ввода") != std::string::npos);
    REQUIRE(output.find("Ошибка операции") != std::string::npos);
    REQUIRE(output.find("BY-1 | Letter | Draft") != std::string::npos);
}
TEST_CASE("CLI creates custom clients letters and sealed parcels", "[application]") {
    using namespace post::cli::test;
    auto script = command(1, 1,
                          {"custom", "Иван Клиент", "220001", "Минск", "Улица", "2", "15",
                           "+375291112233", "ivan@example.com"});
    script += command(
        2, 1, {"custom", "recipient", "20", "200", "100", "5", "2", "Бумаги", "0", "0", "0"});
    script += command(2, 2,
                      {"sender", "local", "500", "200", "100", "50", "0", "0", "1", "0", "0", "2",
                       "1", "Книга", "1", "100", "1000", "2", "seal-custom"});
    script += command(1, 2) + command(2, 3) + command(2, 5, {"BY-3", "Отмена"}) +
              command(2, 4, {"BY-3"}) + "0\n";
    const auto output = run(script);
    REQUIRE(output.find("Ошибка операции") == std::string::npos);
    REQUIRE(output.find("BY-4 | Parcel | Draft") != std::string::npos);
    REQUIRE(output.find("Letter | Cancelled") != std::string::npos);
}
TEST_CASE("CLI performs full transportation return decisions and original sender handover",
          "[application]") {
    using namespace post::cli::test;
    auto script = paid() + transport("t1", "A", "B");
    script += command(7, 1, {"rejected", "BY-1", "1", "sender"}) +
              command(7, 2, {"rejected", "B", "0", "Нет основания"});
    script += command(7, 1, {"return", "BY-1", "1", "sender"}) +
              command(7, 2, {"return", "B", "1"}) + command(7, 3, {"return", "B"});
    script +=
        transport("t2", "B", "A") + command(4, 3,
                                            {"A", "BY-1", "sender", "1", "AB123",
                                             "Анна Отправитель", "2020-01-01", "2030-12-31", ""});
    script += command(5, 6) + command(7, 7) + command(2, 4, {"BY-1"}) + "0\n";
    const auto output = run(script);
    REQUIRE(output.find("Ошибка операции") == std::string::npos);
    REQUIRE(output.find("Letter | Returned") != std::string::npos);
    REQUIRE(output.find("state=Rejected") != std::string::npos);
}
TEST_CASE("CLI courier retry carries history and authorizes representative", "[application]") {
    using namespace post::cli::test;
    auto script = paid("BY-2", "i2", "550") + command(6, 1, {"d1", "A", "BY-2", "0", "3600"}) +
                  command(6, 2, {"d1"});
    script += command(6, 3, {"d1", "d1-BY-2", "f1", "0", "1", "Нет дома"}) + command(6, 4, {"d1"}) +
              advance("3600");
    script += command(6, 5, {"d1", "d1-BY-2", "d2", "0", "3600"}) +
              command(4, 4, {"auth", "BY-2", "rep", "3600"}) + command(6, 2, {"d2"});
    script += command(6, 3,
                      {"d2", "d1-BY-2", "done", "1", "rep", "1", "AB123", "Глеб Представитель",
                       "2020-01-01", "2030-12-31", "auth"});
    script += command(6, 4, {"d2"}) + command(6, 6) + command(2, 4, {"BY-2"}) + "0\n";
    const auto output = run(script);
    REQUIRE(output.find("Ошибка операции") == std::string::npos);
    REQUIRE(output.find("Parcel | Delivered") != std::string::npos);
    REQUIRE(output.find("попыток=2") != std::string::npos);
}
TEST_CASE("CLI revoked authorization rejects delivery without consuming the task",
          "[application]") {
    using namespace post::cli::test;
    auto script = paid("BY-2", "i2", "550") + command(4, 4, {"auth", "BY-2", "rep", "3600"}) +
                  command(4, 5, {"auth"});
    script += command(6, 1, {"d", "A", "BY-2", "0", "3600"}) + command(6, 2, {"d"});
    script += command(6, 3,
                      {"d", "d-BY-2", "bad", "1", "rep", "1", "AB123", "Глеб Представитель",
                       "2020-01-01", "2030-12-31", "auth"});
    script += command(6, 3,
                      {"d", "d-BY-2", "good", "1", "local", "2", "ID123", "Вера Местная",
                       "2020-01-01", "2030-12-31", ""}) +
              command(6, 4, {"d"}) + command(6, 6) + "0\n";
    const auto output = run(script);
    REQUIRE(output.find("Ошибка операции") != std::string::npos);
    REQUIRE(output.find("попыток=1") != std::string::npos);
    REQUIRE(output.find("state=Delivered") != std::string::npos);
}
TEST_CASE("CLI completes physical search and cash compensation without confusing postage refund",
          "[application]") {
    using namespace post::cli::test;
    auto script = paid() + command(7, 4, {"found", "BY-1", "sender", "A,B"}) +
                  command(7, 5, {"found", "A", "1", "На складе"}) +
                  command(7, 6, {"found", "A", "1"});
    script += command(7, 4, {"lost", "BY-1", "sender", "A,B"}) +
              command(7, 5, {"lost", "A", "0", "Не найдено"}) +
              command(7, 5, {"lost", "B", "0", "Не поступило"}) + advance("3600") +
              command(7, 6, {"lost", "A", "0"});
    script += command(8, 1, {"claim", "BY-1", "sender", "500", "1", "report-1,report-2"}) +
              command(8, 2, {"claim", "A", "1", "500", "500", "lost"}) +
              command(8, 3, {"A", "claim"});
    script += command(3, 4, {"A", "i1", "pay-i1", "refund", "100", "Возврат тарифа"}) +
              command(3, 2) + command(7, 7) + command(8, 4, {"A"}) + command(2, 4, {"BY-1"}) +
              "0\n";
    const auto output = run(script);
    REQUIRE(output.find("Ошибка операции") == std::string::npos);
    REQUIRE(output.find("Letter | Lost") != std::string::npos);
    REQUIRE(output.find("наличность 497.00 BYN") != std::string::npos);
    REQUIRE(output.find("state=Found") != std::string::npos);
}
TEST_CASE("CLI handles damage claim rejection reopening cash and refreshing shifts",
          "[application]") {
    using namespace post::cli::test;
    auto script = paid() + command(8, 1, {"damage", "BY-1", "sender", "100", "2", "photo"}) +
                  command(8, 2, {"damage", "A", "0", "Нет подтверждения"});
    script += command(8, 5, {"A", "0", "50300"}) + command(8, 5, {"A", "1", "10000"}) +
              command(8, 4, {"A"});
    script += command(5, 5, {"A", "van-1", "1"}) + advance("86400") + command(9, 3, {"A"}) +
              command(9, 4, {"A", "shift-3", "60"}) + advance("60") + command(9, 1) +
              command(7, 7) + "0\n";
    const auto output = run(script);
    REQUIRE(output.find("Ошибка операции") == std::string::npos);
    REQUIRE(output.find("Новые смены открыты") != std::string::npos);
    REQUIRE(output.find("state=Rejected") != std::string::npos);
}
TEST_CASE("CLI late plan failure rolls back assignment and permits reuse of the same route ID",
          "[application]") {
    using namespace post::cli::test;
    auto script = paid("BY-2", "i2", "550") + command(6, 1, {"d", "A", "BY-2,BY-1", "0", "3600"}) +
                  command(6, 1, {"d", "A", "BY-2", "0", "3600"});
    script += command(6, 2, {"d"}) + command(6, 3, {"d", "d-BY-2", "f", "0", "3", "Отказ"}) +
              command(6, 4, {"d"});
    script += command(6, 5, {"d", "d-BY-2", "next", "0", "3600"}) + command(6, 6) + "0\n";
    const auto output = run(script);
    REQUIRE(output.find("Ошибка операции") != std::string::npos);
    REQUIRE(output.find("Маршрут создан: d;") != std::string::npos);
    REQUIRE(output.find("state=ReturnedToBranch") != std::string::npos);
}
TEST_CASE("CLI refuses stale quote payment and permits cancellation and recalculation",
          "[application]") {
    using namespace post::cli::test;
    auto script =
        command(3, 1, {"BY-1", "old", "standard"}) + command(2, 6, {"BY-1", "21", "0", "0", "0"});
    script += command(3, 3, {"A", "old", "bad", "300", "1"}) + command(3, 5, {"old"});
    script += command(3, 1, {"BY-1", "new", "express"}) +
              command(3, 3, {"A", "new", "good", "600", "2"}) +
              command(4, 1, {"A", "BY-1", "new"}) + command(3, 2) + "0\n";
    const auto output = run(script);
    REQUIRE(output.find("Ошибка операции") != std::string::npos);
    REQUIRE(output.find("Letter | Accepted") != std::string::npos);
    REQUIRE(output.find("status=Cancelled") != std::string::npos);
}
TEST_CASE("CLI reads branch representative document and rejects unknown or duplicate lists",
          "[application]") {
    using namespace post::cli::test;
    auto script =
        paid() + transport("t", "A", "B") + command(4, 4, {"auth", "BY-1", "rep", "3600"});
    script += command(4, 3,
                      {"B", "BY-1", "rep", "1", "AB123", "Глеб Представитель", "2020-01-01",
                       "2030-12-31", "auth"});
    script += command(7, 4, {"bad", "BY-2", "sender", "A,A"}) +
              command(7, 4, {"unknown", "BY-2", "sender", "A,Z"}) + command(2, 4, {"BY-1"}) + "0\n";
    const auto output = run(script);
    REQUIRE(output.find("Letter | Delivered") != std::string::npos);
    REQUIRE(output.find("Пустой или повторный элемент") != std::string::npos);
    REQUIRE(output.find("Не найден ID: Z") != std::string::npos);
}
TEST_CASE("CLI reports actual stream failures as unsuccessful execution", "[application]") {
    std::istringstream input;
    input.setstate(std::ios_base::badbit);
    std::ostringstream output;
    Console console(input, output);
    Application app(console);
    REQUIRE(app.run() == 1);
    REQUIRE(output.str().find("Ошибка ввода-вывода") != std::string::npos);
}
