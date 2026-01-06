// (C) Copyright 2025 Matyas Constans
// Licensed under the MIT License (https://opensource.org/license/mit/)

var_global CO_Context linux_context = {};

fn_internal CO_Context *co_context(void) {
  return &linux_context;
}

fn_internal void co_stream_write(Str buffer, CO_Stream stream) {
  I32 unix_handle = 0;
  
  switch(stream) {
    case CO_Stream_Standard_Output:  { unix_handle = 1; } break;
    case CO_Stream_Standard_Error:   { unix_handle = 2; } break;
    Invalid_Default;
  }

  write(unix_handle, buffer.txt, buffer.len);
}

fn_internal void co_panic(Str reason) {
  co_stream_write(str_lit("## PANIC -- Aborting Execution ##\n"), CO_Stream_Standard_Error);
  co_stream_write(reason, CO_Stream_Standard_Error);
  co_stream_write(str_lit("\n"), CO_Stream_Standard_Error);
  exit(1);
}

fn_internal Local_Time co_local_time(void) {
  struct timeval tv;
  gettimeofday(&tv, 0);
  Local_Time result = local_time_from_unix_time((U64)tv.tv_sec, (U64)tv.tv_usec);
  return result;
}

fn_internal U08 *co_memory_reserve(U64 bytes) {
  void *address = mmap(0, bytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (address == (void*)-1) {
    co_panic(str_lit("virtual memory reserve failed"));
  }

  return (U08 *)address;
}

fn_internal void co_memory_unreserve(void *virtual_base, U64 bytes) {
  if (munmap(virtual_base, bytes)) {
    co_panic(str_lit("virtual memory unreserve failed"));
  }
}

fn_internal void co_memory_commit(void *virtual_base, U64 bytes, CO_Commit_Flag mode) {
  unsigned long prot = 0;
  if (mode & CO_Commit_Flag_Read)       prot |= PROT_READ;
  if (mode & CO_Commit_Flag_Write)      prot |= PROT_WRITE;
  if (mode & CO_Commit_Flag_Executable) prot |= PROT_EXEC;

  if (mprotect(virtual_base, bytes, prot)) {
    co_panic(str_lit("virtual memory commit failed"));
  }
}

fn_internal void co_memory_uncommit(void *virtual_base, U64 bytes) {
  if (mprotect(virtual_base, bytes, PROT_NONE)) {
    co_panic(str_lit("virtual memory uncommit failed"));
  }
}

fn_internal B32 co_directory_create(Str folder_path) {
  // TODO(cmat): Handle this better.
  I08 buffer[4096 + 1];
  memory_copy(buffer, folder_path.txt, u64_min(folder_path.len, 4096));
  buffer[u64_min(folder_path.len, 4096)] = 0;

  B32 result = mkdir((const char *)buffer, 0755) >= 0;
  return result;
}

fn_internal B32 co_directory_delete(Str folder_path) {
  // TODO(cmat): Handle this better.
  I08 buffer[4096 + 1];
  memory_copy(buffer, folder_path.txt, u64_min(folder_path.len, 4096));
  buffer[u64_min(folder_path.len, 4096)] = 0;

  B32 result = rmdir((const char *)buffer) >= 0;
  return result;
}

fn_internal CO_File co_file_open(Str file_path, CO_File_Access_Flag flags) {
  // TODO(cmat): Handle this better.
  I08 buffer[4096 + 1];
  memory_copy(buffer, file_path.txt, u64_min(file_path.len, 4096));
  buffer[u64_min(file_path.len, 4096)] = 0;

  I32 mode = 0;
  if ((flags & CO_File_Access_Flag_Read) && (flags & CO_File_Access_Flag_Write)) {
    mode |= O_RDWR;
  } else {
    if (flags & CO_File_Access_Flag_Read)       mode |= O_RDONLY;
    else if (flags & CO_File_Access_Flag_Write) mode |= O_WRONLY;
  }

  if (flags & CO_File_Access_Flag_Create)    mode |= O_CREAT;
  if (flags & CO_File_Access_Flag_Truncate)  mode |= O_TRUNC;
  if (flags & CO_File_Access_Flag_Append)    mode |= O_APPEND;

  I32 fd = open((const char *)buffer, mode, 0644);
  CO_File result = {
    .os_handle_1 = (U64)fd,
  };

  return result;
}

fn_internal U64 co_file_size(CO_File *file) {
  U64 result = 0;
  I32 file_handle = (I32)file->os_handle_1;

  struct stat st;
  if (fstat(file_handle, &st) == 0) {
    result = (U64)st.st_size;
  }
  
  return result;
}

fn_internal void co_file_write(CO_File *file, U64 offset, U64 bytes, void *data) {
  I32 file_handle = (I32)file->os_handle_1;
  pwrite(file_handle, data, bytes, offset);
}

fn_internal void co_file_read(CO_File *file, U64 offset, U64 bytes, void *data) {
  I32 file_handle = (I32)file->os_handle_1;
  pread(file_handle, data, bytes, offset);
}

fn_internal void co_file_close(CO_File *file) {
  I32 file_handle = (I32)(U64)file->os_handle_1;
  close(file_handle);

  file->os_handle_1 = 0;
}

// NOTE(cmat): Linux entry point.
int main(int argc, char **argv) {
  
#if ARCH_X86

  // NOTE(cmat): Get CPU name.
  U32 eax, ebx, ecx, edx;
  U08 cpu_id[48];
  U08 *cpu_id_at = cpu_id;

  For_U32(it, 3) {
    eax = 0x80000002 + it;
    __asm__ volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(eax));

    memory_copy(cpu_id_at + 0,  &eax, 4);
    memory_copy(cpu_id_at + 4,  &ebx, 4);
    memory_copy(cpu_id_at + 8,  &ecx, 4);
    memory_copy(cpu_id_at + 12, &edx, 4);
    cpu_id_at += 16;
  }

  linux_context.cpu_name = str(sarray_len(cpu_id), cpu_id);
  linux_context.cpu_name = str_trim(linux_context.cpu_name);

#else
  linux_context.cpu_name = str_lit("");
  Not_Implemented;
#endif

  linux_context.cpu_logical_cores  = (U64)sysconf(_SC_NPROCESSORS_ONLN);

  struct sysinfo info = {};
  if (sysinfo(&info) == 0) {
    linux_context.ram_capacity_bytes = (U64)info.totalram * (U64)info.mem_unit;
  }

  linux_context.mmu_page_bytes = (U64)sysconf(_SC_PAGESIZE);

  co_entry_point((I32)argc, argv);
}
