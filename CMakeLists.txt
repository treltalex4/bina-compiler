# Минимальная версия CMake, поддерживающая C++23
cmake_minimum_required(VERSION 3.25)

# Имя проекта и язык
project(bina LANGUAGES CXX)

# Стандарт C++23 — обязателен по ТЗ
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Не использовать расширения компилятора — только стандартный C++
set(CMAKE_CXX_EXTENSIONS OFF)

# Генерировать compile_commands.json — нужен для IDE (подсветка, автодополнение)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Все исходные файлы компилятора
set(BINA_SOURCES
    src/main.cpp
    src/lexer/token.cpp
    src/lexer/lexer.cpp
    src/parser/ast.cpp
    src/parser/parser.cpp
    src/semantic/semantic.cpp
    src/codegen/codegen.cpp
)

# Создаём исполняемый файл "bina" из перечисленных исходников
add_executable(bina ${BINA_SOURCES})

# Говорим компилятору где искать заголовочные файлы (#include)
# PRIVATE = только для этого проекта, не экспортируется
target_include_directories(bina PRIVATE ${CMAKE_SOURCE_DIR}/inc)

# Предупреждения компилятора — помогают находить ошибки заранее
# -Wall    = основные предупреждения
# -Wextra  = дополнительные предупреждения
# -Wpedantic = строгое соответствие стандарту
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
