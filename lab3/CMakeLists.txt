add_library(shop_core
    src/common/Money.cpp
    src/common/Address.cpp
    src/common/ContactDetails.cpp
    src/common/TimeWindow.cpp
    src/catalog/Category.cpp
    src/catalog/ProductAttribute.cpp
    src/catalog/ProductVariant.cpp
    src/catalog/Product.cpp
    src/catalog/ProductCatalog.cpp
    src/customers/Customer.cpp
    src/customers/AddressBook.cpp
    src/customers/Wishlist.cpp
    src/customers/LoyaltyAccount.cpp
    src/customers/LoyaltyPolicy.cpp
    src/pricing/PriceEntry.cpp
    src/pricing/PriceList.cpp
    src/pricing/DiscountAllocation.cpp
    src/pricing/QuoteLine.cpp
    src/pricing/Promotion.cpp
    src/pricing/Coupon.cpp
    src/pricing/CouponBook.cpp
    src/inventory/Warehouse.cpp
    src/inventory/StockItem.cpp
    src/inventory/StockReservation.cpp
    src/inventory/ReceiptLine.cpp
    src/inventory/StockReceipt.cpp
    src/inventory/Inventory.cpp
    src/cart/CartLine.cpp
    src/cart/Cart.cpp
    src/returns/ReturnPolicy.cpp
    src/delivery/DeliveryOption.cpp
    src/delivery/DeliveryPolicy.cpp
    src/pricing/PriceQuote.cpp
    src/pricing/PricingPolicy.cpp
    src/orders/OrderLine.cpp
    src/orders/OrderEvent.cpp
    src/orders/Order.cpp
    src/orders/OrderBook.cpp
    src/payments/PaymentAttempt.cpp
    src/payments/Payment.cpp
    src/payments/Refund.cpp
    src/payments/PaymentBook.cpp
    src/delivery/Fulfillment.cpp
    src/application/OperationRecord.cpp
    src/application/StoreState.cpp
    src/application/Checkout.cpp
    src/application/OrderProcessing.cpp
    src/returns/ReturnLine.cpp
    src/returns/InspectionResult.cpp
    src/returns/ReturnRequest.cpp
    src/returns/ReturnInspection.cpp
    src/application/ReturnProcessing.cpp
    src/catalog/ProductReview.cpp
    src/catalog/ReviewBook.cpp
    src/catalog/ProductComparison.cpp
    src/reporting/SalesReport.cpp
    src/reporting/CustomerAnalysis.cpp
    src/reporting/AssortmentAnalysis.cpp
    src/reporting/Reconciliation.cpp
)
target_include_directories(shop_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(shop_core PUBLIC cxx_std_20)
include(FetchContent)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()
FetchContent_Declare(nlohmann_json
    URL https://codeload.github.com/nlohmann/json/tar.gz/refs/tags/v3.12.0
    URL_HASH SHA256=4b92eb0c06d10683f7447ce9406cb97cd4b453be18d7279320f7b2f025c10187)
set(JSON_BuildTests OFF CACHE BOOL "Do not build third-party JSON tests" FORCE)
FetchContent_MakeAvailable(nlohmann_json)
add_library(shop_persistence
    persistence/SnapshotValues.cpp persistence/SnapshotEvents.cpp
    persistence/SnapshotCatalog.cpp persistence/SnapshotCustomers.cpp
    persistence/SnapshotInventory.cpp persistence/SnapshotOrders.cpp
    persistence/SnapshotOperations.cpp persistence/SnapshotPayments.cpp
    persistence/SnapshotReturns.cpp persistence/SnapshotCodec.cpp
    persistence/SnapshotValidation.cpp persistence/SnapshotDocumentValidation.cpp
    persistence/SnapshotOperationValidation.cpp persistence/SnapshotStore.cpp)
target_include_directories(shop_persistence PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/persistence")
target_link_libraries(shop_persistence PUBLIC shop_core nlohmann_json::nlohmann_json)
add_library(shop_ui
    app/InputReader.cpp app/DemoData.cpp app/ConsoleApplication.cpp app/Formatting.cpp app/Help.cpp app/RunProgram.cpp
    app/commands/CatalogCommands.cpp app/commands/CustomerCommands.cpp app/commands/OrderCommands.cpp
    app/commands/ReturnCommands.cpp app/commands/ReviewCommands.cpp app/commands/ReportCommands.cpp)
target_include_directories(shop_ui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/app")
target_link_libraries(shop_ui PUBLIC shop_persistence)
add_executable(shop_cli app/main.cpp)
target_link_libraries(shop_cli PRIVATE shop_ui)
if(MSVC)
    target_compile_options(shop_core PRIVATE /W4)
else()
    target_compile_options(shop_core PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(shop_persistence PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(shop_ui PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(shop_cli PRIVATE -Wall -Wextra -Wpedantic)
endif()

option(LAB3_ENABLE_COVERAGE "Instrument the online store with LLVM coverage" OFF)
option(LAB3_ENABLE_SANITIZERS "Check the online store with ASan and UBSan" OFF)
if(LAB3_ENABLE_COVERAGE)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "LAB3_ENABLE_COVERAGE requires Clang or AppleClang")
    endif()
    target_compile_options(shop_core PUBLIC -fprofile-instr-generate -fcoverage-mapping)
    target_link_options(shop_core PUBLIC -fprofile-instr-generate)
endif()
if(LAB3_ENABLE_SANITIZERS)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        message(FATAL_ERROR "LAB3_ENABLE_SANITIZERS requires Clang or GCC")
    endif()
    target_compile_options(shop_core PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(shop_core PUBLIC -fsanitize=address,undefined)
endif()

if(BUILD_TESTING)
    include(FetchContent)
    if(POLICY CMP0135)
        cmake_policy(SET CMP0135 NEW)
    endif()
    FetchContent_Declare(Catch2
        URL https://codeload.github.com/catchorg/Catch2/tar.gz/refs/tags/v3.8.1
        URL_HASH SHA256=18b3f70ac80fccc340d8c6ff0f339b2ae64944782f8d2fca2bd705cf47cadb79)
    FetchContent_MakeAvailable(Catch2)
    add_executable(shop_core_tests
        tests/common/ValueTests.cpp
        tests/catalog/CatalogTests.cpp
        tests/customers/CustomerTests.cpp
        tests/pricing/PricingTests.cpp
        tests/pricing/PromotionTests.cpp
        tests/inventory/InventoryTests.cpp
        tests/cart/CartTests.cpp
        tests/pricing/QuoteTests.cpp
        tests/orders/OrderTests.cpp
        tests/payments/PaymentTests.cpp
        tests/delivery/FulfillmentTests.cpp
        tests/application/CheckoutTests.cpp
        tests/application/ProcessingTests.cpp
        tests/application/StoreStateTests.cpp
        tests/returns/ReturnTests.cpp
        tests/application/ReturnProcessingTests.cpp
        tests/catalog/ReviewTests.cpp
        tests/reporting/ReportTests.cpp)
    target_link_libraries(shop_core_tests PRIVATE shop_core Catch2::Catch2WithMain)
    add_executable(shop_persistence_tests tests/persistence/ValueCodecTests.cpp tests/persistence/SnapshotTests.cpp)
    target_link_libraries(shop_persistence_tests PRIVATE shop_persistence nlohmann_json::nlohmann_json Catch2::Catch2WithMain)
    add_executable(shop_ui_tests tests/ui/ConsoleTests.cpp)
    target_link_libraries(shop_ui_tests PRIVATE shop_ui Catch2::Catch2WithMain)
    list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
    include(Catch)
    catch_discover_tests(shop_core_tests TEST_PREFIX "lab3." PROPERTIES LABELS "lab3-unit")
    catch_discover_tests(shop_persistence_tests TEST_PREFIX "lab3-json." PROPERTIES LABELS "lab3-persistence")
    catch_discover_tests(shop_ui_tests TEST_PREFIX "lab3-ui." PROPERTIES LABELS "lab3-ui")
    find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
    add_test(NAME lab3-tools.coverage
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/test_coverage.py")
    set_tests_properties(lab3-tools.coverage PROPERTIES LABELS "lab3-tools")
    add_test(NAME lab3-cli
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/cli_tests.py" "$<TARGET_FILE:shop_cli>")
    set_tests_properties(lab3-cli PROPERTIES LABELS "lab3-cli" TIMEOUT 120)
endif()
