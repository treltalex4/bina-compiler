# Bina Language Grammar (EBNF)

## Нотация

```
=       определение правила
;       конец правила
|       альтернатива (или)
[ ... ] ноль или один раз (опционально)
{ ... } ноль или более раз (повторение)
( ... ) группировка
"..."   терминал (литеральный токен)
```

Имена правил в `snake_case` — нетерминалы.
Имена в `UPPER_CASE` — лексические токены, определённые в разделе «Лексика».

---

## Лексика

### Алфавит

Исходный файл — текст в кодировке UTF-8.
Идентификаторы и ключевые слова состоят из символов ASCII.
Строковые литералы могут содержать произвольные Unicode-символы.

### Пробельные символы и комментарии

```ebnf
whitespace = " " | "\t" | "\r" | "\n" ;

comment    = "//" { any_char_except_newline } "\n" ;
```

Пробельные символы и комментарии игнорируются лексером и в поток токенов не передаются.

### Ключевые слова

Следующие слова зарезервированы и не могут использоваться как идентификаторы:

```
fn       let      mut      return   if       else
while    break    continue struct   namespace type
cast     true     false    print    input     exit
panic    len      void
```

### Идентификаторы

```ebnf
IDENT = ( LETTER | "_" ) { LETTER | DIGIT | "_" } ;

LETTER = "a".."z" | "A".."Z" ;
DIGIT  = "0".."9" ;
```

Идентификатор не может совпадать ни с одним ключевым словом.

### Числовые литералы

```ebnf
INT_LIT   = DIGIT { DIGIT } [ INT_SUFFIX ] ;
FLOAT_LIT = DIGIT { DIGIT } "." DIGIT { DIGIT } [ FLOAT_SUFFIX ] ;

INT_SUFFIX   = "i8" | "i16" | "i32" | "i64"
             | "u8" | "u16" | "u32" | "u64" ;

FLOAT_SUFFIX = "f32" | "f64" ;
```

Примеры: `42`, `42i32`, `3.14`, `3.14f64`, `255u8`.

Литерал без суффикса имеет тип, выводимый из контекста;
при невозможности вывода — `int32` для целых, `float64` для вещественных.

### Строковые литералы

```ebnf
STRING_LIT = '"' { string_char } '"' ;

string_char = any_unicode_char_except_backslash_and_quote
            | escape_seq ;

escape_seq  = "\\" ( "n" | "t" | "r" | '"' | "\\" ) ;
```

Строки хранятся в UTF-8. Поддерживаемые escape-последовательности:
`\n`, `\t`, `\r`, `\"`, `\\`.

### Булевы литералы

```ebnf
BOOL_LIT = "true" | "false" ;
```

### Операторы и разделители

```
+   -   *   /   %          арифметика (+ также конкатенация строк)
==  !=  <   >   <=  >=     сравнения
&&  ||  !                  логика
=                          присваивание
->                         тип возврата функции
::                         доступ к пространству имён
.                          доступ к полю структуры
,   ;   :                  разделители
(   )   {   }   [   ]      скобки
<   >                      параметры cast (в контексте cast<type>)
```

---

## Синтаксис

### Программа

```ebnf
program = { top_level_decl } ;

top_level_decl = fn_decl
               | struct_decl
               | namespace_decl
               | type_alias_decl ;
```

Выражения и инструкции допускаются только внутри тела функции.
На верхнем уровне — только объявления.

---

### Объявления верхнего уровня

#### Функция

```ebnf
fn_decl = "fn" IDENT "(" [ param_list ] ")" "->" type_expr block ;

param_list = param { "," param } ;

param = IDENT ":" type_expr ;
```

Точка входа — функция с именем `main` и типом возврата `int`:

```
fn main() -> int { ... }
```

#### Структура

```ebnf
struct_decl = "struct" IDENT "{" { struct_field } "}" ;

struct_field = IDENT ":" type_expr ";" ;
```

#### Пространство имён

```ebnf
namespace_decl = "namespace" IDENT "{" { top_level_decl } "}" ;
```

Пространства имён могут быть вложенными.

#### Синоним типа

```ebnf
type_alias_decl = "type" IDENT "=" type_expr ";" ;
```

Пример: `type Meters = int32;`

---

### Типы

```ebnf
type_expr = builtin_type
          | array_type
          | qualified_name
          | "void" ;

builtin_type = "int8"  | "int16"  | "int32"  | "int64"
             | "uint8" | "uint16" | "uint32" | "uint64"
             | "float32" | "float64"
             | "bool"
             | "string"
             | "int" ;

array_type = "[" INT_LIT "]" type_expr ;

qualified_name = IDENT { "::" IDENT } ;
```

Размер массива — часть типа. `[5]int32` и `[6]int32` — разные типы.
`int` — псевдоним для `int64`, используется как тип возврата `main`.

---

### Инструкции

```ebnf
block = "{" { stmt } "}" ;

stmt = var_decl_stmt
     | assign_stmt
     | if_stmt
     | while_stmt
     | return_stmt
     | break_stmt
     | continue_stmt
     | expr_stmt
     | null_stmt ;
```

#### Объявление переменной

```ebnf
var_decl_stmt = ( "let" | "mut" ) IDENT ":" type_expr "=" expr ";" ;
```

- `let` — иммутабельная переменная (присваивание после объявления запрещено)
- `mut` — мутабельная переменная

Начальное значение обязательно — использование неинициализированной переменной
является ошибкой компиляции.

#### Присваивание

```ebnf
assign_stmt = lvalue "=" expr ";" ;

lvalue = qualified_name { lvalue_suffix } ;

lvalue_suffix = "[" expr "]"
              | "." IDENT ;
```

Присваивание разрешено только мутабельным переменным (`mut`).

#### Ветвление

```ebnf
if_stmt = "if" expr block [ "else" ( block | if_stmt ) ] ;
```

Ветка `else` необязательна. Допускается цепочка `else if`.

#### Цикл

```ebnf
while_stmt = "while" expr block ;

break_stmt    = "break" ";" ;
continue_stmt = "continue" ";" ;
```

`break` и `continue` допустимы только внутри тела цикла `while`.

#### Возврат из функции

```ebnf
return_stmt = "return" [ expr ] ";" ;
```

В функции с типом возврата `void` — `return;` без значения.
В остальных функциях — `return expr;`.

#### Инструкция-выражение

```ebnf
expr_stmt = expr ";" ;
```

Используется для вызовов функций: `print(x);`

#### Нулевая инструкция

```ebnf
null_stmt = ";" ;
```

Пустая инструкция без эффекта.

---

### Выражения

```ebnf
expr = or_expr ;
```

#### Приоритет операторов (от низшего к высшему)

| Уровень | Операторы          | Ассоциативность |
|---------|--------------------|-----------------|
| 1       | `\|\|`             | левая           |
| 2       | `&&`               | левая           |
| 3       | `== != < > <= >=`  | нет (не цепочка)|
| 4       | `+ -`              | левая           |
| 5       | `* / %`            | левая           |
| 6       | унарные `- !`      | правая          |
| 7       | постфиксные `[] .` | левая           |
| 8       | первичные          | —               |

```ebnf
or_expr    = and_expr     { "||"  and_expr } ;
and_expr   = cmp_expr     { "&&"  cmp_expr } ;
cmp_expr   = add_expr     [ cmp_op add_expr ] ;
add_expr   = mul_expr     { add_op mul_expr } ;
mul_expr   = unary_expr   { mul_op unary_expr } ;
unary_expr = unary_op unary_expr
           | postfix_expr ;
postfix_expr = primary_expr { postfix_op } ;

cmp_op   = "==" | "!=" | "<" | ">" | "<=" | ">=" ;
add_op   = "+" | "-" ;
mul_op   = "*" | "/" | "%" ;
unary_op = "-" | "!" ;

postfix_op = "[" expr "]"          (* индексирование массива *)
           | "." IDENT             (* доступ к полю структуры *) ;
```

#### Первичные выражения

```ebnf
primary_expr = INT_LIT
             | FLOAT_LIT
             | STRING_LIT
             | BOOL_LIT
             | call_expr
             | cast_expr
             | struct_literal
             | array_literal
             | qualified_name
             | "(" expr ")" ;
```

#### Вызов функции

```ebnf
call_expr = qualified_name "(" [ arg_list ] ")" ;

arg_list = expr { "," expr } ;
```

#### Явное приведение типа

```ebnf
cast_expr = "cast" "<" type_expr ">" "(" expr ")" ;
```

Пример: `cast<float64>(count)`

#### Литерал структуры

```ebnf
struct_literal = qualified_name "{" [ field_init { "," field_init } [ "," ] ] "}" ;

field_init = IDENT ":" expr ;
```

Пример: `Point { x: 1.0, y: 2.0 }`

#### Литерал массива

```ebnf
array_literal = "[" [ expr { "," expr } [ "," ] ] "]" ;
```

Пример: `[1, 2, 3, 4, 5]`

Количество элементов должно совпадать с размером типа массива.

#### Квалифицированное имя

```ebnf
qualified_name = IDENT { "::" IDENT } ;
```

Используется для обращения к элементам пространства имён: `Stats::Student`, `Math::PI`.

---

### Встроенные функции

Встроенные функции вызываются как обычные, без квалификатора пространства имён.

| Вызов                 | Описание                                      |
|-----------------------|-----------------------------------------------|
| `print(expr)`         | Вывод значения в stdout                       |
| `input()`             | Чтение строки из stdin, возвращает `string`   |
| `len(expr)`           | Длина строки или массива, возвращает `int64`  |
| `exit(expr)`          | Завершение программы с кодом возврата         |
| `panic(string_expr)`  | Аварийное завершение с сообщением в stderr    |

---

## Примеры

### Минимальная программа

```bina
fn main() -> int {
    print("hello, world");
    return 0;
}
```

### Функция и цикл

```bina
fn sum(arr: [5]int32) -> int32 {
    mut acc: int32 = 0;
    mut i  : int32 = 0;
    while i < 5 {
        acc = acc + arr[i];
        i   = i + 1;
    }
    return acc;
}

fn main() -> int {
    let nums: [5]int32 = [10, 20, 30, 40, 50];
    let result: int32  = sum(nums);
    print(result);
    return 0;
}
```

### Структура и пространство имён

```bina
type Celsius    = float64;
type Fahrenheit = float64;

namespace Temp {
    struct Reading {
        value : Celsius;
        label : string;
    }

    fn to_fahrenheit(c: Celsius) -> Fahrenheit {
        return c * 1.8 + 32.0;
    }
}

fn main() -> int {
    let r: Temp::Reading = Temp::Reading {
        value: 36.6,
        label: "body temp",
    };

    let f: Fahrenheit = Temp::to_fahrenheit(r.value);

    if f > 100.0 {
        print("fever");
    } else {
        print("normal");
    }

    return 0;
}
```

### Явное приведение типа

```bina
fn average(a: int32, b: int32) -> float64 {
    let sum: int32   = a + b;
    let avg: float64 = cast<float64>(sum) / 2.0;
    return avg;
}
```

### Строковые операции

```bina
fn main() -> int {
    let greeting: string = "hello" + " " + "world";
    let length: int64 = len(greeting);
    print(greeting);
    print(length);

    if greeting == "hello world" {
        print("match");
    }

    return 0;
}
```
