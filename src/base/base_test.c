// (C) Copyright 2025 Matyas Constans
// Licensed under the MIT License (https://opensource.org/license/mit/)

cb_function void test_base_allocation(void) {
  Random_Seed rng = 0x0C0FEFE;
  
  Arena_Flag flag_combinations[2] = {
    Arena_Flag_Allow_Chaining,
    Arena_Flag_Allow_Chaining | Arena_Flag_Backtrack_Before_Chaining
  };

  Str flag_combination_strings[2] = {
    str_lit("Arena_Flag_Allow_Chaining"),
    str_lit("Arena_Flag_Allow_Chaining | Arena_Flag_Backtrack_Before_Chaining")
  };

  log_zone_start("allocation testing", "");
  For_I32(combination_it, sarray_len(flag_combinations)) {
    log_zone_start("testing flag combination - %.*s", str_expand(flag_combination_strings[combination_it]));

    // NOTE(cmat): Arena allocator.
    // #--
    Arena arena = {};
    Defer_Scope(arena_init(&arena, .flags = flag_combinations[combination_it]), arena_destroy(&arena)) {
      For_I32(loop_it, 10) {
        For_I32(alloc_it, 10) {
            U64 random_bytes = 0;
            switch (u64_random(&rng, 1, 3)) {
              case 1: { random_bytes = u64_random(&rng, 10, u64_megabytes(120));  } break;
              case 2: { random_bytes = u64_random(&rng, 1,  u64_kilobytes(1));    } break;
              case 3: { random_bytes = u64_random(&rng, 1, 7);                    } break;
              Invalid_Default;
            }

            memory_fill(arena_push_size(&arena, random_bytes), 123, random_bytes);
        }
        
        arena_clear(&arena);
      };
    }

    log_info("serial allocations - ok");

    // NOTE(cmat): Rollback system.
    // #--
    Defer_Scope(arena_init(&arena, .flags = flag_combinations[combination_it]), arena_destroy(&arena)) {

      // NOTE(cmat): Make a random allocation, then allocate some temporary memory.
      U64 random_bytes = u64_random(&rng, 10, u64_kilobytes(10));
      memory_fill(arena_push_size(&arena, random_bytes), 123, random_bytes);

      For_I32(temp_it,10) {
        Arena_Temp_Scope(&arena, temp_1) {
          random_bytes = u64_random(&rng, 10, u64_megabytes(16));
          memory_fill(arena_push_size(&arena, random_bytes), 123, random_bytes);

          Arena_Temp_Scope(&arena, temp_2) {
            random_bytes = u64_random(&rng, 10, u64_megabytes(3));
            memory_fill(arena_push_size(&arena, random_bytes), 123, random_bytes);
          }
        }
      }
    }

    log_info("temporary allocations - ok");
    log_zone_end();
  }

  // NOTE(cmat): Scratch allocator.
  // #--
  For_I32(it, 10) {
    Scratch scratch = { };
    Scratch_Scope(&scratch, 0) {
      U64 random_bytes = u64_random(&rng, 10, u64_megabytes(7));
      memory_fill(arena_push_size(scratch.arena, random_bytes), 123, random_bytes);
    }
  }

  log_info("scratch allocations - ok"); 
  log_zone_end();
}

cb_function void test_base_all(void) {
  Log_Zone_Scope("testing base subsystem") {
    test_base_allocation();
  }
}
