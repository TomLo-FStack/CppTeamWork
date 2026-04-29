use std::ffi::{CStr, OsString};
use std::fs;
use std::io::{self, Write};
use std::os::raw::c_char;
use std::path::{Path, PathBuf};

const DESCRIPTION_CAP: usize = 128;
const CATEGORY_CAP: usize = 64;
const OK: i32 = 0;
const ERR_INVALID_ARGUMENT: i32 = -1;
const ERR_IO: i32 = -4;
const ERR_PARSE: i32 = -5;
const ERR_CAPACITY: i32 = -6;
const ERR_NOT_FOUND: i32 = -7;
const HEADER: &str = "# expense-ledger/v1";

#[repr(C)]
#[derive(Clone, Copy)]
pub struct ExpenseRecord {
    id: u64,
    year: i32,
    month: i32,
    day: i32,
    cents: i64,
    description: [c_char; DESCRIPTION_CAP],
    category: [c_char; CATEGORY_CAP],
}

fn write_error(error: *mut c_char, error_len: usize, message: &str) {
    if error.is_null() || error_len == 0 {
        return;
    }
    let bytes = message.as_bytes();
    let copy_len = bytes.len().min(error_len.saturating_sub(1));
    unsafe {
        std::ptr::copy_nonoverlapping(bytes.as_ptr(), error.cast::<u8>(), copy_len);
        *error.add(copy_len) = 0;
    }
}

unsafe fn path_from_c(path: *const c_char) -> Result<PathBuf, &'static str> {
    if path.is_null() {
        return Err("path is null");
    }
    let text = CStr::from_ptr(path).to_string_lossy();
    if text.trim().is_empty() {
        return Err("path is empty");
    }
    Ok(PathBuf::from(OsString::from(text.as_ref())))
}

fn array_to_string<const N: usize>(data: &[c_char; N]) -> String {
    let len = data.iter().position(|&ch| ch == 0).unwrap_or(N);
    let bytes: Vec<u8> = data[..len].iter().map(|&ch| ch as u8).collect();
    String::from_utf8_lossy(&bytes).trim().to_string()
}

fn write_array<const N: usize>(dest: &mut [c_char; N], value: &str) {
    dest.fill(0);
    let bytes = value.as_bytes();
    let copy_len = bytes.len().min(N.saturating_sub(1));
    for i in 0..copy_len {
        dest[i] = bytes[i] as c_char;
    }
}

fn hex_encode(text: &str) -> String {
    const HEX: &[u8; 16] = b"0123456789abcdef";
    let mut out = String::with_capacity(text.len() * 2);
    for &byte in text.as_bytes() {
        out.push(HEX[(byte >> 4) as usize] as char);
        out.push(HEX[(byte & 0x0f) as usize] as char);
    }
    out
}

fn hex_value(ch: u8) -> Option<u8> {
    match ch {
        b'0'..=b'9' => Some(ch - b'0'),
        b'a'..=b'f' => Some(ch - b'a' + 10),
        b'A'..=b'F' => Some(ch - b'A' + 10),
        _ => None,
    }
}

fn hex_decode(text: &str) -> Result<String, &'static str> {
    let bytes = text.as_bytes();
    if bytes.len() % 2 != 0 {
        return Err("hex field has an odd length");
    }
    let mut out = Vec::with_capacity(bytes.len() / 2);
    let mut i = 0;
    while i < bytes.len() {
        let high = hex_value(bytes[i]).ok_or("hex field contains invalid characters")?;
        let low = hex_value(bytes[i + 1]).ok_or("hex field contains invalid characters")?;
        out.push((high << 4) | low);
        i += 2;
    }
    String::from_utf8(out).map_err(|_| "hex field is not valid UTF-8")
}

fn serialize_record(record: &ExpenseRecord) -> String {
    format!(
        "record\t{}\t{}\t{}\t{}\t{}\t{}\t{}",
        record.id,
        record.year,
        record.month,
        record.day,
        record.cents,
        hex_encode(&array_to_string(&record.description)),
        hex_encode(&array_to_string(&record.category))
    )
}

fn parse_record(line: &str) -> Result<ExpenseRecord, &'static str> {
    let parts: Vec<&str> = line.split('\t').collect();
    if parts.len() != 8 || parts[0] != "record" {
        return Err("record line has an invalid shape");
    }
    let id = parts[1].parse::<u64>().map_err(|_| "id is invalid")?;
    let year = parts[2].parse::<i32>().map_err(|_| "year is invalid")?;
    let month = parts[3].parse::<i32>().map_err(|_| "month is invalid")?;
    let day = parts[4].parse::<i32>().map_err(|_| "day is invalid")?;
    let cents = parts[5].parse::<i64>().map_err(|_| "amount is invalid")?;
    let description = hex_decode(parts[6])?;
    let category = hex_decode(parts[7])?;

    let mut record = ExpenseRecord {
        id,
        year,
        month,
        day,
        cents,
        description: [0; DESCRIPTION_CAP],
        category: [0; CATEGORY_CAP],
    };
    write_array(&mut record.description, &description);
    write_array(&mut record.category, &category);
    Ok(record)
}

fn load_records(path: &Path) -> Result<Vec<ExpenseRecord>, String> {
    if !path.exists() {
        return Ok(Vec::new());
    }
    let content = fs::read_to_string(path).map_err(|err| format!("read failed: {err}"))?;
    let mut lines = content.lines();
    match lines.next() {
        Some(HEADER) => {}
        Some(_) => return Err("ledger header is not recognized".to_string()),
        None => return Ok(Vec::new()),
    }

    let mut records = Vec::new();
    for (index, line) in lines.enumerate() {
        if line.trim().is_empty() {
            continue;
        }
        let record = parse_record(line).map_err(|err| format!("line {}: {err}", index + 2))?;
        records.push(record);
    }
    Ok(records)
}

fn atomic_write(path: &Path, body: &str) -> io::Result<()> {
    if let Some(parent) = path.parent() {
        if !parent.as_os_str().is_empty() {
            fs::create_dir_all(parent)?;
        }
    }
    let tmp = path.with_extension("tmp");
    {
        let mut file = fs::File::create(&tmp)?;
        file.write_all(body.as_bytes())?;
        file.sync_all()?;
    }
    if path.exists() {
        fs::remove_file(path)?;
    }
    fs::rename(tmp, path)?;
    Ok(())
}

fn save_records(path: &Path, records: &[ExpenseRecord]) -> Result<(), String> {
    let mut body = String::from(HEADER);
    body.push('\n');
    for record in records {
        body.push_str(&serialize_record(record));
        body.push('\n');
    }
    atomic_write(path, &body).map_err(|err| format!("write failed: {err}"))
}

#[no_mangle]
pub unsafe extern "C" fn rust_store_load(
    path: *const c_char,
    out_records: *mut ExpenseRecord,
    cap: usize,
    out_len: *mut usize,
    error: *mut c_char,
    error_len: usize,
) -> i32 {
    if out_records.is_null() || out_len.is_null() {
        write_error(error, error_len, "load received a null output pointer");
        return ERR_INVALID_ARGUMENT;
    }
    let path = match path_from_c(path) {
        Ok(path) => path,
        Err(message) => {
            write_error(error, error_len, message);
            return ERR_INVALID_ARGUMENT;
        }
    };
    let records = match load_records(&path) {
        Ok(records) => records,
        Err(message) => {
            write_error(error, error_len, &message);
            return ERR_PARSE;
        }
    };
    if records.len() > cap {
        write_error(error, error_len, "ledger contains more records than caller capacity");
        return ERR_CAPACITY;
    }
    for (index, record) in records.iter().enumerate() {
        *out_records.add(index) = *record;
    }
    *out_len = records.len();
    write_error(error, error_len, "");
    OK
}

#[no_mangle]
pub unsafe extern "C" fn rust_store_save(
    path: *const c_char,
    records: *const ExpenseRecord,
    len: usize,
    error: *mut c_char,
    error_len: usize,
) -> i32 {
    if records.is_null() && len > 0 {
        write_error(error, error_len, "save received a null records pointer");
        return ERR_INVALID_ARGUMENT;
    }
    let path = match path_from_c(path) {
        Ok(path) => path,
        Err(message) => {
            write_error(error, error_len, message);
            return ERR_INVALID_ARGUMENT;
        }
    };
    let slice = if len == 0 {
        &[]
    } else {
        std::slice::from_raw_parts(records, len)
    };
    match save_records(&path, slice) {
        Ok(()) => {
            write_error(error, error_len, "");
            OK
        }
        Err(message) => {
            write_error(error, error_len, &message);
            ERR_IO
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn rust_store_append(
    path: *const c_char,
    record: *const ExpenseRecord,
    error: *mut c_char,
    error_len: usize,
) -> i32 {
    if record.is_null() {
        write_error(error, error_len, "append received a null record pointer");
        return ERR_INVALID_ARGUMENT;
    }
    let path = match path_from_c(path) {
        Ok(path) => path,
        Err(message) => {
            write_error(error, error_len, message);
            return ERR_INVALID_ARGUMENT;
        }
    };
    let mut records = match load_records(&path) {
        Ok(records) => records,
        Err(message) => {
            write_error(error, error_len, &message);
            return ERR_PARSE;
        }
    };
    records.push(*record);
    match save_records(&path, &records) {
        Ok(()) => {
            write_error(error, error_len, "");
            OK
        }
        Err(message) => {
            write_error(error, error_len, &message);
            ERR_IO
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn rust_store_delete_by_id(
    path: *const c_char,
    id: u64,
    error: *mut c_char,
    error_len: usize,
) -> i32 {
    let path = match path_from_c(path) {
        Ok(path) => path,
        Err(message) => {
            write_error(error, error_len, message);
            return ERR_INVALID_ARGUMENT;
        }
    };
    let mut records = match load_records(&path) {
        Ok(records) => records,
        Err(message) => {
            write_error(error, error_len, &message);
            return ERR_PARSE;
        }
    };
    let before = records.len();
    records.retain(|record| record.id != id);
    if before == records.len() {
        write_error(error, error_len, "id was not found");
        return ERR_NOT_FOUND;
    }
    match save_records(&path, &records) {
        Ok(()) => {
            write_error(error, error_len, "");
            OK
        }
        Err(message) => {
            write_error(error, error_len, &message);
            ERR_IO
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn rust_settlement_read(
    path: *const c_char,
    year: *mut i32,
    month: *mut i32,
    error: *mut c_char,
    error_len: usize,
) -> i32 {
    if year.is_null() || month.is_null() {
        write_error(error, error_len, "settlement read received a null output pointer");
        return ERR_INVALID_ARGUMENT;
    }
    let path = match path_from_c(path) {
        Ok(path) => path,
        Err(message) => {
            write_error(error, error_len, message);
            return ERR_INVALID_ARGUMENT;
        }
    };
    if !path.exists() {
        *year = 0;
        *month = 0;
        write_error(error, error_len, "");
        return OK;
    }
    let content = match fs::read_to_string(&path) {
        Ok(content) => content,
        Err(err) => {
            write_error(error, error_len, &format!("settlement read failed: {err}"));
            return ERR_IO;
        }
    };
    let parts: Vec<&str> = content.split_whitespace().collect();
    if parts.len() != 2 {
        write_error(error, error_len, "settlement state has invalid shape");
        return ERR_PARSE;
    }
    *year = match parts[0].parse::<i32>() {
        Ok(value) => value,
        Err(_) => {
            write_error(error, error_len, "settlement year is invalid");
            return ERR_PARSE;
        }
    };
    *month = match parts[1].parse::<i32>() {
        Ok(value) => value,
        Err(_) => {
            write_error(error, error_len, "settlement month is invalid");
            return ERR_PARSE;
        }
    };
    write_error(error, error_len, "");
    OK
}

#[no_mangle]
pub unsafe extern "C" fn rust_settlement_write(
    path: *const c_char,
    year: i32,
    month: i32,
    error: *mut c_char,
    error_len: usize,
) -> i32 {
    let path = match path_from_c(path) {
        Ok(path) => path,
        Err(message) => {
            write_error(error, error_len, message);
            return ERR_INVALID_ARGUMENT;
        }
    };
    let body = format!("{year} {month}\n");
    match atomic_write(&path, &body) {
        Ok(()) => {
            write_error(error, error_len, "");
            OK
        }
        Err(err) => {
            write_error(error, error_len, &format!("settlement write failed: {err}"));
            ERR_IO
        }
    }
}
