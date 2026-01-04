// (C) Copyright 2025 Matyas Constans
// Licensed under the MIT License (https://opensource.org/license/mit/)

var_global struct {
  Arena         arena;
  U64           hash_count;
  UI_Node_List *hash_array;

  // NOTE(cmat): We use a stack for parents,
  // since it's possible to suddenly switch to a different overlay
  // node (instead of the root node), invalidating a strategy where
  // we refer to node->tree.parent.
  U32            parent_stack_cap;
  U32            parent_stack_len;
  UI_Node      **parent_stack;

  U32           font_stack_cap;
  U32           font_stack_at; 
  FO_Font     **font_stack;

  UI_Node      *root;

  B32           next_overlay;
  UI_Node_List  overlay_list;

} UI_State = { };

#define UI_Font_Scope(font_ptr_) Defer_Scope(ui_font_push(font_ptr_), ui_font_pop())

fn_internal void ui_font_push(FO_Font *font) {
  Assert(UI_State.font_stack_at < UI_State.font_stack_cap, "ui_font_push() exceeding stack size");
  If_Likely (UI_State.font_stack_at < UI_State.font_stack_cap) {
    UI_State.font_stack[++UI_State.font_stack_at] = font;
  }
}

fn_internal void ui_font_pop(void) {
  Assert(UI_State.font_stack_at > 0, "ui_font_pop() popping default font");
  If_Likely (UI_State.font_stack_at > 0) {
    UI_State.font_stack_at -= 1;
  }
}

fn_internal FO_Font *ui_font_current(void) {
  return UI_State.font_stack[UI_State.font_stack_at];
}

fn_internal void ui_init(FO_Font *font) {
  zero_fill(&UI_State);

  arena_init(&UI_State.arena);
  UI_State.hash_count = 1024;
  UI_State.hash_array = arena_push_count(&UI_State.arena, UI_Node_List, UI_State.hash_count);

  UI_State.font_stack_cap = 32;
  UI_State.font_stack_at  = 0;
  UI_State.font_stack     = arena_push_count(&UI_State.arena, FO_Font *, UI_State.font_stack_cap);
  UI_State.font_stack[0]  = font;

  UI_State.parent_stack_cap = 2048;
  UI_State.parent_stack_len = 0;
  UI_State.parent_stack     = arena_push_count(&UI_State.arena, UI_Node *, UI_State.parent_stack_cap);
}

fn_internal Str ui_label_from_key(UI_Key key) {
  Str result = key.label;
  return result;
}

fn_internal UI_Node *ui_cache(UI_Key key) {
  U64 bucket_index   = key.id % UI_State.hash_count;
  UI_Node_List *list = UI_State.hash_array + bucket_index;

  UI_Node *result = 0;
  if (!list->first) {
    list->first = arena_push_type(&UI_State.arena, UI_Node);
    list->last  = list->first;

    result      = list->last;
    result->key = (UI_Key) { .id = key.id, .label = arena_push_str(&UI_State.arena, key.label) };

    log_debug("Created UI element '%.*s' with id # %u", str_expand(result->key.label), result->key.id);

  } else {
    UI_Node *entry = list->first;
    while (entry) {
      if (entry->key.id == key.id) {
        result = entry;
        break;
      }

      if (!entry->hash_next) {   
        list->last->hash_next  = arena_push_type(&UI_State.arena, UI_Node);
        list->last             = list->last->hash_next;
        result                 = list->last;
        result->key            = (UI_Key) { .id = key.id, .label = arena_push_str(&UI_State.arena, key.label) };

        log_debug("(Hash-Collision) Created UI element '%.*s' with id # %u", str_expand(result->key.label), result->key.id);
      }

      entry = entry->hash_next;
    }
  }

  return result;
}

#define UI_Parent_Scope(node_) Defer_Scope(ui_parent_push(node_), ui_parent_pop())

fn_internal void ui_parent_push(UI_Node *parent) {
  Assert(UI_State.parent_stack_len < UI_State.parent_stack_cap, "parent_push limit exceeded");
  If_Likely (UI_State.parent_stack_len < UI_State.parent_stack_cap) {
    UI_State.parent_stack[UI_State.parent_stack_len++] = parent;
  }
}

fn_internal void ui_parent_pop(void) {
  Assert(UI_State.parent_stack_len > 0, "can't pop further");
  If_Likely (UI_State.parent_stack_len > 0) {
    UI_State.parent_stack_len--;
  }
}

fn_internal UI_Node *ui_parent_current(void) {
  UI_Node *result = 0;

  if (UI_State.parent_stack_len) {
    result = UI_State.parent_stack[UI_State.parent_stack_len - 1];
  }

  return result;
}

fn_internal void ui_node_update_tree(UI_Node *node, UI_Node *parent) {
  UI_Node_Tree *tree = &node->tree;

  tree->parent = parent;
  if (tree->parent) {
    if (parent->tree.first_child) {
      UI_Node *it = parent->tree.first_child;
      for (;;) {
        it->tree.last = node;
        if (!it->tree.next) {
          it->tree.next = node;
          break;
        }

        it = it->tree.next;
      }
    } else {
      parent->tree.first_child = node;
    }

    node->tree.first = parent->tree.first_child;
    node->tree.last  = node;
  } else {
    node->tree.first = node;
    node->tree.last  = node;
  }

  node->tree.next         = 0;
  node->tree.first_child  = 0;
}

fn_internal void ui_node_update_response(UI_Node *node) {
  zero_fill(&node->response);

  V2F mouse  = platform_input()->mouse.position;
  R2F region = node->solved.region_absolute;

  if (r2f_contains_v2f(region, mouse)) {
    if (node->flags & UI_Flag_Response_Hover) {
      node->response.hover = 1;
    }

    if (platform_input()->mouse.left.down) {
      if (platform_input()->mouse.left.down_first_frame) {
        if (node->flags & UI_Flag_Response_Press) {
          node->response.press = 1;
        }
      }

      if (node->flags & UI_Flag_Response_Down) {
        node->response.down = 1;
      } 
    }
  }
}

fn_internal void ui_node_update_animation(UI_Node *node) {
  UI_Animation *anim = &node->animation;

  U32 frame_index = platform_display()->frame_index;

  // NOTE(cmat): If there were skipped frames, that is,
  // the UI component was hidden, we invalidate the animation cache.
  if (frame_index != node->frame_index + 1) {
    zero_fill(anim);
  }

  node->frame_index = frame_index;

  F32 refresh_rate_coeff = platform_display()->frame_delta;

  anim->spawn_t = f32_exp_smoothing(anim->spawn_t, 1.f,                  f32_min(1.f, refresh_rate_coeff * 15.f));
  anim->hover_t = f32_exp_smoothing(anim->hover_t, node->response.hover, f32_min(1.f, refresh_rate_coeff * 15.f));
  anim->down_t  = f32_exp_smoothing(anim->down_t, node->response.down,   f32_min(1.f, refresh_rate_coeff * 15.f));

#if 1
  anim->spawn_t = anim->spawn_t > .99f ? 1.f : anim->spawn_t;
  anim->spawn_t = anim->spawn_t < .01f ? 0.f : anim->spawn_t;

  anim->hover_t = anim->hover_t > .99f ? 1.f : anim->hover_t;
  anim->hover_t = anim->hover_t < .01f ? 0.f : anim->hover_t;

  anim->down_t = anim->down_t > .99f ? 1.f : anim->down_t;
  anim->down_t = anim->down_t < .01f ? 0.f : anim->down_t;
#endif
}

fn_internal UI_ID ui_node_id(Str label, UI_ID parent_id) {
  UI_ID hash = 0;
  Scratch scratch = { };
  Scratch_Scope(&scratch, 0) {
    U32  bytes = label.len + sizeof(UI_ID);
    U08 *data  = arena_push_size(scratch.arena, bytes);

    memory_copy(data, &parent_id, sizeof(UI_ID));
    memory_copy(data + sizeof(UI_ID), label.txt, label.len);

    hash = crc32(bytes, data);
  }

  return hash;
}

fn_internal void ui_set_next_overlay(void) {
  UI_State.next_overlay = 1;
}

fn_internal UI_Node *ui_node_push(Str label, UI_Flags flags) {
  UI_Node *parent = 0;

  // NOTE(cmat): If it's an overlay, it doesn't have any parents
  // (since an overlay node is considered a 'root' node)
  if (!UI_State.next_overlay) {
    parent = ui_parent_current();
  }

  UI_ID    parent_id = parent ? parent->key.id : 0;
  UI_ID    id        = ui_node_id(label, parent_id);
  UI_Key   key       = (UI_Key) { .id = id, .label = label };
  UI_Node *node      = ui_cache(key);

  node->flags       = flags;
  node->draw.font   = ui_font_current();

  ui_node_update_response   (node);
  ui_node_update_tree       (node, parent);
  ui_node_update_animation  (node);

  // NOTE(cmat): If it's an overlay, push on the overlay stack.
  if (UI_State.next_overlay) {
    UI_Node_List *list = &UI_State.overlay_list;
    queue_push_ext(list->first, list->last, node, overlay_next);
  }

  UI_State.next_overlay = 0;
  return node;
}

// ------------------------------------------------------------
// #-- NOTE(cmat): Layout solver

fn_internal void ui_solve_label(UI_Node *node) {
  if (node) {
    for (UI_Node *it = node->tree.first_child; it; it = it->tree.next) {
      ui_solve_label(it);
    }

    node->solved.label = ui_label_from_key(node->key);
  }
}

// TODO(cmat): I don't like how we have to call ui_size_animate inside of each ui_solve_* function.
// Seems clunky, but since there's a dependency between stages, I don't see a better way right now.
// Maybe we can make this explicit or something, right after the solve calls, but again, it's hard
// because some passes like Grow have also a fit pass beforehand.
fn_internal void ui_size_animate(UI_Node *node, Axis2 axis) {
  if ((axis == Axis2_X && (node->flags & UI_Flag_Animation_Grow_X)) ||
      (axis == Axis2_Y && (node->flags & UI_Flag_Animation_Grow_Y))) {
    node->solved.size.dat[axis] = f32_lerp(node->animation.spawn_t, 0.f, node->solved.size.dat[axis]);
  }

}

fn_internal void ui_solve_layout_size_known_for_axis(UI_Node *node, Axis2 axis) {
  if (node) {
    UI_Size size = node->layout.size[axis];
    switch (size.type) {
      case UI_Size_Type_Fixed: {
        node->solved.size.dat[axis] = size.value;
        ui_size_animate(node, axis);
      } break;

      case UI_Size_Type_Text: {
        if (axis == Axis2_X) {
          node->solved.size.dat[axis] = fo_text_width(node->draw.font, node->solved.label);
          node->solved.size.dat[axis] += 2.f * node->layout.gap_border[Axis2_X];
        } else {
          node->solved.size.dat[axis] = node->draw.font->metric_height;
          node->solved.size.dat[axis] += 2.f * node->layout.gap_border[Axis2_Y];
        }

        ui_size_animate(node, axis);
      } break;
    }

    for (UI_Node *it = node->tree.first_child; it; it = it->tree.next) {
      ui_solve_layout_size_known_for_axis(it, axis);
    }
  }
}

fn_internal void ui_solve_layout_size_known(UI_Node *node) {
  ui_solve_layout_size_known_for_axis(node, Axis2_X);
  ui_solve_layout_size_known_for_axis(node, Axis2_Y);
}

fn_internal void ui_solve_layout_size_fit_for_axis(UI_Node *node, Axis2 axis) {
  if (node) {
    for (UI_Node *it = node->tree.first_child; it; it = it->tree.next) {
      ui_solve_layout_size_fit_for_axis(it, axis);
    }

    UI_Size size = node->layout.size[axis];
    switch (size.type) {
      case UI_Size_Type_Fill:
      case UI_Size_Type_Fit: {
         
          F32 used_space = 0;
          if (node) {
            if (axis == node->layout.direction) {
              F32 children_sum    = 0;
              F32 children_count  = 0;

              for (UI_Node *it = node->tree.first_child; it; it = it->tree.next) {
                children_count += 1;
                children_sum   += it->solved.size.dat[axis];
              }

              used_space += 2 * node->layout.gap_border[axis];
              used_space += children_sum;
              if (children_count) {
                used_space += (children_count - 1) * node->layout.gap_child;
              }

            } else {
              // NOTE(cmat): Max of children sizes.
              F32 children_max   = 0;
              F32 children_count = 0;

              for (UI_Node *it = node->tree.first_child; it; it = it->tree.next) {
                children_count += 1;
                children_max   = f32_max(children_max, it->solved.size.dat[axis]);
              }

              used_space += 2 * node->layout.gap_border[axis];
              used_space += children_max;
            }
          }

        node->solved.size.dat[axis] = used_space;
        ui_size_animate(node, axis);

      } break;
    }
  }
}

fn_internal void ui_solve_layout_size_fit(UI_Node *node) {
  ui_solve_layout_size_fit_for_axis(node, Axis2_X);
  ui_solve_layout_size_fit_for_axis(node, Axis2_Y);
}

fn_internal void ui_solve_layout_size_fill_for_axis(UI_Node *node, Axis2 axis, F32 free_space) {
  if (node) {
    UI_Size size = node->layout.size[axis];
    switch (size.type) {
      case UI_Size_Type_Fill: {
        node->solved.size.dat[axis] += free_space;
        ui_size_animate(node, axis);
      } break;
    }

    if (axis != node->layout.direction) {
      for (UI_Node *it = node->tree.first_child; it; it = it->tree.next) {  
        free_space = node->solved.size.dat[axis] - it->solved.size.dat[axis] - 2.f * node->layout.gap_border[axis];
        ui_solve_layout_size_fill_for_axis(it, axis, free_space);
      }
    } else {
      F32 free_space          = node->solved.size.dat[axis];
      F32 children_count      = 0;
      F32 fill_children_count = 0;

      free_space -= 2.f * node->layout.gap_border[axis];
      for (UI_Node *it = node->tree.first_child; it; it = it->tree.next) {
        UI_Size it_size = it->layout.size[axis];

        children_count += 1;
        fill_children_count += it_size.type == UI_Size_Type_Fill;
        free_space     -= it->solved.size.dat[axis];
      }

      if (children_count) {
        free_space -= (children_count - 1) * node->layout.gap_child;
      }

      free_space *= f32_div_safe(1.f, fill_children_count);
      free_space = f32_max(free_space, 0);
      for (UI_Node *it = node->tree.first_child; it; it = it->tree.next) {
        ui_solve_layout_size_fill_for_axis(it, axis, free_space);
      }
    }
  }
}

fn_internal void ui_solve_layout_size_fill(UI_Node *node, V2F free_space) {
  ui_solve_layout_size_fill_for_axis(node, Axis2_X, free_space.x);
  ui_solve_layout_size_fill_for_axis(node, Axis2_Y, free_space.y);
}

fn_internal F32 ui_solve_position_relative_for_axis(UI_Node *node, Axis2 axis, Axis2 layout_direction, F32 relative_position) {
  if (node) {
    if (axis == Axis2_X) {
      if (node->flags & UI_Flag_Layout_Float_X) {
        node->solved.position_relative.x = node->layout.float_position[Axis2_X];
      } else {
        node->solved.position_relative.x = relative_position;
      }
    }

    if (axis == Axis2_Y) {
      if (node->flags & UI_Flag_Layout_Float_Y) {
        node->solved.position_relative.y = node->layout.float_position[Axis2_Y];
      } else {
        node->solved.position_relative.y = relative_position;
      }
    }

    if (layout_direction == axis) {
      relative_position += node->solved.size.dat[axis];
    }

    F32 child_position = node->layout.gap_border[axis];
    for (UI_Node *it = node->tree.first_child; it; it = it->tree.next) {
      child_position = ui_solve_position_relative_for_axis(it, axis, node->layout.direction, child_position);
      if (node->layout.direction == axis)
        child_position += node->layout.gap_child;
    }
  }

  return relative_position;
}

fn_internal void ui_solve_position_relative(UI_Node *node, Axis2 layout_direction, V2F relative_position) {
  ui_solve_position_relative_for_axis(node, Axis2_X, layout_direction, relative_position.x);
  ui_solve_position_relative_for_axis(node, Axis2_Y, layout_direction, relative_position.y);
}

fn_internal void ui_solve_region(UI_Node *node, V2F position_at) {
  if (node) {
    position_at = v2f_add(position_at, node->solved.position_relative);

    V2F display_resolution = platform_display()->resolution;
    V2F size               = node->solved.size;
    V2F position_absolute  = v2f(position_at.x, display_resolution.y - position_at.y - node->solved.size.y);
    node->solved.region_absolute = r2f_v(position_absolute, v2f_add(position_absolute, size));

    for (UI_Node *it = node->tree.first_child; it; it = it->tree.next) {
      ui_solve_region(it, position_at);
    }
  }
}

fn_internal void ui_solve(UI_Node *node) {
  ui_solve_label(node);

  ui_solve_layout_size_known(node);
  ui_solve_layout_size_fit(node);
  ui_solve_layout_size_fill(node, v2f(0, 0));

  ui_solve_position_relative(node, Axis2_X, v2f(0, 0));
  ui_solve_region(node, v2f(0, 0));
}

// ------------------------------------------------------------
// #-- NOTE(cmat): Draw UI tree

typedef struct UI_Draw_Context {
  F32 opacity;
  R2I clip_region;
} UI_Draw_Context;

fn_internal void ui_draw(UI_Node *node, UI_Draw_Context *context) {
  if (node) {

    F32 opacity = context->opacity;
    if (node->flags & UI_Flag_Animation_Fade_In) {
      opacity = f32_lerp(node->animation.spawn_t, 0.f, 1.f);
    }

    // TODO(cmat): Force clipping to be hierarchical.
    R2F region = node->solved.region_absolute;
    R2I clip_region = context->clip_region;
    if (node->flags & UI_Flag_Draw_Clip_Content) {
      clip_region = r2i(region.x0, region.y0, region.x1, region.y1);
    }

    g2_clip_region(clip_region);

    if (node->flags & UI_Flag_Draw_Background) {
      HSV hsv_color = node->palette.idle;

      hsv_color = v3f_lerp(node->animation.hover_t, hsv_color, node->palette.hover);
      hsv_color = v3f_lerp(node->animation.down_t,  hsv_color, node->palette.down);

      V4F rgb_color = { 0 };
      rgb_color.rgb = rgb_from_hsv(hsv_color);
      rgb_color.a   = opacity;

      V2F position = region.min;
      V2F size     = v2f_sub(region.max, region.min);

      g2_draw_rect(position, size, .color = rgb_color);
    }

    if (node->flags & UI_Flag_Draw_Inner_Fill) {
      RGBA rgb_color = { };
      rgb_color.rgb = rgb_from_hsv(node->palette.inner_fill);
      rgb_color.a   = opacity;

      R2F region_inner_fill = r2f(region.x0 + node->draw.inner_fill_border,
                                  region.y0 + node->draw.inner_fill_border,
                                  region.x1 - node->draw.inner_fill_border,
                                  region.y1 - node->draw.inner_fill_border);

      V2F position = region_inner_fill.min;
      V2F size     = v2f_sub(region_inner_fill.max, region_inner_fill.min);
      g2_draw_rect(position, size, .color = rgb_color);
    }

    if (node->flags & UI_Flag_Draw_Content_Hook) {
      Assert(node->draw.content_hook, "draw content hook not set");
      If_Likely (node->draw.content_hook) {
        node->draw.content_hook(&node->response, region, node->draw.content_user_data);
      }
    }

    if (node->flags & UI_Flag_Draw_Label) {
      V2F text_at = v2f_add(region.min, v2f(0, -node->draw.font->metric_descent));
      text_at = v2f_add(text_at, v2f(node->layout.gap_border[Axis2_X], node->layout.gap_border[Axis2_Y]));

      g2_draw_text(node->solved.label, node->draw.font, v2f_add(text_at, v2f(1, -1)), .color = v4f(0, 0, 0, opacity * .6f));

      text_at = v2f_add(text_at, v2f_lerp(node->animation.down_t, v2f(0, 0), v2f(1.f, -1.f)));
      g2_draw_text(node->solved.label, node->draw.font, text_at, .color = v4f(1, 1, 1, opacity));
    }

    UI_Draw_Context child_context = {
      .opacity     = opacity,
      .clip_region = clip_region,
    };

    for (UI_Node *it = node->tree.first_child; it; it = it->tree.next) {
      ui_draw(it, &child_context);
    }
  }
}

fn_internal void ui_frame_begin(void) {
  Assert(UI_State.parent_stack_len == 0, "parent set before ui_frame_begin()");

  UI_State.root = ui_node_push(UI_Root_Label, UI_Flag_None);
  ui_parent_push(UI_State.root);

  UI_State.root->layout.size[Axis2_X] = UI_Size_Fixed(platform_display()->resolution.x);
  UI_State.root->layout.size[Axis2_Y] = UI_Size_Fixed(platform_display()->resolution.y);

  // NOTE(cmat): Clear overlay list.
  UI_State.overlay_list.first = 0;
  UI_State.overlay_list.last  = 0;
}

fn_internal void ui_frame_end(void) {
  ui_parent_pop();
  Assert(UI_State.parent_stack_len == 0, "mismatched ui_parent_push(), pop missing");

  UI_Draw_Context draw_context = {
    .opacity = 1.f,
    .clip_region = G2_Clip_None,
  };

  // NOTE(cmat): Solve and draw root.
  ui_solve  (UI_State.root);
  ui_draw   (UI_State.root, &draw_context);

  // NOTE(cmat): Solve and draw overlays.
  for (UI_Node *it = UI_State.overlay_list.first; it; it = it->overlay_next) {
    ui_solve  (it);
    ui_draw   (it, &draw_context);
  }
}

// ------------------------------------------------------------
// #-- NOTE(cmat): UI component definitions
typedef U32 UI_Container_Mode;
enum {
  UI_Container_Mode_None, // NOTE(cmat): Invisible
  UI_Container_Mode_Box,
};

fn_internal UI_Node *ui_container(Str label, UI_Container_Mode mode, Axis2 layout_direction, UI_Size size_x, UI_Size size_y) {
  UI_Flags flags = 0;

  switch (mode) {
    case UI_Container_Mode_Box: {
      flags = UI_Flag_Response_Hover  |
              UI_Flag_Draw_Background |
              UI_Flag_Draw_Border;
    } break;
  }

  UI_Node *node = ui_node_push(label, flags);
  node->layout.direction           = layout_direction;
  node->layout.size[Axis2_X]       = size_x;
  node->layout.size[Axis2_Y]       = size_y;

  if (UI_Container_Mode_Box) {
    node->layout.gap_border[Axis2_X] = 20;
    node->layout.gap_border[Axis2_Y] = 20;
    node->layout.gap_child           = 10;
  }

  node->palette.idle  = hsv_u32(200, 10, 10);
  node->palette.hover = hsv_u32(210, 10, 12);
  node->palette.down  = hsv_u32(220, 10, 5);

  return node;
}

fn_internal UI_Response ui_label(Str label) {
  UI_Flags flags = UI_Flag_Draw_Label;

  UI_Node *node = ui_node_push(label, flags);
  node->layout.size[Axis2_X] = UI_Size_Text;
  node->layout.size[Axis2_Y] = UI_Size_Text;

  return node->response;
}

fn_internal UI_Response ui_button(Str label) {
  UI_Flags flags =
    UI_Flag_Response_Hover   |
    UI_Flag_Response_Down    |
    UI_Flag_Response_Press   |
    UI_Flag_Draw_Background  |
    UI_Flag_Draw_Border      |
    UI_Flag_Draw_Label;

  UI_Node *node                    = ui_node_push(label, flags);
  node->layout.size[Axis2_X]       = UI_Size_Text;
  node->layout.size[Axis2_Y]       = UI_Size_Text;
  node->layout.gap_border[Axis2_X] = fo_em(node->draw.font, .25f);
  node->layout.gap_border[Axis2_Y] = fo_em(node->draw.font, .1f);

  node->palette.idle  = hsv_u32(235, 27, 25);
  node->palette.hover = hsv_u32(235, 24, 44);
  node->palette.down  = hsv_u32(235, 10, 15);

  return node->response;
}

fn_internal UI_Response ui_checkbox(Str label, B32 *value) {
  UI_Response response = { };

  Assert(value, "value is null");
  If_Likely (value) {
    UI_Node *container = ui_container(label, UI_Container_Mode_None, Axis2_X, UI_Size_Fit, UI_Size_Fit);
    container->layout.gap_child = 5.0f;

    UI_Parent_Scope(container) {
      ui_label(label);

      UI_Flags flags =
        UI_Flag_Response_Hover   |
        UI_Flag_Response_Down    |
        UI_Flag_Response_Press   |
        UI_Flag_Draw_Background  |
        UI_Flag_Draw_Border      |
        UI_Flag_Draw_Inner_Fill;


      UI_Node *node = ui_node_push(str_lit("checkbox"), flags);

      F32 checkbox_size          = fo_em(node->draw.font, 1.f);
      node->layout.size[Axis2_X] = UI_Size_Fixed(checkbox_size);
      node->layout.size[Axis2_Y] = UI_Size_Fixed(checkbox_size);

      node->palette.idle       = hsv_u32(235, 27, 25);
      node->palette.hover      = hsv_u32(235, 24, 44);
      node->palette.down       = hsv_u32(235, 10, 15);
      node->palette.inner_fill = hsv_u32(100, 80, 80);

      if (node->response.press) {
        *value = !(*value);
      }

      F32 border_active   = .18f * checkbox_size;
      F32 border_inactive = .5f  * checkbox_size;

      node->draw.inner_fill_border = border_inactive;
      if (*value) {
        if (node->response.down) {
          node->draw.inner_fill_border = f32_lerp(node->animation.down_t, border_inactive, border_active);
        } else {
          node->draw.inner_fill_border = border_active;
        }

      } else {
        if (node->response.down) {
          node->draw.inner_fill_border = f32_lerp(node->animation.down_t, border_active, border_inactive);
        } else {
          node->draw.inner_fill_border = border_inactive;
        }
      }

      response = node->response;
    }
  }

  return response;
}

fn_internal UI_Response ui_edit_f32(Str label, F32 *value, F32 step) {
  UI_Flags flags =
    UI_Flag_Response_Hover   |
    UI_Flag_Response_Down    |
    UI_Flag_Response_Press   |
    UI_Flag_Draw_Background  |
    UI_Flag_Draw_Border      |
    UI_Flag_Draw_Label;

  UI_Node *node = ui_node_push(label, flags);
  node->layout.size[Axis2_X] = UI_Size_Text;
  node->layout.size[Axis2_Y] = UI_Size_Text;
  node->layout.gap_border[Axis2_X] = 0; // .25f * node->draw.font->metric_em;
  node->layout.gap_border[Axis2_Y] = 0; // .25f * node->draw.font->metric_em;

  node->palette.idle  = hsv_u32(235, 27, 25);
  node->palette.hover = hsv_u32(235, 24, 44);
  node->palette.down  = hsv_u32(235, 10, 15);

  return node->response;
}
