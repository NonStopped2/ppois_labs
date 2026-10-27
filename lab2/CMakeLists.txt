add_library(post_domain
    src/core/Money.cpp
    src/core/Dimensions.cpp
    src/core/PostalAddress.cpp
    src/core/ContactDetails.cpp
    src/core/Customer.cpp
    src/core/IdentityDocument.cpp
    src/shipments/ContentItem.cpp
    src/shipments/DeliveryOptions.cpp
    src/shipments/Shipment.cpp
    src/shipments/Letter.cpp
    src/shipments/Parcel.cpp
    src/registry/ShipmentRegistry.cpp
    src/payments/InvoiceLine.cpp
    src/tariffs/TariffZone.cpp
    src/tariffs/WeightBand.cpp
    src/tariffs/ServiceRate.cpp
    src/tariffs/TariffPlan.cpp
    src/tariffs/PostageQuote.cpp
    src/tariffs/PostageCalculator.cpp
    src/payments/Payment.cpp
    src/payments/Refund.cpp
    src/payments/Invoice.cpp
    src/storage/StoragePolicy.cpp
    src/storage/StorageEntry.cpp
    src/storage/StorageCell.cpp
    src/storage/StorageArea.cpp
    src/transport/RouteStop.cpp
    src/transport/PostalRoute.cpp
    src/personnel/Employee.cpp
    src/personnel/WorkShift.cpp
    src/sorting/SortingRule.cpp
    src/sorting/SortingPlan.cpp
    src/delivery/PickupAuthorization.cpp
    src/delivery/DeliveryAttempt.cpp
    src/sorting/MailBag.cpp
    src/sorting/DispatchBatch.cpp
    src/branches/PostalBranch.cpp
    src/returns/ReturnRequest.cpp
    src/tracking/TrackingEvent.cpp
    src/tracking/TrackingHistory.cpp
    src/detail/Text.cpp
)
target_include_directories(post_domain PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(post_domain PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(post_domain PRIVATE /W4)
else()
    target_compile_options(post_domain PRIVATE -Wall -Wextra -Wpedantic)
endif()

option(LAB2_ENABLE_COVERAGE "Instrument lab2 with Clang/LLVM coverage" OFF)
option(LAB2_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer for lab2" OFF)

if(LAB2_ENABLE_COVERAGE)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "LAB2_ENABLE_COVERAGE requires Clang or AppleClang")
    endif()
    # PUBLIC propagates instrumentation to lab2 consumers, but not to Catch2 itself.
    target_compile_options(post_domain PUBLIC -fprofile-instr-generate -fcoverage-mapping)
    target_link_options(post_domain PUBLIC -fprofile-instr-generate)
endif()

if(LAB2_ENABLE_SANITIZERS)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        message(FATAL_ERROR "LAB2_ENABLE_SANITIZERS requires Clang or GCC")
    endif()
    target_compile_options(post_domain PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(post_domain PUBLIC -fsanitize=address,undefined)
endif()

if(BUILD_TESTING)
    include(FetchContent)
    if(POLICY CMP0135)
        cmake_policy(SET CMP0135 NEW)
    endif()
    FetchContent_Declare(Catch2
        URL https://codeload.github.com/catchorg/Catch2/tar.gz/refs/tags/v3.8.1
        URL_HASH SHA256=18b3f70ac80fccc340d8c6ff0f339b2ae64944782f8d2fca2bd705cf47cadb79
    )
    FetchContent_MakeAvailable(Catch2)

    add_executable(post_core_tests
        tests/core/MoneyTests.cpp
        tests/core/DimensionsTests.cpp
        tests/core/PostalAddressTests.cpp
        tests/core/ContactDetailsTests.cpp
        tests/core/CustomerTests.cpp
        tests/core/IdentityDocumentTests.cpp
        tests/shipments/ContentItemTests.cpp
        tests/shipments/DeliveryOptionsTests.cpp
        tests/shipments/ShipmentTests.cpp
        tests/shipments/LetterTests.cpp
        tests/shipments/ParcelTests.cpp
        tests/registry/ShipmentRegistryTests.cpp
        tests/payments/InvoiceLineTests.cpp
        tests/tariffs/TariffZoneTests.cpp
        tests/tariffs/WeightBandTests.cpp
        tests/tariffs/ServiceRateTests.cpp
        tests/tariffs/TariffPlanTests.cpp
        tests/tariffs/PostageQuoteTests.cpp
        tests/tariffs/PostageCalculatorTests.cpp
        tests/payments/PaymentTests.cpp
        tests/payments/RefundTests.cpp
        tests/payments/InvoiceTests.cpp
        tests/storage/StoragePolicyTests.cpp
        tests/storage/StorageEntryTests.cpp
        tests/storage/StorageCellTests.cpp
        tests/storage/StorageAreaTests.cpp
        tests/transport/RouteStopTests.cpp
        tests/transport/PostalRouteTests.cpp
        tests/personnel/EmployeeTests.cpp
        tests/personnel/WorkShiftTests.cpp
        tests/sorting/SortingRuleTests.cpp
        tests/sorting/SortingPlanTests.cpp
        tests/delivery/PickupAuthorizationTests.cpp
        tests/delivery/DeliveryAttemptTests.cpp
        tests/sorting/MailBagTests.cpp
        tests/sorting/DispatchBatchTests.cpp
        tests/branches/PostalBranchTests.cpp
        tests/returns/ReturnRequestTests.cpp
        tests/tracking/TrackingEventTests.cpp
        tests/tracking/TrackingHistoryTests.cpp
    )
    target_link_libraries(post_core_tests PRIVATE post_domain Catch2::Catch2WithMain)

    list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
    include(Catch)
    catch_discover_tests(post_core_tests TEST_PREFIX "lab2." PROPERTIES LABELS "lab2")
endif()
