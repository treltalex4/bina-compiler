# Минимальная версия CMake, поддерживающая C++23
cmake_minimum_required(VERSION 3.30)

set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "451f2fe2-a8a2-47c3-bc32-94786d8fc91b")

# Использовать libc++ вместо libstdc++ (нужно для import std)
set(CMAKE_CXX_FLAGS_INIT "-stdlib=libc++")

# Имя проекта и языки
project(bina LANGUAGES C CXX)

# Стандарт C++23 — обязателен по ТЗ
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_SCAN_FOR_MODULES ON)
set(CMAKE_CXX_MODULE_STD ON)

add_executable(bina src/main.cpp)

add_library(bina_runtime OBJECT runtime/bina_runtime.c)
target_compile_options(bina_runtime PRIVATE -O2 -Wall -Wextra)
set_target_properties(bina_runtime PROPERTIES C_STANDARD 99)
add_dependencies(bina bina_runtime)

target_sources(bina
    PUBLIC
    FILE_SET CXX_MODULES FILES
        src/lexer/token.cppm
        src/lexer/lexer.cppm
        src/parser/ast.cppm
        src/parser/parser.cppm
        src/semantic/semantic.cppm
        src/semantic/scope.cppm
        src/semantic/symbol.cppm
        src/semantic/type.cppm
        src/semantic/overload.cppm
        src/codegen/codegen.cppm
        src/driver/driver.cppm
)

target_sources(bina PRIVATE
    src/lexer/token.cpp
    src/lexer/lexer.cpp
    src/parser/ast.cpp
    src/parser/parser.cpp
    src/semantic/semantic.cpp
    src/semantic/scope.cpp
    src/semantic/type.cpp
    src/semantic/overload.cpp
    src/codegen/codegen.cpp
    src/driver/driver.cpp
)

target_compile_options(bina PRIVATE -Wall -Wextra -Wpedantic)
target_link_options(bina PRIVATE -stdlib=libc++)

add_custom_command(TARGET bina POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_OBJECTS:bina_runtime>
        ${CMAKE_BINARY_DIR}/bina_runtime.o
)

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
