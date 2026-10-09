 add_library(rubiks_cube_core
       rubiks_cube/src/RubiksCube.cpp
   )

   target_include_directories(rubiks_cube_core PUBLIC
       rubiks_cube/include
   )

   add_executable(rubiks_cube
       rubiks_cube/app/main.cpp
   )

   target_link_libraries(rubiks_cube PRIVATE rubiks_cube_core)

   add_executable(post_machine
       post_machine/app/main.cpp
   )
