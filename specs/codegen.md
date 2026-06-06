# Bina Codegen

## 1. Подход

Компилятор генерирует текстовый LLVM IR (`.ll`), затем вызывает `llc` для
получения объектного файла и системный C-компилятор (`cc`) для линковки с
минимальным C-runtime.

Проект не использует `libLLVM` как библиотечную зависимость: LLVM выступает как
внешний toolchain.

## 2. Pipeline

```text
source.bina
  -> lexer
  -> parser
  -> semantic analyzer
  -> textual LLVM IR
  -> llc -filetype=obj
  -> cc + bina_runtime.o
  -> executable
```

Флаги `--dump-tokens`, `--dump-ast`, `--dump-symbols` завершают работу на
соответствующей frontend-фазе.

## 3. Маппинг типов

| Bina | LLVM IR |
|---|---|
| `int8`, `uint8` | `i8` |
| `int16`, `uint16` | `i16` |
| `int32`, `uint32` | `i32` |
| `int64`, `uint64`, `int` | `i64` |
| `float32` | `float` |
| `float64` | `double` |
| `bool` | `i1` в SSA и сигнатурах, `i8` в памяти |
| `char` | `i32` |
| `string` | `{ ptr, i64 }` |
| `void` | `void` |
| `[N]T` | `[N x storage(T)]` |
| `struct X` | `%struct.<mangled-qualified-name>` |

LLVM IR использует opaque pointers: все указатели записываются как `ptr`.

## 4. Layout

Строка хранится как дескриптор `{ ptr, i64 }`, где `ptr` указывает на UTF-8
байты, а `i64` хранит длину в байтах.

Массив `[N]T` имеет фиксированный compile-time размер и хранится как LLVM array.
Если `T = bool`, элементы хранятся как `i8`.

Структура хранит поля в порядке объявления. Если поле имеет тип `bool`, оно
хранится как `i8`.

## 5. Calling Convention

Все значения Bina передаются по значению. У методов параметр `self` является
обычным первым параметром.

Глобальная функция `main() -> int` эмитится как `i32 @main()`. Значение Bina
типа `int` (`int64`) перед возвратом из `main` сужается до `i32`.

## 6. Mangling

Глобальный `main` получает имя `@main`.

Все остальные функции получают имя:

```text
@bina.<namespace-or-owner>.<name>$<param-types>
```

Примеры:

```text
fn add(a: int32, b: int32) -> int32
  -> @bina.add$i32_i32

namespace Math { fn abs(x: int) -> int }
  -> @bina.Math.abs$i64

impl Point { fn len(self: Point) -> float64 }
  -> @bina.Point.len$S.Point
```

Типы параметров в mangling:

| Bina | Mangled |
|---|---|
| `int8`..`int64` | `i8`..`i64` |
| `uint8`..`uint64` | `u8`..`u64` |
| `float32`, `float64` | `f32`, `f64` |
| `bool` | `b` |
| `char` | `c` |
| `string` | `s` |
| `[N]T` | `aN.<T>` |
| `struct A::B` | `S.A.B` |

## 7. Lowering

`let`/`mut` создают `alloca` в текущей функции и сохраняют значение через
`store`. Чтение переменной делает `load`.

`if`, `while`, `break`, `continue` эмитятся через basic blocks и `br`.

`&&` и `||` обязаны быть short-circuit операциями и эмитятся через `phi`.

Целочисленные `+`, `-`, `*` проверяют overflow через LLVM overflow intrinsics.
Целочисленные `/` и `%` проверяют деление на ноль, а signed division дополнительно
проверяет `INT_MIN / -1`.

Индексация массива всегда выполняет runtime bounds check.

Равенство массивов и структур сравнивает элементы/поля рекурсивно.

## 8. Runtime

Runtime предоставляет:

```text
bina_print_i64, bina_print_u64, bina_print_f64, bina_print_bool
bina_print_char, bina_print_str
bina_input
bina_assert, bina_panic, bina_exit
bina_div_zero, bina_index_oob, bina_int_overflow
bina_char_from
bina_to_string_i64, bina_to_string_u64, bina_to_string_f64
bina_parse_i64, bina_parse_u64, bina_parse_f64
bina_str_concat, bina_str_eq
```

## 9. Runtime Errors

Все runtime errors пишутся в `stderr` и завершают процесс с exit code `1`.

Форматы:

```text
runtime error: division by zero at line N
runtime error: index out of bounds: index I, length L at line N
runtime error: integer overflow at line N
runtime error: invalid numeric conversion at line N
runtime error: assertion failed at line N
runtime error: invalid character code at line N
runtime error: <message> at line N
```

Последняя форма используется для `panic`.

## 10. Управление памятью

Значения языка имеют value semantics.

Строки, созданные runtime-функциями, выделяются через `malloc` и в текущей версии
не освобождаются. Это допустимое ограничение учебного runtime.

Локальные массивы и структуры размещаются через `alloca`.

## 11. CLI

```text
bina [--dump-tokens] [--dump-ast] [--dump-symbols] [--emit-ir] [--no-link]
     [-o <output>] <source.bina>
```

`--emit-ir` печатает LLVM IR в stdout и не вызывает `llc`.

`--no-link` вызывает `llc`, но не вызывает `cc`; результатом является `.o`.

`-o` задает путь к executable или к object-файлу при `--no-link`.

## 12. Сборка Runtime

CMake собирает `runtime/bina_runtime.c` как object target и копирует
`bina_runtime.o` рядом с бинарником `bina`.

Driver ищет runtime object в таком порядке:

1. рядом с исполняемым файлом `bina`;
2. в переменной окружения `BINA_RUNTIME`;
3. как `./bina_runtime.o`.

Пути к инструментам можно переопределить через `BINA_LLC` и `BINA_CC`.

## 13. Ограничения

Пока не генерируется debug info.

Runtime не освобождает heap-строки.

Оптимизации не являются частью спецификации codegen: IR должен быть корректным
без отдельного `opt`.
