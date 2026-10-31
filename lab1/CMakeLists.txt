function(configure_first_party target)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    if(ENABLE_COVERAGE)
        target_compile_options(${target} PRIVATE -fprofile-instr-generate -fcoverage-mapping)
        target_link_options(${target} PUBLIC -fprofile-instr-generate)
    endif()
endfunction()

add_library(rubiks_cube_core rubiks_cube/src/RubiksCube.cpp)
target_include_directories(rubiks_cube_core PUBLIC rubiks_cube/include)
configure_first_party(rubiks_cube_core)
add_executable(rubiks_cube rubiks_cube/app/main.cpp)
target_link_libraries(rubiks_cube PRIVATE rubiks_cube_core)
configure_first_party(rubiks_cube)

add_library(post_machine_core
    post_machine/src/Tape.cpp
    post_machine/src/Instruction.cpp
    post_machine/src/Program.cpp
    post_machine/src/PostMachine.cpp)
target_include_directories(post_machine_core PUBLIC post_machine/include)
configure_first_party(post_machine_core)
add_executable(post_machine post_machine/app/main.cpp)
target_link_libraries(post_machine PRIVATE post_machine_core)
configure_first_party(post_machine)

if(BUILD_TESTING)
    include(FetchContent)
    if(POLICY CMP0135)
        cmake_policy(SET CMP0135 NEW)
    endif()
    FetchContent_Declare(Catch2
        URL https://codeload.github.com/catchorg/Catch2/tar.gz/refs/tags/v3.8.1
        URL_HASH SHA256=18b3f70ac80fccc340d8c6ff0f339b2ae64944782f8d2fca2bd705cf47cadb79)
    FetchContent_MakeAvailable(Catch2)
    add_executable(lab1_tests
        rubiks_cube/tests/RubiksCubeTests.cpp
        post_machine/tests/TapeTests.cpp
        post_machine/tests/InstructionTests.cpp
        post_machine/tests/ProgramTests.cpp
        post_machine/tests/PostMachineTests.cpp)
    target_link_libraries(lab1_tests PRIVATE rubiks_cube_core post_machine_core Catch2::Catch2WithMain)
    configure_first_party(lab1_tests)
    list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
    include(Catch)
    catch_discover_tests(lab1_tests)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_test(NAME cli_integration COMMAND "${Python3_EXECUTABLE}"
        "${PROJECT_SOURCE_DIR}/tools/cli_tests.py"
        --cube $<TARGET_FILE:rubiks_cube> --post $<TARGET_FILE:post_machine>
        --examples "${CMAKE_CURRENT_SOURCE_DIR}/examples")
    set_tests_properties(cli_integration PROPERTIES TIMEOUT 90)
endif()

install(TARGETS rubiks_cube post_machine RUNTIME DESTINATION bin)
install(DIRECTORY examples/ DESTINATION share/ppois_lab1/examples)
install(FILES README.md DESTINATION share/ppois_lab1)
