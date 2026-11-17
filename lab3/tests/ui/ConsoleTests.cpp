#include "ConsoleApplication.hpp"
#include "DemoData.hpp"
#include "InputError.hpp"
#include "InputReader.hpp"
#include "SnapshotCodec.hpp"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <unistd.h>

using namespace shop;
using namespace shop::ui;
namespace {
void executeScript(ConsoleApplication& application, std::ostringstream& output,
                   std::string_view script) {
    std::istringstream input{std::string(script)};
    application.run(input, false);
    INFO(output.str());
    REQUIRE(application.errorCount() == 0);
}
} // namespace

TEST_CASE("Console token parsing rejects ambiguous input and checks entire integer arguments") {
    REQUIRE(InputReader::tokenize("  cart add SKU 2 ") ==
            std::vector<std::string>{"cart", "add", "SKU", "2"});
    REQUIRE(InputReader::tokenize("review add \"текст с пробелом\" \"\"").back().empty());
    REQUIRE(InputReader::tokenize("  # комментарий").empty());
    REQUIRE(InputReader::tokenize(" ").empty());
    REQUIRE_THROWS_AS(InputReader::tokenize("\"без конца"), InputError);
    REQUIRE_THROWS_AS(InputReader::tokenize("\"a\"b"), InputError);
    REQUIRE(InputReader::integer("-123") == -123);
    REQUIRE(InputReader::yesNo("yes"));
    REQUIRE_FALSE(InputReader::yesNo("no"));
    REQUIRE_THROWS_AS(InputReader::integer("1x"), InputError);
    REQUIRE_THROWS_AS(InputReader::integer("999999999999999999999"), InputError);
    REQUIRE_THROWS_AS(InputReader::yesNo("true"), InputError);
}

TEST_CASE(
    "Console demo executes purchase decline shipment partial return review and reconciliation") {
    std::ostringstream output;
    ConsoleApplication application(makeDemoStore(), output);
    std::istringstream input(demoScript());
    application.run(input, false);
    INFO(output.str());
    REQUIRE(application.errorCount() == 0);
    REQUIRE(application.state().orders().order("order-1")->status() == OrderStatus::Delivered);
    REQUIRE(application.state().payments().refundedForOrder("order-1") == Money(1080));
    REQUIRE(application.state().reviews().ratingSummary("shirt").count == 1);
    REQUIRE(output.str().find("Сверка: 0 расхождений") != std::string::npos);
    REQUIRE(SnapshotCodec::decode(SnapshotCodec::encode(application.state())) ==
            application.state());
}

TEST_CASE("Console rejects invalid UTF-8 before it can prevent saving the store") {
    std::ostringstream output;
    ConsoleApplication application(makeDemoStore(), output);
    const auto before = application.state();
    const std::vector<std::string> invalid{"\xff",         "\xe2\x82",     "\xe2!\x82",
                                           "\xe0\x80\x80", "\xed\xa0\x80", "\xf4\x90\x80\x80"};
    for (const auto& text : invalid) {
        const auto errors = application.errorCount();
        application.execute("product add broken clothes title \"" + text + "\"");
        REQUIRE(application.errorCount() == errors + 1);
        REQUIRE(application.state() == before);
    }
    REQUIRE(output.str().find("некорректный UTF-8") != std::string::npos);
    REQUIRE(SnapshotCodec::decode(SnapshotCodec::encode(application.state())) == before);
    REQUIRE(InputReader::tokenize("\"ёж € \xf0\x9f\x93\xa6\"").front() == "ёж € \xf0\x9f\x93\xa6");
}

TEST_CASE("Console checks roles ownership and extra arguments before domain mutation") {
    std::ostringstream output;
    ConsoleApplication application(makeDemoStore(), output);
    REQUIRE(application.execute("cart add SHIRT-BLUE 1"));
    REQUIRE(application.errorCount() == 1);
    application.execute("as customer alice");
    const auto before = application.state();
    application.execute("cart add SHIRT-BLUE 1 extra");
    REQUIRE(application.state() == before);
    application.execute("clock advance 1");
    REQUIRE(application.state() == before);
    application.execute("order place op");
    REQUIRE(application.state() == before);
    application.execute("unknown");
    REQUIRE(application.state() == before);
    application.execute("cart add SHIRT-BLUE 1");
    REQUIRE(application.state().cart("alice").totalUnits() == 1);
    application.execute("cart show");
    application.execute("address list");
    application.execute("catalog search");
    application.execute("review rating shirt");
    REQUIRE_FALSE(application.execute("quit"));
}

TEST_CASE("Program arguments demo script EOF and missing files produce explicit outcomes") {
    std::istringstream empty;
    std::ostringstream output, errors;
    REQUIRE(runProgram({"--help"}, empty, output, errors) == 0);
    REQUIRE(errors.str().empty());
    REQUIRE(runProgram({"--demo"}, empty, output, errors) == 0);
    REQUIRE(runProgram({"--script", "-"}, empty, output, errors) == 0);
    REQUIRE(runProgram({"--script"}, empty, output, errors) == 1);
    REQUIRE(runProgram({"--bad"}, empty, output, errors) == 1);
    REQUIRE(runProgram({"--demo", "--script", "-"}, empty, output, errors) == 1);
    REQUIRE(runProgram({"--load", "/nonexistent/store.json"}, empty, output, errors) == 1);
    REQUIRE(runProgram({"--script", "/nonexistent/store.txt"}, empty, output, errors) == 1);
    std::istringstream badCommand("bad\n");
    REQUIRE(runProgram({"--script", "-"}, badCommand, output, errors) == 1);
}

TEST_CASE("Console catalog customers prices stock addresses and cart commands preserve domain "
          "contracts") {
    std::ostringstream output;
    ConsoleApplication application(makeDemoStore(), output);
    executeScript(application, output, R"(category add misc Другое root
category add source Источник root
category add target Назначение root
category add child Потомок source
category move child target
category archive source
category add merging Сливаемая root
category merge merging target
product add mug misc Кружка ""
product attribute mug capacity integer yes 250 500
variant add mug MUG "250 мл" 300 100 100 100 capacity=250
variant add mug MUG500 "500 мл" 400 100 100 150 capacity=500
product publish mug
price set MUG 700
price set MUG500 800
price category misc 500
catalog search Кружка root capacity=250
catalog variants mug capacity=500
catalog compare shirt mug
variant enable MUG500 no
variant enable MUG500 yes
product archive mug
product restore mug
product publish mug
product move target mug
promotion add exact Точная fixed 50 0 0 99999 no root
coupon add EX exact 10 2 0 99999
coupon usage EX alice
coupon disable EX
receipt post receipt2 Поставщик MUG 2 300 batch MUG500 3 400 batch2
stock show
stock count MUG 10
stock needs
stock audit
customer add carol Карина carol@example.org +375291234569
customer restrict carol Проверка
customer restore carol
as customer carol
address add home 220003 Минск Новая 1 ""
address replace home 220003 Минск Новая 1 5
address default home
address list
address remove home
as customer alice
wish add mug
wish show
wish to-cart mug MUG
cart show
cart quantity MUG 2
cart switch MUG MUG500
cart coupon WELCOME
cart coupon -
cart adjust
cart remove MUG500
wish remove mug
wish add mug
cart add SHIRT-BLUE 1
cart wish move-wish SHIRT-BLUE
as operator
product archive mug
as customer alice
wish purge
wish to-cart shirt SHIRT-BLUE
cart clear
wish remove shirt
cart add SHIRT-BLUE 1
as operator
variant enable SHIRT-BLUE no
as customer alice
cart prune
cart show
coupon usage WELCOME
quit
)");
    REQUIRE(application.state().cart("alice").lines().empty());
    REQUIRE(application.state().inventory().item("MUG").onHand() == 10);
    REQUIRE(application.state().prices().price("MUG").unitPrice() == Money(735));
    REQUIRE(SnapshotCodec::decode(SnapshotCodec::encode(application.state())) ==
            application.state());
}

TEST_CASE("Console order editing repeat cancellation packing correction and return decisions are "
          "callable") {
    std::ostringstream output;
    ConsoleApplication application(makeDemoStore(), output);
    executeScript(application, output, R"(as customer alice
cart add SHIRT-BLUE 3
order preview standard -
order place place1
order show order-1
order list unpaid 0 2000
order delivery-preview order-1 express home
order delivery-confirm change1
order extend order-1 extend1
order repeat-plan order-1
order repeat order-1 repeat1 SHIRT-BLUE
order cancel order-1 cancel1
order list cancelled
order preview pickup home
order place place2
order pay order-3 3238 transfer yes pay2
as operator
order show order-3
fulfill start order-3 start2
fulfill show order-3
fulfill pick order-3 SHIRT-BLUE 3 pick2
fulfill pack order-3 pack2
fulfill reopen order-3 reopen2
fulfill undo order-3 SHIRT-BLUE 1 undo2
fulfill pick order-3 SHIRT-BLUE 1 fix2
fulfill pack order-3 repack2
fulfill ship order-3 TRACK-UI ship2
as customer alice
order deliver order-3 deliver2
return open order-3 open1 SHIRT-BLUE 1 changed-mind
return show return-8
return list
return amend return-8 amend1 SHIRT-BLUE 2 defective
return withdraw return-8 withdraw1
return open order-3 open2 SHIRT-BLUE 1 wrong-item
as operator
return reject return-9 reject1 Причина
as customer alice
return open order-3 open3 SHIRT-BLUE 1 changed-mind
as operator
return inspect return-10 inspect3 SHIRT-BLUE 1 0 0 incomplete
as customer alice
review add order-3 shirt 5 Отлично
as operator
review moderate review-12 yes
as customer bob
review vote review-12
as customer alice
review rating shirt
review list shirt helpful 0 20
review list shirt high
review list shirt low
review list shirt id
review edit review-12 4 Уточнение
as operator
review moderate review-12 no Проверка
review moderate review-12 yes
clock show
clock cleanup
clock advance 1
report 0 1001
quit
)");
    REQUIRE(application.state().orders().order("order-1")->status() == OrderStatus::Cancelled);
    REQUIRE(application.state().orders().order("order-3")->status() == OrderStatus::Delivered);
    REQUIRE(application.state().returnRequests().at("return-8").status() ==
            ReturnStatus::Withdrawn);
    REQUIRE(application.state().reviews().review("review-12").rating() == 4);
    REQUIRE(SnapshotCodec::decode(SnapshotCodec::encode(application.state())) ==
            application.state());
}

TEST_CASE("Console file commands and program script files retain explicit replacement and reset "
          "previews") {
    auto pattern = (std::filesystem::temp_directory_path() / "shop-ui-test-XXXXXX").string();
    const auto created = ::mkdtemp(pattern.data());
    REQUIRE(created);
    const std::filesystem::path directory(created), snapshot = directory / "shop state.json",
                                                    script = directory / "commands.txt";
    std::ostringstream output;
    ConsoleApplication application(makeDemoStore(), output);
    std::ostringstream quoted;
    quoted << std::quoted(snapshot.string());
    const auto path = quoted.str();
    application.execute("save " + path);
    REQUIRE(application.errorCount() == 0);
    const auto before = application.state();
    application.execute("clock advance 1");
    application.execute("load " + path + " replace");
    REQUIRE(application.state() == before);
    {
        std::ofstream commands(script);
        commands << "clock show\nquit\n";
    }
    std::istringstream input;
    std::ostringstream errors;
    REQUIRE(runProgram({"--script", script.string(), "--load", snapshot.string()}, input, output,
                       errors) == 0);
    REQUIRE(errors.str().empty());
    application.execute("load " + path + " no");
    REQUIRE(application.state() == before);
    std::istringstream eof;
    application.run(eof, true);
    REQUIRE(output.str().find("> ") != std::string::npos);
    std::filesystem::remove(snapshot);
    std::filesystem::remove(script);
    std::filesystem::remove(directory);
}

TEST_CASE("Console unknown actions malformed arguments and role violations do not mutate store") {
    std::ostringstream output;
    ConsoleApplication application(makeDemoStore(), output);
    const auto initial = application.state();
    const std::vector<std::string> invalid{"as other",
                                           "as customer unknown",
                                           "quit extra",
                                           "clock other",
                                           "category other",
                                           "product other shirt",
                                           "variant other SHIRT-BLUE",
                                           "price other SHIRT-BLUE 100",
                                           "promotion other",
                                           "coupon other WELCOME",
                                           "stock other",
                                           "receipt other",
                                           "customer other alice",
                                           "catalog other",
                                           "catalog variants shirt bad",
                                           "product attribute shirt x nonsense yes",
                                           "variant add shirt S Label 1 1 1 1 color",
                                           "promotion add bad B nonsense 1 0 0 10000 yes",
                                           "coupon usage WELCOME",
                                           "help extra",
                                           "fulfill other order-1 op",
                                           "fulfill show missing",
                                           "report 0 1001"};
    for (const auto& command : invalid) {
        const auto errors = application.errorCount();
        application.execute(command);
        INFO(command);
        REQUIRE(application.errorCount() == errors + 1);
        REQUIRE(application.state() == initial);
    }
    application.execute("as customer alice");
    const std::vector<std::string> buyerInvalid{"address other home",
                                                "wish other",
                                                "cart other",
                                                "order other x op",
                                                "return other x op",
                                                "review other",
                                                "order delivery-confirm op",
                                                "order list wrong",
                                                "order pay missing 1 cash yes op",
                                                "return open missing op S 1 bad",
                                                "review list shirt bad",
                                                "review add missing shirt 99 x",
                                                "review edit missing 0 x",
                                                "save file",
                                                "load file replace",
                                                "return show missing"};
    for (const auto& command : buyerInvalid) {
        const auto errors = application.errorCount();
        application.execute(command);
        INFO(command);
        REQUIRE(application.errorCount() == errors + 1);
        REQUIRE(application.state() == initial);
    }
    REQUIRE_THROWS_AS(InputReader::tokenize(std::string(8193, 'x')), InputError);
    REQUIRE_THROWS_AS(InputReader::tokenize(std::string(4097, 'x')), InputError);
    REQUIRE_THROWS_AS(InputReader::tokenize("\"x\\"), InputError);
    REQUIRE(InputReader::tokenize("\"a\\\"b\"")[0] == "a\"b");
    REQUIRE_THROWS_AS(InputReader::integer(""), InputError);
}
