#include "expense_abi.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace {

struct Config {
    std::filesystem::path data_path = "expenses.etx";
    std::filesystem::path settlement_path = "settlement_state.txt";
};

struct AppError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

std::string read_fixed(const char* data, std::size_t cap) {
    const auto len = strnlen(data, cap);
    return std::string(data, data + len);
}

void write_fixed(char* dest, std::size_t cap, const std::string& value) {
    std::fill(dest, dest + cap, '\0');
    const auto copy_len = std::min(cap - 1, value.size());
    std::memcpy(dest, value.data(), copy_len);
}

std::string error_text(const char* error, int code) {
    const std::string detail = error != nullptr ? std::string(error) : std::string();
    if (!detail.empty()) {
        return detail;
    }
    return expense_status_name(code);
}

std::string money(int64_t cents) {
    std::array<char, 64> out{};
    const int rc = expense_format_cents(cents, out.data(), out.size());
    if (rc != EXPENSE_OK) {
        return "<amount-error>";
    }
    return out.data();
}

std::string date_of(const ExpenseRecord& record) {
    std::ostringstream out;
    out << std::setfill('0') << std::setw(4) << record.year << '-'
        << std::setw(2) << record.month << '-' << std::setw(2) << record.day;
    return out.str();
}

std::string normalize_text(std::string_view input, bool category) {
    std::array<char, 256> out{};
    std::string owned(input);
    if (category) {
        v_canonical_category(owned.c_str(), out.data(), out.size());
    } else {
        v_normalize_text(owned.c_str(), out.data(), out.size());
    }
    return out.data();
}

std::string join_tail(const std::vector<std::string>& args, std::size_t start) {
    std::ostringstream out;
    for (std::size_t i = start; i < args.size(); ++i) {
        if (i != start) {
            out << ' ';
        }
        out << args[i];
    }
    return out.str();
}

std::vector<ExpenseRecord> load_records(const Config& config) {
    std::array<ExpenseRecord, EXPENSE_MAX_RECORDS> buffer{};
    std::size_t len = 0;
    std::array<char, EXPENSE_ERROR_CAP> error{};
    const auto path = config.data_path.string();
    const int rc = rust_store_load(path.c_str(), buffer.data(), buffer.size(), &len,
                                   error.data(), error.size());
    if (rc != EXPENSE_OK) {
        throw AppError("load failed: " + error_text(error.data(), rc));
    }
    return std::vector<ExpenseRecord>(buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(len));
}

void save_records(const Config& config, const std::vector<ExpenseRecord>& records) {
    std::array<char, EXPENSE_ERROR_CAP> error{};
    const auto path = config.data_path.string();
    const int rc = rust_store_save(path.c_str(), records.data(), records.size(), error.data(), error.size());
    if (rc != EXPENSE_OK) {
        throw AppError("save failed: " + error_text(error.data(), rc));
    }
}

uint64_t next_id(const std::vector<ExpenseRecord>& records, const ExpenseRecord& candidate) {
    uint64_t max_id = 0;
    for (const auto& record : records) {
        max_id = std::max(max_id, record.id);
    }
    if (max_id != std::numeric_limits<uint64_t>::max()) {
        return max_id + 1;
    }
    return expense_record_fingerprint(&candidate);
}

ExpenseRecord build_record(const std::vector<std::string>& args, const std::vector<ExpenseRecord>& existing) {
    if (args.size() < 4) {
        throw AppError("usage: add YYYY-MM-DD amount category description...");
    }
    std::array<char, EXPENSE_ERROR_CAP> error{};

    ExpenseRecord record{};
    int rc = expense_parse_date(args[0].c_str(), &record.year, &record.month, &record.day,
                                error.data(), error.size());
    if (rc != EXPENSE_OK) {
        throw AppError("invalid date: " + error_text(error.data(), rc));
    }
    rc = expense_parse_cents(args[1].c_str(), &record.cents, error.data(), error.size());
    if (rc != EXPENSE_OK) {
        throw AppError("invalid amount: " + error_text(error.data(), rc));
    }

    const auto category = normalize_text(args[2], true);
    const auto description = normalize_text(join_tail(args, 3), false);
    if (category.empty() || description.empty()) {
        throw AppError("category and description must not be empty");
    }
    write_fixed(record.category, EXPENSE_CATEGORY_CAP, category);
    write_fixed(record.description, EXPENSE_DESCRIPTION_CAP, description);
    record.id = next_id(existing, record);

    rc = expense_validate_record(&record, error.data(), error.size());
    if (rc != EXPENSE_OK) {
        throw AppError("invalid record: " + error_text(error.data(), rc));
    }
    return record;
}

void append_record(const Config& config, const ExpenseRecord& record) {
    std::array<char, EXPENSE_ERROR_CAP> error{};
    const auto path = config.data_path.string();
    const int rc = rust_store_append(path.c_str(), &record, error.data(), error.size());
    if (rc != EXPENSE_OK) {
        throw AppError("append failed: " + error_text(error.data(), rc));
    }
}

void print_records(std::vector<ExpenseRecord> records) {
    std::sort(records.begin(), records.end(), [](const ExpenseRecord& a, const ExpenseRecord& b) {
        return std::tie(a.year, a.month, a.day, a.id) < std::tie(b.year, b.month, b.day, b.id);
    });
    std::cout << std::left << std::setw(8) << "id"
              << std::setw(12) << "date"
              << std::right << std::setw(12) << "amount"
              << "  " << std::left << std::setw(18) << "category"
              << "description\n";
    std::cout << std::string(72, '-') << '\n';
    for (const auto& record : records) {
        std::cout << std::left << std::setw(8) << record.id
                  << std::setw(12) << date_of(record)
                  << std::right << std::setw(12) << money(record.cents)
                  << "  " << std::left << std::setw(18) << read_fixed(record.category, EXPENSE_CATEGORY_CAP)
                  << read_fixed(record.description, EXPENSE_DESCRIPTION_CAP) << '\n';
    }
    std::cout << records.size() << " record(s)\n";
}

std::vector<ExpenseRecord> filter_month(const std::vector<ExpenseRecord>& records, int32_t year, int32_t month) {
    std::vector<ExpenseRecord> out;
    std::copy_if(records.begin(), records.end(), std::back_inserter(out), [&](const ExpenseRecord& record) {
        return record.year == year && record.month == month;
    });
    return out;
}

std::vector<ExpenseRecord> filter_year(const std::vector<ExpenseRecord>& records, int32_t year) {
    std::vector<ExpenseRecord> out;
    std::copy_if(records.begin(), records.end(), std::back_inserter(out), [&](const ExpenseRecord& record) {
        return record.year == year;
    });
    return out;
}

std::pair<int32_t, int32_t> parse_year_month_or_throw(const std::string& text) {
    int32_t year = 0;
    int32_t month = 0;
    std::array<char, EXPENSE_ERROR_CAP> error{};
    const int rc = expense_parse_year_month(text.c_str(), &year, &month, error.data(), error.size());
    if (rc != EXPENSE_OK) {
        throw AppError("invalid period: " + error_text(error.data(), rc));
    }
    return {year, month};
}

void print_summary(const std::vector<ExpenseRecord>& records, int32_t year, int32_t month) {
    ExpenseSummary summary{};
    int rc = zig_monthly_summary(records.data(), records.size(), year, month, &summary);
    if (rc != EXPENSE_OK) {
        throw AppError("summary failed: " + std::string(expense_status_name(rc)));
    }

    std::array<ExpenseCategoryTotal, EXPENSE_MAX_CATEGORY_TOTALS> totals{};
    std::size_t totals_len = 0;
    rc = zig_category_totals(records.data(), records.size(), year, month,
                             totals.data(), totals.size(), &totals_len);
    if (rc != EXPENSE_OK) {
        throw AppError("category summary failed: " + std::string(expense_status_name(rc)));
    }
    std::sort(totals.begin(), totals.begin() + static_cast<std::ptrdiff_t>(totals_len),
              [](const ExpenseCategoryTotal& a, const ExpenseCategoryTotal& b) {
                  if (a.total_cents != b.total_cents) {
                      return a.total_cents > b.total_cents;
                  }
                  return read_fixed(a.category, EXPENSE_CATEGORY_CAP) < read_fixed(b.category, EXPENSE_CATEGORY_CAP);
              });

    std::cout << "summary " << std::setfill('0') << std::setw(4) << year << '-'
              << std::setw(2) << month << std::setfill(' ') << "\n";
    std::cout << "records: " << summary.count << "\n";
    std::cout << "total:   " << money(summary.total_cents) << "\n";
    if (totals_len > 0) {
        std::cout << "\nby category\n";
        for (std::size_t i = 0; i < totals_len; ++i) {
            std::cout << "  " << std::left << std::setw(18)
                      << read_fixed(totals[i].category, EXPENSE_CATEGORY_CAP)
                      << std::right << std::setw(12) << money(totals[i].total_cents)
                      << "  (" << totals[i].count << ")\n";
        }
    }
}

void command_add(const Config& config, const std::vector<std::string>& args) {
    const auto existing = load_records(config);
    const auto record = build_record(args, existing);
    append_record(config, record);
    std::cout << "added id=" << record.id << " " << date_of(record) << " "
              << money(record.cents) << " " << read_fixed(record.category, EXPENSE_CATEGORY_CAP) << "\n";
}

void command_list(const Config& config, const std::vector<std::string>& args) {
    const auto records = load_records(config);
    if (args.empty() || args[0] == "all") {
        print_records(records);
        return;
    }
    if (args[0] == "month" && args.size() == 2) {
        const auto [year, month] = parse_year_month_or_throw(args[1]);
        print_records(filter_month(records, year, month));
        return;
    }
    if (args[0] == "year" && args.size() == 2) {
        print_records(filter_year(records, std::stoi(args[1])));
        return;
    }
    throw AppError("usage: list [all | month YYYY-MM | year YYYY]");
}

void command_summary(const Config& config, const std::vector<std::string>& args) {
    if (args.size() != 1) {
        throw AppError("usage: summary YYYY-MM");
    }
    const auto [year, month] = parse_year_month_or_throw(args[0]);
    print_summary(load_records(config), year, month);
}

void command_delete(const Config& config, const std::vector<std::string>& args) {
    if (args.size() != 1) {
        throw AppError("usage: delete id");
    }
    std::array<char, EXPENSE_ERROR_CAP> error{};
    const auto path = config.data_path.string();
    const auto id = std::stoull(args[0]);
    const int rc = rust_store_delete_by_id(path.c_str(), id, error.data(), error.size());
    if (rc != EXPENSE_OK) {
        throw AppError("delete failed: " + error_text(error.data(), rc));
    }
    std::cout << "deleted id=" << id << "\n";
}

void command_settle(const Config& config, const std::vector<std::string>& args) {
    if (args.size() != 1) {
        throw AppError("usage: settle YYYY-MM");
    }
    const auto [year, month] = parse_year_month_or_throw(args[0]);
    print_summary(load_records(config), year, month);

    std::array<char, EXPENSE_ERROR_CAP> error{};
    const auto path = config.settlement_path.string();
    const int rc = rust_settlement_write(path.c_str(), year, month, error.data(), error.size());
    if (rc != EXPENSE_OK) {
        throw AppError("settlement write failed: " + error_text(error.data(), rc));
    }
    std::cout << "settled through " << args[0] << "\n";
}

void print_help() {
    std::cout
        << "CppTeamWork system-language expense lab\n\n"
        << "usage:\n"
        << "  expense_app [--data file] [--settlement file] add YYYY-MM-DD amount category description...\n"
        << "  expense_app [--data file] list [all | month YYYY-MM | year YYYY]\n"
        << "  expense_app [--data file] summary YYYY-MM\n"
        << "  expense_app [--data file] delete id\n"
        << "  expense_app [--settlement file] settle YYYY-MM\n"
        << "  expense_app tui\n";
}

std::string prompt(std::string_view label) {
    std::cout << label;
    std::string line;
    std::getline(std::cin, line);
    return line;
}

void run_tui(const Config& config) {
    for (;;) {
        std::cout << "\nExpense Tracker\n"
                  << "1. Add expense\n"
                  << "2. List all\n"
                  << "3. List month\n"
                  << "4. Monthly summary\n"
                  << "5. Delete\n"
                  << "6. Settle month\n"
                  << "0. Exit\n"
                  << "> ";
        std::string choice;
        std::getline(std::cin, choice);
        try {
            if (choice == "0") {
                return;
            }
            if (choice == "1") {
                std::vector<std::string> args;
                args.push_back(prompt("date YYYY-MM-DD: "));
                args.push_back(prompt("amount: "));
                args.push_back(prompt("category: "));
                args.push_back(prompt("description: "));
                command_add(config, args);
            } else if (choice == "2") {
                command_list(config, {"all"});
            } else if (choice == "3") {
                command_list(config, {"month", prompt("period YYYY-MM: ")});
            } else if (choice == "4") {
                command_summary(config, {prompt("period YYYY-MM: ")});
            } else if (choice == "5") {
                command_delete(config, {prompt("id: ")});
            } else if (choice == "6") {
                command_settle(config, {prompt("period YYYY-MM: ")});
            } else {
                std::cout << "unknown option\n";
            }
        } catch (const AppError& err) {
            std::cerr << "error: " << err.what() << "\n";
        } catch (const std::exception& err) {
            std::cerr << "error: " << err.what() << "\n";
        }
    }
}

std::vector<std::string> tail(const std::vector<std::string>& args, std::size_t start) {
    if (start >= args.size()) {
        return {};
    }
    return {args.begin() + static_cast<std::ptrdiff_t>(start), args.end()};
}

int dispatch(Config config, std::vector<std::string> args) {
    for (std::size_t i = 0; i < args.size();) {
        if (args[i] == "--data" && i + 1 < args.size()) {
            config.data_path = args[i + 1];
            args.erase(args.begin() + static_cast<std::ptrdiff_t>(i),
                       args.begin() + static_cast<std::ptrdiff_t>(i + 2));
        } else if (args[i] == "--settlement" && i + 1 < args.size()) {
            config.settlement_path = args[i + 1];
            args.erase(args.begin() + static_cast<std::ptrdiff_t>(i),
                       args.begin() + static_cast<std::ptrdiff_t>(i + 2));
        } else {
            ++i;
        }
    }
    if (args.empty() || args[0] == "help" || args[0] == "--help" || args[0] == "-h") {
        print_help();
        return 0;
    }

    const std::string command = args[0];
    const auto rest = tail(args, 1);
    if (command == "add") {
        command_add(config, rest);
    } else if (command == "list") {
        command_list(config, rest);
    } else if (command == "summary") {
        command_summary(config, rest);
    } else if (command == "delete") {
        command_delete(config, rest);
    } else if (command == "settle") {
        command_settle(config, rest);
    } else if (command == "tui") {
        run_tui(config);
    } else if (command == "version") {
        std::cout << "expense_app 0.1.0\n";
    } else {
        throw AppError("unknown command: " + command);
    }
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }
    try {
        return dispatch(Config{}, std::move(args));
    } catch (const AppError& err) {
        std::cerr << "error: " << err.what() << "\n";
        return EXIT_FAILURE;
    } catch (const std::exception& err) {
        std::cerr << "fatal: " << err.what() << "\n";
        return EXIT_FAILURE;
    }
}
