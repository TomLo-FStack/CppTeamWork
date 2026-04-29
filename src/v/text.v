module main

fn is_space(ch u8) bool {
    return ch == ` ` || ch == `\t` || ch == `\n` || ch == `\r`
}

fn lower_ascii(ch u8) u8 {
    if ch >= `A` && ch <= `Z` {
        return ch + 32
    }
    return ch
}

fn write_normalized(input &char, output &char, output_len usize, title_case bool) {
    unsafe {
        if output_len == 0 {
            return
        }
        mut out_i := usize(0)
        mut in_i := usize(0)
        mut pending_space := false
        mut at_word_start := true
        for input[in_i] != 0 && out_i + 1 < output_len {
            raw := u8(input[in_i])
            in_i++
            if is_space(raw) {
                if out_i > 0 {
                    pending_space = true
                    at_word_start = true
                }
                continue
            }
            if pending_space && out_i + 1 < output_len {
                output[out_i] = char(` `)
                out_i++
                pending_space = false
            }
            mut ch := lower_ascii(raw)
            if title_case && at_word_start && ch >= `a` && ch <= `z` {
                ch -= 32
            }
            output[out_i] = char(ch)
            out_i++
            at_word_start = false
        }
        if out_i > 0 && output[out_i - 1] == char(` `) {
            out_i--
        }
        output[out_i] = 0
    }
}

@[export: 'v_normalize_text']
pub fn v_normalize_text(input &char, output &char, output_len usize) {
    write_normalized(input, output, output_len, false)
}

@[export: 'v_canonical_category']
pub fn v_canonical_category(input &char, output &char, output_len usize) {
    write_normalized(input, output, output_len, true)
}
