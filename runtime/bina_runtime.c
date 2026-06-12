#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

typedef struct {
    char* ptr;
    int64_t len;
} bina_str;

#define DIE(fmt, ...)                                                       \
    do {                                                                    \
        fprintf(stderr, "runtime error: " fmt "\n", ##__VA_ARGS__);        \
        exit(1);                                                            \
    } while (0)

static void* bina_xmalloc(size_t size) {
    void* ptr = malloc(size == 0 ? 1 : size);
    if (ptr == NULL) DIE("out of memory");
    return ptr;
}

static void* bina_xrealloc(void* old, size_t size) {
    void* ptr = realloc(old, size == 0 ? 1 : size);
    if (ptr == NULL) DIE("out of memory");
    return ptr;
}

void bina_print_i64(int64_t x) { printf("%lld", (long long)x); }

void bina_print_u64(uint64_t x) {
    printf("%llu", (unsigned long long)x);
}

void bina_print_f64(double x) {
    if (x != x) {
        printf("nan");
        return;
    }
    if (x == (double)(1.0 / 0.0)) {
        printf("inf");
        return;
    }
    if (x == (double)(-1.0 / 0.0)) {
        printf("-inf");
        return;
    }
    printf("%g", x);
}

void bina_print_bool(bool x) { printf("%s", x ? "true" : "false"); }

void bina_print_char(uint32_t cp) {
    char buf[4];
    int n = 0;

    if (cp <= 0x7F) {
        buf[0] = (char)cp;
        n = 1;
    } else if (cp <= 0x7FF) {
        buf[0] = (char)(0xC0 | (cp >> 6));
        buf[1] = (char)(0x80 | (cp & 0x3F));
        n = 2;
    } else if (cp <= 0xFFFF) {
        buf[0] = (char)(0xE0 | (cp >> 12));
        buf[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        buf[2] = (char)(0x80 | (cp & 0x3F));
        n = 3;
    } else {
        buf[0] = (char)(0xF0 | (cp >> 18));
        buf[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
        buf[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
        buf[3] = (char)(0x80 | (cp & 0x3F));
        n = 4;
    }

    fwrite(buf, 1, (size_t)n, stdout);
}

void bina_print_str(const char* ptr, int64_t len) {
    if (len <= 0) return;
    fwrite(ptr, 1, (size_t)len, stdout);
}

bina_str bina_input(void) {
    size_t cap = 64;
    size_t len = 0;
    char* buf = (char*)bina_xmalloc(cap);

    int c = 0;
    while ((c = getchar()) != EOF && c != '\n') {
        if (len + 1 >= cap) {
            cap *= 2;
            buf = (char*)bina_xrealloc(buf, cap);
        }
        buf[len++] = (char)c;
    }

    bina_str s = {buf, (int64_t)len};
    return s;
}

void bina_panic(const char* ptr, int64_t len, int64_t line) {
    fprintf(stderr, "runtime error: %.*s at line %lld\n", (int)len, ptr,
            (long long)line);
    exit(1);
}

void bina_assert(bool ok, int64_t line) {
    if (!ok) DIE("assertion failed at line %lld", (long long)line);
}

void bina_index_oob(int64_t idx, int64_t len, int64_t line) {
    DIE("index out of bounds: index %lld, length %lld at line %lld",
        (long long)idx, (long long)len, (long long)line);
}

void bina_div_zero(int64_t line) {
    DIE("division by zero at line %lld", (long long)line);
}

void bina_int_overflow(int64_t line) {
    DIE("integer overflow at line %lld", (long long)line);
}

uint32_t bina_char_from(int64_t code, int64_t line) {
    if (code < 0 || code > 0x10FFFF ||
        (code >= 0xD800 && code <= 0xDFFF)) {
        DIE("invalid character code at line %lld", (long long)line);
    }
    return (uint32_t)code;
}

bina_str bina_to_string_i64(int64_t x) {
    char tmp[32];
    int n = snprintf(tmp, sizeof(tmp), "%lld", (long long)x);
    char* buf = (char*)bina_xmalloc((size_t)n);
    memcpy(buf, tmp, (size_t)n);
    bina_str s = {buf, (int64_t)n};
    return s;
}

bina_str bina_to_string_u64(uint64_t x) {
    char tmp[32];
    int n = snprintf(tmp, sizeof(tmp), "%llu", (unsigned long long)x);
    char* buf = (char*)bina_xmalloc((size_t)n);
    memcpy(buf, tmp, (size_t)n);
    bina_str s = {buf, (int64_t)n};
    return s;
}

bina_str bina_to_string_f64(double x) {
    char tmp[64];
    int n = 0;
    if (x != x) {
        n = snprintf(tmp, sizeof(tmp), "nan");
    } else if (x == (double)(1.0 / 0.0)) {
        n = snprintf(tmp, sizeof(tmp), "inf");
    } else if (x == (double)(-1.0 / 0.0)) {
        n = snprintf(tmp, sizeof(tmp), "-inf");
    } else {
        n = snprintf(tmp, sizeof(tmp), "%g", x);
    }

    char* buf = (char*)bina_xmalloc((size_t)n);
    memcpy(buf, tmp, (size_t)n);
    bina_str s = {buf, (int64_t)n};
    return s;
}

static char* dup_zero_terminated(bina_str s) {
    char* z = (char*)bina_xmalloc((size_t)s.len + 1);
    if (s.len > 0) memcpy(z, s.ptr, (size_t)s.len);
    z[s.len] = '\0';
    return z;
}

static bool is_digit_char(char c) { return c >= '0' && c <= '9'; }

/* Формат целого: "-"? digit+ — без пробелов, знака "+", hex и прочего,
 * что молча принимает strtoll. */
static bool is_strict_int_format(const char* z) {
    size_t i = 0;
    if (z[i] == '-') i++;
    if (!is_digit_char(z[i])) return false;
    while (is_digit_char(z[i])) i++;
    return z[i] == '\0';
}

/* Формат вещественного: "-"? digit+ ("." digit+)? (("e"|"E") ("+"|"-")? digit+)?
 * — как числовой литерал языка; hex-float и "Infinity" из strtod не проходят. */
static bool is_strict_float_format(const char* z) {
    size_t i = 0;
    if (z[i] == '-') i++;
    if (!is_digit_char(z[i])) return false;
    while (is_digit_char(z[i])) i++;
    if (z[i] == '.') {
        i++;
        if (!is_digit_char(z[i])) return false;
        while (is_digit_char(z[i])) i++;
    }
    if (z[i] == 'e' || z[i] == 'E') {
        i++;
        if (z[i] == '+' || z[i] == '-') i++;
        if (!is_digit_char(z[i])) return false;
        while (is_digit_char(z[i])) i++;
    }
    return z[i] == '\0';
}

int64_t bina_parse_i64(bina_str s, int64_t line) {
    char* z = dup_zero_terminated(s);
    if (!is_strict_int_format(z)) {
        free(z);
        DIE("invalid numeric conversion at line %lld", (long long)line);
    }

    errno = 0;
    char* end = NULL;
    long long v = strtoll(z, &end, 10);
    bool overflow = (errno == ERANGE);
    free(z);

    if (overflow) DIE("integer overflow at line %lld", (long long)line);
    return (int64_t)v;
}

uint64_t bina_parse_u64(bina_str s, int64_t line) {
    char* z = dup_zero_terminated(s);
    if (!is_strict_int_format(z)) {
        free(z);
        DIE("invalid numeric conversion at line %lld", (long long)line);
    }
    if (z[0] == '-') {
        free(z);
        DIE("integer overflow at line %lld", (long long)line);
    }

    errno = 0;
    char* end = NULL;
    unsigned long long v = strtoull(z, &end, 10);
    bool overflow = (errno == ERANGE);
    free(z);

    if (overflow) DIE("integer overflow at line %lld", (long long)line);
    return (uint64_t)v;
}

double bina_parse_f64(bina_str s, int64_t line) {
    char* z = dup_zero_terminated(s);
    if (strcmp(z, "inf") == 0) {
        free(z);
        return 1.0 / 0.0;
    }
    if (strcmp(z, "-inf") == 0) {
        free(z);
        return -1.0 / 0.0;
    }
    if (strcmp(z, "nan") == 0 || strcmp(z, "NaN") == 0) {
        free(z);
        return 0.0 / 0.0;
    }

    if (!is_strict_float_format(z)) {
        free(z);
        DIE("invalid numeric conversion at line %lld", (long long)line);
    }

    char* end = NULL;
    double v = strtod(z, &end);
    free(z);
    (void)end;
    return v;
}

bina_str bina_str_concat(bina_str a, bina_str b) {
    int64_t n = a.len + b.len;
    char* buf = (char*)bina_xmalloc((size_t)n);
    if (a.len > 0) memcpy(buf, a.ptr, (size_t)a.len);
    if (b.len > 0) memcpy(buf + a.len, b.ptr, (size_t)b.len);
    bina_str r = {buf, n};
    return r;
}

bool bina_str_eq(bina_str a, bina_str b) {
    if (a.len != b.len) return false;
    if (a.len == 0) return true;
    return memcmp(a.ptr, b.ptr, (size_t)a.len) == 0;
}

void bina_exit(int64_t code) { exit((int)code); }
