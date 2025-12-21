// (C) Copyright 2025 Matyas Constans
// Licensed under the MIT License (https://opensource.org/license/mit/)

// NOTE(cmat): UI system-design based on these resources:
// - Casey Muratori's initial ImGUI talk:   https://youtu.be/Z1qyvQsjK5Y?si=LAjyAcb8h94JujiS
// - Ryan J. Fleury's articles:             https://www.rfleury.com/p/ui-part-2-build-it-every-frame-immediate
// - Clay UI layout:                        https://www.youtube.com/watch?v=by9lQvpvMIc

typedef U32 UI_Flags;
enum {
  UI_Flag_None                  = 0,

  // NOTE(cmat): Action Response
  UI_Flag_Response_Hover        = 1 << 0,
  UI_Flag_Response_Press        = 1 << 1,
  UI_Flag_Response_Click        = 1 << 2,

  // NOTE(cmat): Layout
  UI_Flag_Layout_Float_X        = 1 << 3,
  UI_Flag_Layout_Float_Y        = 1 << 4,

  // NOTE(cmat): Draw flags
  UI_Flag_Draw_Background       = 1 << 5,
  UI_Flag_Draw_Shadow           = 1 << 6,
  UI_Flag_Draw_Rounded          = 1 << 7,
  UI_Flag_Draw_Border           = 1 << 8,
  UI_Flag_Draw_Label            = 1 << 9,
  UI_Flag_Draw_Clip_Content     = 1 << 10,
  UI_Flag_Draw_Content_Hook     = 1 << 11,
};

typedef struct UI_Response {
  B32 hover;
  B32 press;
  B32 click;
} UI_Response;

typedef U32 UI_Size_Type;
enum {
  UI_Size_Type_Fit = 0, // NOTE(cmat): Default.
  UI_Size_Type_Fill,
  UI_Size_Type_Fixed,
  UI_Size_Type_Text,
};

typedef struct UI_Size {
  UI_Size_Type  type;
  F32           value;
} UI_Size;

#define UI_Size_Fixed(pixels_)  (UI_Size) { .type = UI_Size_Type_Fixed, .value = (F32)(pixels_) }
#define UI_Size_Text            (UI_Size) { .type = UI_Size_Type_Text                           }
#define UI_Size_Fit             (UI_Size) { .type = UI_Size_Type_Fit                            }

typedef struct UI_Layout {
  Axis2   direction;
  UI_Size size[Axis2_Count];
  I32     gap_border;
  I32     gap_child;

  I32     float_position_x;
  I32     float_position_y;
} UI_Layout;

typedef struct UI_Draw {
  V3F hsv_background;
  V3F hsv_idle;
  V3F hsv_hover;
  V4F hsv_press;
} UI_Draw;

// NOTE(cmat): Solved location, size.
typedef struct UI_Solved {
  Str label;
  V2F size;
  V2F position_relative;
  R2F region_absolute;
} UI_Solved;

struct UI_Node;
typedef struct UI_Node UI_Node;
typedef struct UI_Node_Tree {
  UI_Node *parent;
  UI_Node *first;
  UI_Node *last;
  UI_Node *next;
  UI_Node *first_child;
} UI_Node_Tree;

typedef struct UI_Node {
  Str            key;
  UI_Flags       flags;
  UI_Node       *hash_next;
  UI_Node_Tree   tree;
  UI_Layout      layout;
  UI_Draw        draw;
  UI_Response    response;
  UI_Solved      solved;
} UI_Node;

typedef struct UI_Node_List {
  UI_Node *first;
  UI_Node *last;
} UI_Node_List;

#if 0

typedef U64 UI_ID;
fn_internal UI_ID ui_id_from_str(Str string);

typedef U64 UI_Size_Type;
enum {
  UI_Size_Type_Pixels,
  UI_Size_Type_Fit_Text,
  UI_Size_Type_Parent_Ratio,
  UI_Size_Type_Children_Sum
};

typedef struct UI_Size {
  UI_Size_Type type;
  F32          value;
} UI_Size;

typedef void UI_Node_Draw_Content_Hook(R2I render_region, void *user_data);

typedef U64 UI_Node_Flag;
enum {
  UI_Node_Flag_Draw_Background    = 1 << 0,
  UI_Node_Flag_Draw_Shadow        = 1 << 1,
  UI_Node_Flag_Draw_Rounded       = 1 << 2,
  UI_Node_Flag_Draw_Border        = 1 << 3,
  UI_Node_Flag_Draw_Label         = 1 << 4,
  UI_Node_Flag_Draw_Clip_Content  = 1 << 5,

  UI_Node_Flag_Draw_Content_Hook  = 1 << 6,

  UI_Node_Flag_Layout_Float_X     = 1 << 7,
  UI_Node_Flag_Layout_Float_Y     = 1 << 8,

  UI_Node_Flag_Action_Clickable   = 1 << 9,

  UI_Node_Flag_Anim_Hot           = 1 << 10,
  UI_Node_Flag_Anim_Fade_In       = 1 << 11,
  UI_Node_Flag_Anim_Size_X        = 1 << 12,
  UI_Node_Flag_Anim_Size_Y        = 1 << 13,
};

typedef struct UI_Action {
  B32 clicked;
} UI_Action;

typedef struct UI_Node {
  UI_ID           id;
  Str             key;
  UI_Node_Flag    flags;
  UI_Action       action;

  Str             label;

  V3F             color_hsv_background;
  V3F             color_hsv_border;

  UI_Node_Draw_Content_Hook *draw_content_hook;
  void                      *draw_content_user_data;

  struct UI_Node *parent;
  struct UI_Node *first;
  struct UI_Node *next;
  struct UI_Node *last;
  struct UI_Node *first_child;

  UI_Size         input_size[2];
  U64             input_layout_axis;

  F32             input_float_relative_x;
  F32             input_float_relative_y;

  F32             solved_size[2];
  V2F             solved_relative_position;
  R2F             solved_absolute_region;

  F32             hot_t;
  F32             active_t;
  F32             opacity_t;
  F32             size_t[2];

  U64             frame_index;
  B32             first_frame;
} UI_Node;



#endif
