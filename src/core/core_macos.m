// (C) Copyright 2025 Matyas Constans
// Licensed under the MIT License (https://opensource.org/license/mit/)

cb_global Core_Context macos_context = {};

cb_function Core_Context *core_context(void) {
  return &macos_context;
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
  syscall(SYS_exit, 1);
}

cb_function Local_Time core_local_time(void) {
  clock_serv_t maccore_clock;
  mach_timespec_t time_spec;
  host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &maccore_clock);
  clock_get_time(maccore_clock, &time_spec);
  mach_port_deallocate(mach_task_self(), maccore_clock);

  CFTimeZoneRef  time_zone     = CFTimeZoneCopySystem();
  CFAbsoluteTime absolute_time = (CFAbsoluteTime)(time_spec.tv_sec - kCFAbsoluteTimeIntervalSince1970);
  CFTimeInterval time_offset   = CFTimeZoneGetSecondsFromGMT(time_zone, absolute_time);
  CFRelease(time_zone);
  
  Local_Time local_time = local_time_from_unix_time(time_spec.tv_sec + time_offset, time_spec.tv_nsec);
  return local_time;  
}

cb_function U08 *core_memory_reserve(U64 bytes) {
  mach_port_t   task    = mach_task_self();
  vm_address_t  address = 0;
  kern_return_t error   = vm_allocate(task, &address, (size_t)bytes, VM_FLAGS_ANYWHERE);

  if (error != KERN_SUCCESS) {
    core_panic(str_lit("failed to reserve virtual memory"));
  }

  return (U08 *)address;
}

cb_function void core_memory_unreserve(void *virtual_base, U64 bytes) {
  mach_port_t task = mach_task_self();
  vm_deallocate(task, (vm_address_t)virtual_base, bytes);
}

cb_function void core_memory_commit(void *virtual_base, U64 bytes, Core_Commit_Flag mode) {
  mach_port_t   task  = mach_task_self();
  vm_prot_t     prot  = 0;
  kern_return_t error = 0;
    
  if (mode & Core_Commit_Flag_Read)       prot |= VM_PROT_READ;
  if (mode & Core_Commit_Flag_Write)      prot |= VM_PROT_WRITE;
  if (mode & Core_Commit_Flag_Executable) prot |= VM_PROT_EXECUTE;

  error = vm_protect(task, (vm_address_t)virtual_base, bytes, 0, prot);
  if (error != KERN_SUCCESS) {
    core_panic(str_lit("failed to commit memory"));
  }
}

cb_function void core_memory_uncommit(void *virtual_base, U64 bytes) {
  mach_port_t   task  = mach_task_self();
  kern_return_t error = vm_protect(task, (vm_address_t)virtual_base, bytes, 0, VM_PROT_NONE);
  if (error != KERN_SUCCESS) {
    core_panic(str_lit("failed to uncommit physical memory"));
  }
}

// NOTE(cmat): MacOS entry point.
int main(int argc, char **argv) {
  size_t cpu_name_len = 0;
  sysctlbyname("machdep.cpu.brand_string", 0, &cpu_name_len, 0, 0);

  U08 cpu_name_txt[256] = { };
  if (cpu_name_len <= sarray_len(cpu_name_txt)) {
    sysctlbyname("machdep.cpu.brand_string", cpu_name_txt, &cpu_name_len, 0, 0);
  }

  zero_fill(&macos_context);
  @autoreleasepool {
    macos_context.cpu_name             = str(cpu_name_len, (I08 *)cpu_name_txt);
    macos_context.cpu_logical_cores    = (U64)[[NSProcessInfo processInfo] processorCount];
    macos_context.ram_capacity_bytes   = (U64)[[NSProcessInfo processInfo] physicalMemory];
    macos_context.mmu_page_bytes       = (U64)getpagesize();
  }

  core_entry_point((I32)argc, argv);
}
