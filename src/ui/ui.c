// (C) Copyright 2025 Matyas Constans
// Licensed under the MIT License (https://opensource.org/license/mit/)ui

typedef struct UI_Node_Entry {
  struct UI_Node_Entry *next;
  UI_Node               node;
} UI_Node_Entry;

var_global struct {
  Arena            node_arena;
  U32              node_hash_count;
  UI_Node_Entry  **node_hash;
  UI_Node         *active_parent;
  U64              next_id;
  Font             font;
  F32              font_height;
} UI_State = { };

fn_internal void ui_init(void) {
  zero_fill(&UI_State);
  arena_init(&UI_State.node_arena);

  UI_State.font            = font_load(&UI_State.node_arena, "noto_serif.png", "noto_serif.bin");
  UI_State.font_height     = 32;

  UI_State.node_hash_count = 2048;
  UI_State.node_hash       = arena_push_count(&UI_State.node_arena, UI_Node_Entry *, UI_State.node_hash_count);
  UI_State.active_parent   = 0;
  UI_State.next_id         = 0;
}

fn_internal UI_Node *ui_node_cache(Str key) {
  UI_Node *result      = 0;
  U64 hash_slot        = str_hash(key) % UI_State.node_hash_count;
  UI_Node_Entry *entry = UI_State.node_hash[hash_slot];
  
  if (!UI_State.node_hash[hash_slot]) {
    UI_State.node_hash[hash_slot] = arena_push_type(&UI_State.node_arena, UI_Node_Entry);
    result      = &UI_State.node_hash[hash_slot]->node;
    result->id  = UI_State.next_id++;
    result->key = key;
  } else {
    for (;;) {
      if (str_equals(entry->node.key, key)) {
        result = &entry->node;
        break;
      }

      if (!entry->next) {
        entry->next = arena_push_type(&UI_State.node_arena, UI_Node_Entry);
        result      = &entry->next->node;
        result->id  = UI_State.next_id++;
        result->key = key;
      }

      entry = entry->next;
    }
  }

  return result;
}

fn_internal void ui_node_parent_push(UI_Node *parent)  {
  UI_State.active_parent = parent;
}

fn_internal void ui_node_parent_pop() {
  Assert(UI_State.active_parent, "can't pop further");
  UI_State.active_parent = UI_State.active_parent->parent;
}

fn_internal UI_Node *ui_node_push(Str key, UI_Node_Flag flags) {
  UI_Node *node     = ui_node_cache(key);
  node->label       = node->key;

  U64 frame_index   = platform_display()->frame_index;
  node->first_frame = (frame_index - node->frame_index) != 1;
  node->frame_index = frame_index;

  UI_Node *parent   = UI_State.active_parent;
  node->parent      = parent;

  if (node->parent) {
    if (parent->first_child) {
      UI_Node *it = parent->first_child;
      for (;;) {
        it->last = node;
        if (!it->next) {
          it->next = node;
          break;
        }

        it = it->next;
      }
    } else {
      parent->first_child = node;
    }

    node->first       = parent->first_child;
    node->next        = 0;
    node->last        = node;
    node->first_child = 0;

  } else {
    node->first       = node;
    node->next        = 0;
    node->last        = node;
    node->first_child = 0;
  }

  node->flags = flags;

  zero_fill(&node->action);

  if (!node->first_frame) {
    V2F mouse = platform_input()->mouse.position;
    R2F region = node->solved_absolute_region;
    if (mouse.x >= region.min.x && mouse.y >= region.min.y && mouse.x <= region.max.x && mouse.y <= region.max.y) {
      if (platform_input()->mouse.left.down_first_frame) {
        node->action.clicked = 1;
      }
    }
  }

  return node;
}

fn_internal void ui_layout_solve_fixed(UI_Node *node) {
  if (node) {
    For_U32 (it, 2) {
      switch (node->input_size[it].type) {
        case UI_Size_Type_Pixels: {
          node->solved_size[it] = node->size_t[it] * node->input_size[it].value;
        } break;
   
        case UI_Size_Type_Fit_Text: {
          if (it == 0) {
            node->solved_size[it] = node->size_t[it] * font_text_width(&UI_State.font, node->label, UI_State.font_height) + 15.f; // TODO(cmat): temporary.
          } else {
            node->solved_size[it] = node->size_t[it] * 1.5f *  UI_State.font_height; // TODO(cmat): temporary.
          }
        } break;
      }
    }

    ui_layout_solve_fixed(node->first_child);
    if (node->first == node) {
      UI_Node *it = node->next;
      while (it) {
        ui_layout_solve_fixed(it);
        it = it->next;
      }
    }
  }
}

fn_internal void ui_layout_solve_upwards(UI_Node *node) {
  if (node) {
    For_U32 (it, 2) {
      switch (node->input_size[it].type) {
        case UI_Size_Type_Parent_Ratio: {
          if (node->parent) {
            node->solved_size[it] = node->size_t[it] * node->input_size[it].value * node->parent->solved_size[it];
          }
        } break;
      }
    }

    ui_layout_solve_upwards(node->first_child);
    if (node->first == node) {
      UI_Node *it = node->next;
      while (it) {
        ui_layout_solve_upwards(it);
        it = it->next;
      }
    }
  }
}

fn_internal void ui_layout_solve_downwards(UI_Node *node) {
  if (node) {
    ui_layout_solve_downwards(node->first_child);
    if (node->first == node) {
      UI_Node *it = node->next;
      while (it) {
        ui_layout_solve_downwards(it);
        it = it->next;
      }
    }

    For_U32 (it, 2) {
      switch (node->input_size[it].type) {
        case UI_Size_Type_Children_Sum: {

          F32 sum = 0;
          UI_Node *it_node = node->first_child;
          while (it_node) {
            sum += it_node->solved_size[it];
            it_node = it_node->next;
          }

          node->solved_size[it] = node->size_t[it] * sum;
        } break;
      }
    }
  }
}

fn_internal V2F ui_layout_solve_position(UI_Node *node, V2F relative_position) {
  if (node) {

    if (node->flags & UI_Node_Flag_Layout_Float_X) {
      node->solved_relative_position.x = node->input_float_relative_x;
    } else {
      node->solved_relative_position.x = relative_position.x;
    }

    if (node->flags & UI_Node_Flag_Layout_Float_Y) {
      node->solved_relative_position.y = node->input_float_relative_y;
    } else {
      node->solved_relative_position.y = relative_position.y;
    }

    U32 input_layout_axis = 0;
    if (node->parent) input_layout_axis = node->parent->input_layout_axis;
    relative_position.dat[input_layout_axis] += node->solved_size[input_layout_axis];

    ui_layout_solve_position(node->first_child, v2f(0, 0));
    if (node->first == node) {
      UI_Node *it = node->next;
      while (it) {
        if (input_layout_axis == 0)
        relative_position.x += 5.f; // TODO(cmat): fix this

        relative_position = ui_layout_solve_position(it, relative_position);
        it = it->next;
      }
    }
  }

  return relative_position;
}

fn_internal void ui_layout_solve(UI_Node *root) {
  ui_layout_solve_fixed     (root);
  ui_layout_solve_upwards   (root);
  ui_layout_solve_downwards (root);
  ui_layout_solve_position  (root, v2f(0, 0));
}

var_global Random_Seed ui_random = { };

fn_internal void ui_node_draw(UI_Node *node, V2F draw_at, F32 opacity, R2I clip_region) {
  if (node) {
    draw_at = v2f_add(draw_at, node->solved_relative_position);
    opacity = node->opacity_t * opacity;

    V4F color = { };
    color.rgb = rgb_from_hsv(v3f_add(node->color_hsv_background, v3f(.0f, .0f, 0.04f * node->hot_t))),
    color.a   = opacity;

    V2F resolution = platform_display()->resolution;
    V2F size       = v2f(node->solved_size[0], node->solved_size[1]);
    V2F position   = v2f(draw_at.x, resolution.y - draw_at.y - size.y);
 
    node->solved_absolute_region = (R2F) {
      .min = position,
      .max = v2f_add(position, size),
    };

    if (node->flags & UI_Node_Flag_Draw_Clip_Content) {
      clip_region = r2i(position.x, position.y, position.x + size.x, position.y + size.y);
    }

    g2_clip_region(clip_region);

    if (node->flags & UI_Node_Flag_Draw_Border) {
      V3F border_color = rgb_from_hsv(node->color_hsv_border);
      g2_draw_rect(position, size, .color = v4f(border_color.r, border_color.g, border_color.b, opacity));
      g2_draw_rect(v2f_add(position, v2f(1, 1)), v2f_sub(size, v2f(2, 2)), .color = color);
    } else {
      g2_draw_rect(position, size, .color = color);
    }

    if (node->flags & UI_Node_Flag_Draw_Label) {
      // TODO(cmat): temporary
      g2_draw_text(node->label, &UI_State.font, v2f_add(position, v2f(7.5f, .25f * UI_State.font_height)), UI_State.font_height, .color = v4f(1, 1, 1, opacity));
    }

    if (node->flags & UI_Node_Flag_Draw_Content_Hook) {
      g2_submit_draw();
      node->draw_content_hook(clip_region, node->draw_content_user_data);
    }

    ui_node_draw(node->first_child, draw_at, opacity, clip_region);
    if (node->first == node) {
      UI_Node *it = node->next;
      while (it) {
        ui_node_draw(it, draw_at, opacity, clip_region);
        it = it->next;
      }
    }
  }
}

fn_internal void ui_animation_solve(UI_Node *node) {
 if (node) {
    ui_animation_solve(node->first_child);
    if (node->first == node) {
      UI_Node *it = node->next;
      while (it) {
        ui_animation_solve(it);
        it = it->next;
      }
    }

    // NOTE(cmat): Size animations.
    if (node->flags & UI_Node_Flag_Anim_Size_X) {
      if (node->first_frame) {
        node->size_t[0] = 0.f;
      }

      node->size_t[0] = node->size_t[0] + (1.f - node->size_t[0]) * 15.0f * platform_display()->frame_delta;
    } else {
      node->size_t[0] = 1.f;
    }
 
    if (node->flags & UI_Node_Flag_Anim_Size_Y) {
      if (node->first_frame) {
        node->size_t[1] = 0.f;
      }

      node->size_t[1] = node->size_t[1] + (1.f - node->size_t[1]) * 15.0f * platform_display()->frame_delta;
    } else {
      node->size_t[1] = 1.f;
    }

    // NOTE(cmat): Fade-in animation.
    if (node->flags & UI_Node_Flag_Anim_Fade_In) {
      if (node->first_frame) {
        node->opacity_t = 0.f;
      }

      node->opacity_t = node->opacity_t + (1.f - node->opacity_t) * 10.0f * platform_display()->frame_delta;
    } else {
      node->opacity_t = 1.f;
    }

    // NOTE(cmat): Hot animation.ui.c
    R2F region = node->solved_absolute_region;
    V2F mouse = platform_input()->mouse.position;
    if (mouse.x >= region.min.x && mouse.y >= region.min.y &&
        mouse.x <= region.max.x && mouse.y <= region.max.y) {

      node->hot_t = node->hot_t + (1.f - node->hot_t) * 8.5f * platform_display()->frame_delta;
    } else {
      node->hot_t = node->hot_t + (0.0f - node->hot_t) * 8.5f * platform_display()->frame_delta;
    }
  }
}

fn_internal void ui_frame_flush(UI_Node *root_node) {
  ui_animation_solve(root_node);
  ui_layout_solve(root_node);

  // NOTE(cmat): Draw UI.
  ui_random = 4123;
  ui_node_draw(root_node, v2f(0, 0), 1.f, G2_Clip_None);

  Assert(UI_State.active_parent == 0, "mismatched ui_node_parent_push(), pop missing");
}
