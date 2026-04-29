#include "expense_abi.h"

#include <ctype.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static void set_error(char* error, size_t error_len, const char* message) {
    if (error != NULL && error_len > 0) {
        (void)snprintf(error, error_len, "%s", message);
    }
}

const char* expense_status_name(int code) {
    switch (code) {
        case EXPENSE_OK: return "ok";
        case EXPENSE_ERR_INVALID_ARGUMENT: return "invalid argument";
        case EXPENSE_ERR_INVALID_DATE: return "invalid date";
        case EXPENSE_ERR_INVALID_AMOUNT: return "invalid amount";
        case EXPENSE_ERR_IO: return "io error";
        case EXPENSE_ERR_PARSE: return "parse error";
        case EXPENSE_ERR_CAPACITY: return "capacity exceeded";
        case EXPENSE_ERR_NOT_FOUND: return "not found";
        default: return "unknown";
    }
}

static int is_leap_year(int32_t year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

int expense_days_in_month(int32_t year, int32_t month) {
    static const int days[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (month < 1 || month > 12 || year < 1900 || year > 9999) {
        return 0;
    }
    if (month == 2 && is_leap_year(year)) {
        return 29;
    }
    return days[month - 1];
}

static int parse_fixed_int(const char* text, size_t offset, size_t width, int32_t* out) {
    int32_t value = 0;
    for (size_t i = 0; i < width; ++i) {
        unsigned char ch = (unsigned char)text[offset + i];
        if (!isdigit(ch)) {
            return 0;
        }
        value = (int32_t)(value * 10 + (ch - '0'));
    }
    *out = value;
    return 1;
}

int expense_parse_date(const char* text, int32_t* year, int32_t* month, int32_t* day,
                       char* error, size_t error_len) {
    if (text == NULL || year == NULL || month == NULL || day == NULL) {
        set_error(error, error_len, "date parse received a null argument");
        return EXPENSE_ERR_INVALID_ARGUMENT;
    }
    if (strlen(text) != 10 || text[4] != '-' || text[7] != '-') {
        set_error(error, error_len, "date must be YYYY-MM-DD");
        return EXPENSE_ERR_INVALID_DATE;
    }
    if (!parse_fixed_int(text, 0, 4, year) ||
        !parse_fixed_int(text, 5, 2, month) ||
        !parse_fixed_int(text, 8, 2, day)) {
        set_error(error, error_len, "date contains non-numeric fields");
        return EXPENSE_ERR_INVALID_DATE;
    }
    int dim = expense_days_in_month(*year, *month);
    if (dim == 0 || *day < 1 || *day > dim) {
        set_error(error, error_len, "date is outside the supported calendar range");
        return EXPENSE_ERR_INVALID_DATE;
    }
    set_error(error, error_len, "");
    return EXPENSE_OK;
}

int expense_parse_year_month(const char* text, int32_t* year, int32_t* month,
                             char* error, size_t error_len) {
    int32_t day = 1;
    char full[11];
    if (text == NULL || strlen(text) != 7 || text[4] != '-') {
        set_error(error, error_len, "period must be YYYY-MM");
        return EXPENSE_ERR_INVALID_DATE;
    }
    (void)snprintf(full, sizeof(full), "%s-01", text);
    return expense_parse_date(full, year, month, &day, error, error_len);
}

int expense_parse_cents(const char* text, int64_t* cents, char* error, size_t error_len) {
    if (text == NULL || cents == NULL) {
        set_error(error, error_len, "amount parse received a null argument");
        return EXPENSE_ERR_INVALID_ARGUMENT;
    }

    const unsigned char* p = (const unsigned char*)text;
    while (isspace(*p)) {
        ++p;
    }
    if (*p == '+') {
        ++p;
    }
    if (*p == '-' || *p == '\0') {
        set_error(error, error_len, "amount must be positive");
        return EXPENSE_ERR_INVALID_AMOUNT;
    }

    int64_t units = 0;
    int saw_digit = 0;
    while (isdigit(*p)) {
        saw_digit = 1;
        if (units > (INT64_MAX - 9) / 10) {
            set_error(error, error_len, "amount is too large");
            return EXPENSE_ERR_INVALID_AMOUNT;
        }
        units = units * 10 + (*p - '0');
        ++p;
    }

    int64_t frac = 0;
    int frac_digits = 0;
    if (*p == '.') {
        ++p;
        while (isdigit(*p)) {
            if (frac_digits >= 2) {
                set_error(error, error_len, "amount supports at most two decimal places");
                return EXPENSE_ERR_INVALID_AMOUNT;
            }
            frac = frac * 10 + (*p - '0');
            ++frac_digits;
            ++p;
        }
    }

    while (isspace(*p)) {
        ++p;
    }
    if (*p != '\0' || !saw_digit) {
        set_error(error, error_len, "amount must be a decimal number");
        return EXPENSE_ERR_INVALID_AMOUNT;
    }
    while (frac_digits < 2) {
        frac *= 10;
        ++frac_digits;
    }
    if (units > (INT64_MAX - frac) / 100) {
        set_error(error, error_len, "amount is too large");
        return EXPENSE_ERR_INVALID_AMOUNT;
    }
    *cents = units * 100 + frac;
    if (*cents <= 0) {
        set_error(error, error_len, "amount must be greater than zero");
        return EXPENSE_ERR_INVALID_AMOUNT;
    }
    set_error(error, error_len, "");
    return EXPENSE_OK;
}

int expense_format_cents(int64_t cents, char* output, size_t output_len) {
    if (output == NULL || output_len == 0) {
        return EXPENSE_ERR_INVALID_ARGUMENT;
    }
    int64_t abs_value = cents < 0 ? -cents : cents;
    int written = snprintf(output, output_len, "%s%" PRId64 ".%02" PRId64,
                           cents < 0 ? "-" : "", abs_value / 100, abs_value % 100);
    if (written < 0 || (size_t)written >= output_len) {
        return EXPENSE_ERR_CAPACITY;
    }
    return EXPENSE_OK;
}

static int has_text(const char* buffer, size_t cap) {
    for (size_t i = 0; i < cap && buffer[i] != '\0'; ++i) {
        if (!isspace((unsigned char)buffer[i])) {
            return 1;
        }
    }
    return 0;
}

static int is_terminated(const char* buffer, size_t cap) {
    return memchr(buffer, '\0', cap) != NULL;
}

int expense_validate_record(const ExpenseRecord* record, char* error, size_t error_len) {
    if (record == NULL) {
        set_error(error, error_len, "record is null");
        return EXPENSE_ERR_INVALID_ARGUMENT;
    }
    int dim = expense_days_in_month(record->year, record->month);
    if (dim == 0 || record->day < 1 || record->day > dim) {
        set_error(error, error_len, "record date is invalid");
        return EXPENSE_ERR_INVALID_DATE;
    }
    if (record->cents <= 0) {
        set_error(error, error_len, "record amount must be greater than zero");
        return EXPENSE_ERR_INVALID_AMOUNT;
    }
    if (!is_terminated(record->description, EXPENSE_DESCRIPTION_CAP) ||
        !has_text(record->description, EXPENSE_DESCRIPTION_CAP)) {
        set_error(error, error_len, "description is empty or not terminated");
        return EXPENSE_ERR_INVALID_ARGUMENT;
    }
    if (!is_terminated(record->category, EXPENSE_CATEGORY_CAP) ||
        !has_text(record->category, EXPENSE_CATEGORY_CAP)) {
        set_error(error, error_len, "category is empty or not terminated");
        return EXPENSE_ERR_INVALID_ARGUMENT;
    }
    set_error(error, error_len, "");
    return EXPENSE_OK;
}

static uint64_t fnv1a_bytes(uint64_t hash, const void* data, size_t len) {
    const unsigned char* p = (const unsigned char*)data;
    for (size_t i = 0; i < len; ++i) {
        hash ^= (uint64_t)p[i];
        hash *= 1099511628211ull;
    }
    return hash;
}

uint64_t expense_record_fingerprint(const ExpenseRecord* record) {
    if (record == NULL) {
        return 0;
    }
    uint64_t hash = 1469598103934665603ull;
    hash = fnv1a_bytes(hash, &record->year, sizeof(record->year));
    hash = fnv1a_bytes(hash, &record->month, sizeof(record->month));
    hash = fnv1a_bytes(hash, &record->day, sizeof(record->day));
    hash = fnv1a_bytes(hash, &record->cents, sizeof(record->cents));
    hash = fnv1a_bytes(hash, record->description, strnlen(record->description, EXPENSE_DESCRIPTION_CAP));
    hash = fnv1a_bytes(hash, record->category, strnlen(record->category, EXPENSE_CATEGORY_CAP));
    return hash == 0 ? 1 : hash;
}
