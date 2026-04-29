#ifndef EXPENSE_ABI_H
#define EXPENSE_ABI_H

#include <stddef.h>
#include <stdint.h>

#define EXPENSE_DESCRIPTION_CAP 128
#define EXPENSE_CATEGORY_CAP 64
#define EXPENSE_ERROR_CAP 256
#define EXPENSE_MAX_RECORDS 4096
#define EXPENSE_MAX_CATEGORY_TOTALS 64

#ifdef __cplusplus
extern "C" {
#endif

typedef enum ExpenseStatus {
    EXPENSE_OK = 0,
    EXPENSE_ERR_INVALID_ARGUMENT = -1,
    EXPENSE_ERR_INVALID_DATE = -2,
    EXPENSE_ERR_INVALID_AMOUNT = -3,
    EXPENSE_ERR_IO = -4,
    EXPENSE_ERR_PARSE = -5,
    EXPENSE_ERR_CAPACITY = -6,
    EXPENSE_ERR_NOT_FOUND = -7
} ExpenseStatus;

typedef struct ExpenseRecord {
    uint64_t id;
    int32_t year;
    int32_t month;
    int32_t day;
    int64_t cents;
    char description[EXPENSE_DESCRIPTION_CAP];
    char category[EXPENSE_CATEGORY_CAP];
} ExpenseRecord;

typedef struct ExpenseSummary {
    int32_t year;
    int32_t month;
    uint32_t count;
    int64_t total_cents;
} ExpenseSummary;

typedef struct ExpenseCategoryTotal {
    char category[EXPENSE_CATEGORY_CAP];
    uint32_t count;
    int64_t total_cents;
} ExpenseCategoryTotal;

const char* expense_status_name(int code);
int expense_days_in_month(int32_t year, int32_t month);
int expense_parse_date(const char* text, int32_t* year, int32_t* month, int32_t* day,
                       char* error, size_t error_len);
int expense_parse_year_month(const char* text, int32_t* year, int32_t* month,
                             char* error, size_t error_len);
int expense_parse_cents(const char* text, int64_t* cents, char* error, size_t error_len);
int expense_format_cents(int64_t cents, char* output, size_t output_len);
int expense_validate_record(const ExpenseRecord* record, char* error, size_t error_len);
uint64_t expense_record_fingerprint(const ExpenseRecord* record);

void v_normalize_text(const char* input, char* output, size_t output_len);
void v_canonical_category(const char* input, char* output, size_t output_len);

int zig_monthly_summary(const ExpenseRecord* records, size_t len, int32_t year, int32_t month,
                        ExpenseSummary* out_summary);
int zig_category_totals(const ExpenseRecord* records, size_t len, int32_t year, int32_t month,
                        ExpenseCategoryTotal* out_totals, size_t totals_cap, size_t* out_len);

int rust_store_load(const char* path, ExpenseRecord* out_records, size_t cap, size_t* out_len,
                    char* error, size_t error_len);
int rust_store_save(const char* path, const ExpenseRecord* records, size_t len,
                    char* error, size_t error_len);
int rust_store_append(const char* path, const ExpenseRecord* record,
                      char* error, size_t error_len);
int rust_store_delete_by_id(const char* path, uint64_t id, char* error, size_t error_len);
int rust_settlement_read(const char* path, int32_t* year, int32_t* month,
                         char* error, size_t error_len);
int rust_settlement_write(const char* path, int32_t year, int32_t month,
                          char* error, size_t error_len);

#ifdef __cplusplus
}
#endif

#endif
