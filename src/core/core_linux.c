// (C) Copyright 2025 Matyas Constans
// Licensed under the MIT License (https://opensource.org/license/mit/)

cb_global Core_Context linux_context = {};

cb_function Core_Context *core_context(void) {
  return &linux_context;
}

cb_function void core_stream_write(Str buffer, Core_Stream stream) {
  I32 unix_handle = 0;
  
  switch(stream) {
    case Core_Stream_Standard_Output:  { unix_handle = 1; } break;
    case Core_Stream_Standard_Error:   { unix_handle = 2; } break;
    Invalid_Default;
  }

  write(unix_handle, buffer.txt, buffer.len);
}

cb_function void core_panic(Str reason) {
  core_stream_write(str_lit("## PANIC -- Aborting Execution ##\n"), Core_Stream_Standard_Error);
  core_stream_write(reason, Core_Stream_Standard_Error);
  core_stream_write(str_lit("\n"), Core_Stream_Standard_Error);
  exit(1);
}

cb_function Local_Time core_local_time(void) {
  struct timeval tv;
  gettimeofday(&tv, 0);
  Local_Time result = local_time_from_unix_time((U64)tv.tv_sec, (U64)tv.tv_usec);
  return result;
}

cb_function U08 *core_memory_reserve(U64 bytes) {
  void *address = mmap(0, bytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (address == (void*)-1) {
    core_panic(str_lit("virtual memory reserve failed"));
  }

  return (U08 *)address;
}

cb_function void core_memory_unreserve(void *virtual_base, U64 bytes) {
  if (munmap(virtual_base, bytes)) {
    core_panic(str_lit("virtual memory unreserve failed"));
  }
}

cb_function void core_memory_commit(void *virtual_base, U64 bytes, Core_Commit_Flag mode) {
  unsigned long prot = 0;
  if (mode & Core_Commit_Flag_Read)       prot |= PROT_READ;
  if (mode & Core_Commit_Flag_Write)      prot |= PROT_WRITE;
  if (mode & Core_Commit_Flag_Executable) prot |= PROT_EXEC;

  if (mprotect(virtual_base, bytes, prot)) {
    core_panic(str_lit("virtual memory commit failed"));
  }
}

cb_function void core_memory_uncommit(void *virtual_base, U64 bytes) {
  if (mprotect(virtual_base, bytes, PROT_NONE)) {
    core_panic(str_lit("virtual memory uncommit failed"));
  }
}

cb_function B32 core_directory_create(Str folder_path) {
  // TODO(cmat): Handle this better.
  I08 buffer[4096 + 1];
  memory_copy(buffer, folder_path.txt, u64_min(folder_path.len, 4096));
  buffer[u64_min(folder_path.len, 4096)] = 0;

  B32 result = mkdir((const char *)buffer, 0755) >= 0;
  return result;
}

cb_function B32 core_directory_delete(Str folder_path) {
  // TODO(cmat): Handle this better.
  I08 buffer[4096 + 1];
  memory_copy(buffer, folder_path.txt, u64_min(folder_path.len, 4096));
  buffer[u64_min(folder_path.len, 4096)] = 0;

  B32 result = rmdir((const char *)buffer) >= 0;
  return result;
}

cb_function Core_File core_file_open(Str file_path, Core_File_Access_Flag flags) {
  // TODO(cmat): Handle this better.
  I08 buffer[4096 + 1];
  memory_copy(buffer, file_path.txt, u64_min(file_path.len, 4096));
  buffer[u64_min(file_path.len, 4096)] = 0;

  I32 mode = 0;
  if ((flags & Core_File_Access_Flag_Read) && (flags & Core_File_Access_Flag_Write)) {
    mode |= O_RDWR;
  } else {
    if (flags & Core_File_Access_Flag_Read)       mode |= O_RDONLY;
    else if (flags & Core_File_Access_Flag_Write) mode |= O_WRONLY;
  }

  if (flags & Core_File_Access_Flag_Create)    mode |= O_CREAT;
  if (flags & Core_File_Access_Flag_Truncate)  mode |= O_TRUNC;
  if (flags & Core_File_Access_Flag_Append)    mode |= O_APPEND;

  I32 fd = open((const char *)buffer, mode, 0644);
  Core_File result = {
    .os_handle_1 = (U64)fd,
  };

  return result;
}

cb_function U64 core_file_size(Core_File *file) {
  U64 result = 0;
  I32 file_handle = (I32)file->os_handle_1;

  struct stat st;
  if (fstat(file_handle, &st) == 0) {
    result = (U64)st.st_size;
  }
  
  return result;
}

cb_function void core_file_write(Core_File *file, U64 offset, U64 bytes, void *data) {
  I32 file_handle = (I32)file->os_handle_1;
  pwrite(file_handle, data, bytes, offset);
}

cb_function void core_file_read(Core_File *file, U64 offset, U64 bytes, void *data) {
  I32 file_handle = (I32)file->os_handle_1;
  pread(file_handle, data, bytes, offset);
}

cb_function void core_file_close(Core_File *file) {
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

  linux_context.cpu_name = str(sarray_len(cpu_id), (I08 *)cpu_id);
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

  core_entry_point((I32)argc, argv);
}
