# CMakeLists.txt - minimal build file for the Student Records project
cmake_minimum_required(VERSION 3.10)
project(StudentRecords CXX)

# std::filesystem needs C++17 or newer
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(student_manager main.cpp record_manager.cpp)

# Show as many helpful warnings as possible while learning
target_compile_options(student_manager PRIVATE -Wall -Wextra)

# Very old GCC versions (before 9.1) keep std::filesystem in a separate library.
# Modern compilers skip this block automatically.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.1)
    target_link_libraries(student_manager PRIVATE stdc++fs)
endif()
