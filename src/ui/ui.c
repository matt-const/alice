// (C) Copyright 2025 Matyas Constans
// Licensed under the MIT License (https://opensource.org/license/mit/)

var_global struct {
  Arena         arena;
  U64           hash_count;
  UI_Node_List *hash_array;
  UI_Node      *active_parent;
  FO_Font       font;
} UI_State = { };

fn_internal void ui_init(void) {
  zero_fill(&UI_State);

  arena_init(&UI_State.arena);
  UI_State.hash_count = 1024;
  UI_State.hash_array = arena_push_count(&UI_State.arena, UI_Node_List, UI_State.hash_count);

  fo_font_init(&UI_State.font,
               &UI_State.arena,
               str(Ubuntu_Regular_ttf_len, Ubuntu_Regular_ttf),
               26, v2_u16(1024, 1024), Codepoints_ASCII);
}

fn_internal UI_Node *ui_cache(Str node_key) {
  U64 bucket_index   = str_hash(node_key) % UI_State.hash_count;
  UI_Node_List *list = UI_State.hash_array + bucket_index;

  UI_Node *result = 0;
  if (!list->first) {
    list->first = arena_push_type(&UI_State.arena, UI_Node);
    list->last  = list->first;

    result      = list->last;
    result->key = node_key;

  } else {
    UI_Node *entry = list->first;
    while (entry) {
      if (str_equals(node_key, entry->key)) {
        result = entry;
        break;
      }

      if (!entry->hash_next) {   
        list->last->hash_next  = arena_push_type(&UI_State.arena, UI_Node);
        list->last             = list->last->hash_next;
        result                 = list->last;
        result->key            = node_key;
      }

      entry = entry->hash_next;
    }
  }

  return result;
}

fn_internal void ui_parent_push(UI_Node *parent) {
  UI_State.active_parent = parent;
}

fn_internal void ui_parent_pop(void) {
  Assert(UI_State.active_parent, "can't pop further");
  UI_State.active_parent = UI_State.active_parent->tree.parent;
}

fn_internal void ui_node_update_tree(UI_Node *node) {
  UI_Node_Tree *tree = &node->tree;
  UI_Node *parent    = UI_State.active_parent;

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
  }
}

fn_internal Str ui_label_from_label_key(UI_Key key) {
  Str result = key.label_key; 
}

fn_internal UI_Node *ui_node_push(Str key, UI_Flags flags) {
  UI_Node *node = ui_cache(key);

  node->solved.label = node->key.label_key;
  node->flags        = flags;

  ui_node_update_response (node);
  ui_node_update_tree     (node);

  return node;
}

// ------------------------------------------------------------
// #-- NOTE(cmat): Layout solver

fn_internal void ui_solve_layout_size_known_for_axis(UI_Node *node, Axis2 axis) {
  if (node) {
    UI_Size size = node->layout.size[axis];
    switch (size.type) {
      case UI_Size_Type_Fixed: {
        node->solved.size.dat[axis] = size.value;
      } break;

      case UI_Size_Type_Text: {
        if (axis == Axis2_X) {
          node->solved.size.dat[axis] = fo_text_width(&UI_State.font, node->solved.label);
        } else {
          node->solved.size.dat[axis] = UI_State.font.metric_height;
        }
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
              I32 children_count  = 0;

              for (UI_Node *it = node->tree.first_child; it; it = it->tree.next) {
                children_count += 1;
                children_sum   += it->solved.size.dat[axis];
              }

              used_space  = 2 * node->layout.gap_border;
              used_space += children_sum;
              if (children_count) {
                used_space += (children_count - 1) * node->layout.gap_child;
              }

            } else {
              // NOTE(cmat): Max of children sizes.
              F32 children_max   = 0;
              I32 children_count = 0;

              for (UI_Node *it = node->tree.first_child; it; it = it->tree.next) {
                children_count += 1;
                children_max   = f32_max(children_max, it->solved.size.dat[axis]);
              }

              used_space  = 2 * node->layout.gap_border;
              used_space += children_max;
            }
          }

        node->solved.size.dat[axis] = used_space;
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
      } break;
    }

    if (axis != node->layout.direction) {
      for (UI_Node *it = node->tree.first_child; it; it = it->tree.next) {  
        free_space = node->solved.size.dat[axis] - 2.f * it->solved.size.dat[axis];
        ui_solve_layout_size_fill_for_axis(it, axis, free_space);
      }
    } else {
      F32 free_space          = node->solved.size.dat[axis];
      I32 children_count      = 0;
      I32 fill_children_count = 0;

      free_space -= 2.f * node->layout.gap_border;
      for (UI_Node *it = node->tree.first_child; it; it = it->tree.next) {
        UI_Size it_size = it->layout.size[axis];

        children_count += 1;
        fill_children_count += it_size.type == UI_Size_Type_Fill;
        free_space     -= it->solved.size.dat[axis];
      }

      if (children_count) {
        free_space -= (node->layout.gap_child - 1) * children_count;
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

fn_internal F32 ui_solve_position_relative_for_axis(UI_Node *node, Axis2 axis, Axis2 layout_direction, I32 relative_position) {
  if (node) {
    if (axis == Axis2_X) {
      if (node->flags & UI_Flag_Layout_Float_X) {
        node->solved.position_relative.x = node->layout.float_position_x;
      } else {
        node->solved.position_relative.x = relative_position;
      }
    }

    if (axis == Axis2_Y) {
      if (node->flags & UI_Flag_Layout_Float_Y) {
        node->solved.position_relative.y = node->layout.float_position_y;
      } else {
        node->solved.position_relative.y = relative_position;
      }
    }

    if (layout_direction == axis) {
      relative_position += node->solved.size.dat[axis];
    }

    I32 child_position = node->layout.gap_border;
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

    V2F display_resolution  = platform_display()->resolution;
    V2F size                = node->solved.size;
    V2F position_absolute   = v2f(position_at.x, display_resolution.y - position_at.y - node->solved.size.y);
    node->solved.region_absolute = r2f_v(position_absolute,              v2f_add(position_absolute, size));

    for (UI_Node *it = node->tree.first_child; it; it = it->tree.next) {
      ui_solve_region(it, position_at);
    }
  }
}

fn_internal void ui_solve(UI_Node *node) {
  ui_solve_layout_size_known(node);
  ui_solve_layout_size_fit(node);
  ui_solve_layout_size_fill(node, v2f(0, 0));

  ui_solve_position_relative(node, Axis2_X, v2f(0, 0));
  ui_solve_region(node, v2f(0, 0));
}

// ------------------------------------------------------------
// #-- NOTE(cmat): Draw UI tree

fn_internal void ui_draw(UI_Node *node) {
  if (node) {
    V4F color = { };
    color.rgb = rgb_from_hsv(node->draw.hsv_background);
    color.a   = 1.f;

    R2F region = node->solved.region_absolute;
    V2F position = region.min;
    V2F size     = v2f_sub(region.max, region.min);

    g2_draw_rect(position, size, .color = color);
    if (node->flags & UI_Flag_Draw_Label) {
      g2_draw_text(node->solved.label, &UI_State.font, v2f_add(position, v2f(0, -UI_State.font.metric_descent)), .color = v4f(1, 1, 1, 1));
    }

    for (UI_Node *it = node->tree.first_child; it; it = it->tree.next) {
      ui_draw(it);
    }
  }
}

var_local_persist Random_Seed Random = 1234;

fn_internal void ui_frame_flush(UI_Node *root_node) {
  Random = 1234;

  ui_solve(root_node);
  ui_draw(root_node);
  Assert(UI_State.active_parent == 0, "mismatched ui_parent_push(), pop missing");
}

// ------------------------------------------------------------
// #-- NOTE(cmat): UI component definitions

fn_internal UI_Response ui_button(Str key) {
  UI_Flags flags =
    UI_Flag_Response_Hover        |
    UI_Flag_Response_Press        |
    UI_Flag_Response_Click        |
    UI_Flag_Draw_Background       |
    UI_Flag_Draw_Border           |
    UI_Flag_Draw_Label;

  UI_Node *node = ui_node_push(key, flags);
  node->layout.size[Axis2_X] = UI_Size_Text;
  node->layout.size[Axis2_Y] = UI_Size_Text;

  node->draw.hsv_background = v3f(f32_random_unilateral(&Random), .8f, .6f);

  return node->response;
}

