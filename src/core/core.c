// (C) Copyright 2025 Matyas Constans
// Licensed under the MIT License (https://opensource.org/license/mit/)

// ------------------------------------------------------------
// #-- String Operations.
fn_internal U64 cstring_len(char *cstring) {
  U64 len = 0;
  while(*cstring++) {
    len++;
  }
  
  return len;
}

fn_internal Str str_slice(Str base, U64 start, U64 len) {
  Assert(base.len >= start + len, "invalid string slice");
  return (Str) { .len = len, .txt = base.txt + start };
}

fn_internal Str str_from_cstr(char *cstring) {
  Str result = {
    .len = cstring_len(cstring),
    .txt = (U08 *)cstring
  };

  return result;
}

fn_internal Str str_trim(Str string) {
  U64 start = 0;
  For_U64(it, string.len) {
    if (char_is_whitespace(string.txt[it])) {
        start++;
    } else {
      break;
    }
  }

  U64 end = string.len;
  For_U64_Reverse(it, string.len) {
    if (char_is_whitespace(string.txt[it])) {
      end--;
    } else {
      break;
    }
  }

  return str_slice(string, start, end - start);
}

fn_internal B32 str_equals(Str lhs, Str rhs) {
  B32 result = lhs.len == rhs.len;
  if (result) {
    For_U64(it, lhs.len) {
      if (lhs.txt[it] != rhs.txt[it]) {
        result = 0;
        break;
      }
    }
  }

  return result;
}

fn_internal B32 str_equals_any_case(Str lhs, Str rhs) {
  B32 result = lhs.len == rhs.len;
  if (result) {
    For_U64(it, lhs.len) {
      if (char_to_lower(lhs.txt[it]) != char_to_lower(rhs.txt[it])) {
        result = 0;
        break;
      }
    }
  }

  return result;
}

fn_internal B32 str_starts_with(Str base, Str start) {
  B32 result = base.len >= start.len;
  if (result) {
    result = str_equals(str_slice(base, 0, start.len), start);
  }

  return result;
}

fn_internal B32 str_starts_with_any_case(Str base, Str start) {
  B32 result = base.len >= start.len;
  if (result) {
    result = str_equals_any_case(str_slice(base, 0, start.len), start);
  }

  return result;
}

fn_internal B32 str_contains(Str base, Str sub) {
  B32 result = 0;
  if (sub.len <= base.len) {
    For_U64(it, base.len - sub.len) {
      result = (str_equals(str_slice(base, it, sub.len), sub));
      if (result) break;
    }
  }

  return result;
}

fn_internal B32 str_contains_any_case(Str base, Str sub) {
  B32 result = 0;
  if (sub.len <= base.len) {
    For_U64(it, base.len - sub.len) {
      result = (str_equals_any_case(str_slice(base, it, sub.len), sub));
      if (result) break;
    }
  }

  return result;
}

// NOTE(cmat): djb2, Dan Bernstein
fn_internal U64 str_hash(Str string) {
  U64 hash = 5381;
  For_U64(it, string.len) {
    hash = ((hash << 5) + hash) + string.txt[it];
  }

  return hash;
}

fn_internal I64 i64_from_str(Str value) {
  Not_Implemented;
  return 0;
}

fn_internal F64 f64_from_str(Str value) {
  Not_Implemented;
  return 0;
}

fn_internal B32 b32_from_str(Str value) {
  Not_Implemented;
  return 0;
}

fn_internal Codepoint codepoint_from_utf8(Str str_utf8, I32 *advance) {
  Codepoint result = -1;
  
  if (str_utf8.len) {
    U08 c0 = str_utf8.txt[0];

    if (c0 < 0x80) {  // NOTE(cmat): 0xxxxxxx
      result = c0;
      if (advance) {
        *advance += 1;
      }

    } else if ((c0 & 0xE0) == 0xC0) { // NOTE(cmat): 110xxxxx 10xxxxxx
      Assert(str_utf8.len >= 2, "utf8-decode missing 2 bytes");
      if (str_utf8.len >= 2) {
        result = c0 & 0x1F;
        result = (result << 6) | (str_utf8.txt[1] & 0x3F);
        if (advance) {
          *advance += 2;
        }
      }

    } else if ((c0 & 0xF0) == 0xE0) { // NOTE(cmat): 1110xxxxx 10xxxxx 10xxxxxx
      Assert(str_utf8.len >= 3, "utf8-decode missing 3 bytes");
      if (str_utf8.len >= 3) {
        result = c0 & 0x0F;
        result = (result << 6) | (str_utf8.txt[1] & 0x3F);
        result = (result << 6) | (str_utf8.txt[2] & 0x3F);
        if (advance) {
          *advance += 3;
        }
      }

    } else if ((c0 & 0xF8) == 0xF0) { // NOTE(cmat): 11110xxx 10xxxxx 10xxxxx 10xxxxxx
      Assert(str_utf8.len >= 4, "utf8-decode missing 4 bytes");
      if (str_utf8.len >= 4) {
        result = c0 & 0x07;
        result = (result << 6) | (str_utf8.txt[1] & 0x3F);
        result = (result << 6) | (str_utf8.txt[2] & 0x3F);
        result = (result << 6) | (str_utf8.txt[3] & 0x3F);
        if (advance) {
          *advance += 4;
        }
      }

    } else {
      Assert(0, "utf8-decode invalid sequence");
    }
  }

  return result;
}


// ------------------------------------------------------------
// #-- F32 Base Operations

fn_internal F32 f32_sin(F32 x) {

  // NOTE(cmat): wrap between [-pi, pi)
  x -= f32_2pi * f32_floor((x + f32_pi) / f32_2pi);

  // NOTE(cmat): wrap between [0, pi)
  F32 sign = f32_sign(x);
  x = f32_abs(x);

  // NOTE(cmat): wrap between [0, pi/2)
  if (x > f32_hpi) {
      x = f32_pi - x;
  }

  // NOTE(cmat): Minimax-based Approximation, degree 7.
  // Coefficients from: https://gist.github.com/publik-void/067f7f2fef32dbe5c27d6e215f824c91#sin-rel-error-minimized-degree-7
  F32 x2 = x * x;
  x = x *(0.999996615908002773079325846913220383 +
      x2*(-0.16664828381895056829366054140948866 +
      x2*(0.00830632522715989396465411782615901079 -
          0.00018363653976946785297280224158683484 * x2)));

  return sign * x;
}

// ------------------------------------------------------------
// #-- Local Time
fn_internal Local_Time local_time_from_unix_time(U64 unix_seconds, U64 unix_microseconds) {

  // NOTE(cmat): Derived math, it works.
  U64 until_today_seconds       = (unix_seconds % (3600LL * 24));
  U64 until_today_microseconds  = unix_microseconds;
  U64 until_1970_days           = unix_seconds / (3600LL * 24);
  U64 t                         = until_1970_days;
  U64 a                         = (4 * t + 102032) / 146097 + 15;
  U64 b                         = t + 2442113 + a - (a / 4);
  U64 c                         = (20 * b - 2442) / 7305;
  U64 d                         = b - 365 * c - (c / 4);
  U64 e                         = d * 1000 / 30601;
  U64 f                         = d - e * 30 - e * 601 / 1000;

  // NOTE(cmat): Handle January and February. Counted as month 13 & 14 of the previous year.
  if (e <= 13) {
    c -= 4716;
    e -= 1;
  } else {
    c -= 4715;
    e -= 13;
  }

  U32 current_year  = (U32)c;
  U08 current_month = (U08)e;
  U08 current_day   = (U08)f;

  Local_Time time = {
    .year         = current_year,
    .month        = current_month,
    .day          = current_day,
    .hours        = (U08)(until_today_seconds / 3600),
    .minutes      = (U08)((until_today_seconds / 60) % 60),
    .seconds      = (U08)(until_today_seconds % 60),
    .microseconds = (U16)(until_today_microseconds),
  };

  return time;
}

