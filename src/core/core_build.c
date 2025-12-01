// (C) Copyright 2025 Matyas Constans
// Licensed under the MIT License (https://opensource.org/license/mit/)

#include "core.c"

#if OS_MACOS
# include <mach/mach.h>
# include <mach/clock.h>
# include <mach/mach_time.h>
# include <Foundation/Foundation.h>
# include <Foundation/NSProcessInfo.h>
# include <unistd.h>
# include <sys/syscall.h>
# include <sys/sysctl.h>

# include "core_macos.m"

#elif OS_LINUX
# include <unistd.h>
# include <fcntl.h>

# include <sys/syscall.h>
# include <sys/time.h>
# include <sys/sysinfo.h>
# include <sys/stat.h>
# include <sys/mman.h>

# include <linux/io_uring.h>

# include "core_linux.c"

#elif OS_WASM

# include "core_wasm.c"

#endif
