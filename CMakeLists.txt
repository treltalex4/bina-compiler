# Минимальная версия CMake, поддерживающая C++23
cmake_minimum_required(VERSION 3.30)

set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "451f2fe2-a8a2-47c3-bc32-94786d8fc91b")

# Использовать libc++ вместо libstdc++ (нужно для import std)
set(CMAKE_CXX_FLAGS_INIT "-stdlib=libc++")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-stdlib=libc++")

# Имя проекта и язык
project(bina LANGUAGES CXX)

# Стандарт C++23 — обязателен по ТЗ
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_SCAN_FOR_MODULES ON)
set(CMAKE_CXX_MODULE_STD ON)

add_executable(bina src/main.cpp)

target_sources(bina
    PUBLIC
    FILE_SET CXX_MODULES FILES
        src/lexer/token.cppm
        src/lexer/lexer.cppm
        src/parser/ast.cppm
        src/parser/parser.cppm
)

target_sources(bina PRIVATE
    src/lexer/token.cpp
    src/lexer/lexer.cpp
    src/parser/ast.cpp
    src/parser/parser.cpp
)

target_compile_options(bina PRIVATE -Wall -Wextra -Wpedantic)

# make run — собрать и запустить
add_custom_target(run
    COMMAND $<TARGET_FILE:bina>
    DEPENDS bina
    COMMENT "Запуск компилятора Bina"
)

# make debug — собрать с отладочной информацией и запустить в gdb
add_custom_target(debug
    COMMAND ${CMAKE_COMMAND} -E env CMAKE_BUILD_TYPE=Debug ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target bina
    COMMAND gdb $<TARGET_FILE:bina>
    COMMENT "Отладка компилятора Bina"
)
