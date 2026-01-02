// (C) Copyright 2025 Matyas Constans
// Licensed under the MIT License (https://opensource.org/license/mit/)

__attribute__((export_name("wasm_arena_push_size")))
fn_entry UAddr wasm_arena_push_size(Arena *arena, U32 bytes) {
  U08 *ptr = arena_push_size(arena, bytes);
  return (UAddr)ptr;
}

enum {
  HTTP_Status_Failed        = 0,
  HTTP_Status_Done          = 1,
  HTTP_Status_In_Progress   = 2,
};

#pragma pack(push, 1)
typedef struct HTTP_Request {
  U32  status;
  U32  bytes_downloaded;
  U32  bytes_total;
  U08 *bytes_data;
} HTTP_Request;
#pragma pack(pop)


fn_external void js_http_request_send(HTTP_Request *request, Arena *arena, U32 url_len, char *url_txt);

fn_internal void http_request_send(HTTP_Request *request, Arena *arena, Str url) {
  js_http_request_send(request, arena, (U32)url.len, (char *)url.txt);
}
