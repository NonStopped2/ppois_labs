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
)
target_include_directories(shop_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(shop_core PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(shop_core PRIVATE /W4)
else()
    target_compile_options(shop_core PRIVATE -Wall -Wextra -Wpedantic)
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
        tests/application/StoreStateTests.cpp)
    target_link_libraries(shop_core_tests PRIVATE shop_core Catch2::Catch2WithMain)
    list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
    include(Catch)
    catch_discover_tests(shop_core_tests TEST_PREFIX "lab3." PROPERTIES LABELS "lab3-unit")
    find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
    add_test(NAME lab3-tools.coverage
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/test_coverage.py")
    set_tests_properties(lab3-tools.coverage PROPERTIES LABELS "lab3-tools")
endif()
