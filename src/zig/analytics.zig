const DESCRIPTION_CAP = 128;
const CATEGORY_CAP = 64;

const ExpenseRecord = extern struct {
    id: u64,
    year: i32,
    month: i32,
    day: i32,
    cents: i64,
    description: [DESCRIPTION_CAP]u8,
    category: [CATEGORY_CAP]u8,
};

const ExpenseSummary = extern struct {
    year: i32,
    month: i32,
    count: u32,
    total_cents: i64,
};

const ExpenseCategoryTotal = extern struct {
    category: [CATEGORY_CAP]u8,
    count: u32,
    total_cents: i64,
};

fn clearCategory(dest: *[CATEGORY_CAP]u8) void {
    for (dest) |*ch| {
        ch.* = 0;
    }
}

fn copyCategory(dest: *[CATEGORY_CAP]u8, src: *const [CATEGORY_CAP]u8) void {
    clearCategory(dest);
    var i: usize = 0;
    while (i < CATEGORY_CAP - 1 and src[i] != 0) : (i += 1) {
        dest[i] = src[i];
    }
}

fn categoryEquals(a: *const [CATEGORY_CAP]u8, b: *const [CATEGORY_CAP]u8) bool {
    var i: usize = 0;
    while (i < CATEGORY_CAP) : (i += 1) {
        if (a[i] != b[i]) return false;
        if (a[i] == 0) return true;
    }
    return true;
}

export fn zig_monthly_summary(
    records: [*]const ExpenseRecord,
    len: usize,
    year: i32,
    month: i32,
    out_summary: *ExpenseSummary,
) i32 {
    if (month < 1 or month > 12) return -2;
    out_summary.year = year;
    out_summary.month = month;
    out_summary.count = 0;
    out_summary.total_cents = 0;

    var i: usize = 0;
    while (i < len) : (i += 1) {
        const rec = records[i];
        if (rec.year == year and rec.month == month) {
            out_summary.count += 1;
            out_summary.total_cents += rec.cents;
        }
    }
    return 0;
}

export fn zig_category_totals(
    records: [*]const ExpenseRecord,
    len: usize,
    year: i32,
    month: i32,
    out_totals: [*]ExpenseCategoryTotal,
    totals_cap: usize,
    out_len: *usize,
) i32 {
    if (month < 1 or month > 12) return -2;
    out_len.* = 0;

    var i: usize = 0;
    while (i < len) : (i += 1) {
        const rec = records[i];
        if (rec.year != year or rec.month != month) continue;

        var found: ?usize = null;
        var j: usize = 0;
        while (j < out_len.*) : (j += 1) {
            if (categoryEquals(&out_totals[j].category, &rec.category)) {
                found = j;
                break;
            }
        }

        if (found) |index| {
            out_totals[index].count += 1;
            out_totals[index].total_cents += rec.cents;
        } else {
            if (out_len.* >= totals_cap) return -6;
            const index = out_len.*;
            copyCategory(&out_totals[index].category, &rec.category);
            out_totals[index].count = 1;
            out_totals[index].total_cents = rec.cents;
            out_len.* += 1;
        }
    }
    return 0;
}
