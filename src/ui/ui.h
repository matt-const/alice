// (C) Copyright 2025 Matyas Constans
// Licensed under the MIT License (https://opensource.org/license/mit/)

// NOTE(cmat): UI system-design based on these resources:
// - Casey Muratori's initial ImGUI talk:   https://youtu.be/Z1qyvQsjK5Y?si=LAjyAcb8h94JujiS
// - Ryan J. Fleury's articles:             https://www.rfleury.com/p/ui-part-2-build-it-every-frame-immediate
// - Clay UI layout:                        https://www.youtube.com/watch?v=by9lQvpvMIc

#define UI_Root_Label str_lit("##root")

typedef U32 UI_Flags;
enum {
  UI_Flag_None                  = 0,

  // NOTE(cmat): Action Response
  UI_Flag_Response_Hover        = 1 << 0,
  UI_Flag_Response_Down         = 1 << 1,

  UI_Flag_Response_Press        = 1 << 2, // NOTE(cmat): A press triggers ONCE, after a mouse click.
  UI_Flag_Response_Release      = 1 << 3, // NOTE(cmat): A release triggers ONCE, after the mouse is released.

  // NOTE(cmat): Layout
  UI_Flag_Layout_Float_X        = 1 << 4,
  UI_Flag_Layout_Float_Y        = 1 << 5,

  // NOTE(cmat): Draw flags
  UI_Flag_Draw_Background       = 1 << 6,
  UI_Flag_Draw_Shadow           = 1 << 7,
  UI_Flag_Draw_Rounded          = 1 << 8,
  UI_Flag_Draw_Label            = 1 << 9,
  UI_Flag_Draw_Clip_Content     = 1 << 10,
  UI_Flag_Draw_Content_Hook     = 1 << 11,
  UI_Flag_Draw_Border           = 1 << 12,
  UI_Flag_Draw_Inner_Fill       = 1 << 13,
};

typedef struct UI_Response {
  B32 hover;
  B32 down;
  B32 press;
  B32 release;
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
#define UI_Size_Fill            (UI_Size) { .type = UI_Size_Type_Fill                           }

typedef struct UI_Layout {
  Axis2   direction;
  UI_Size size        [Axis2_Count];
  I32     gap_border  [Axis2_Count];
  I32     gap_child;

  I32     float_position[Axis2_Count];
} UI_Layout;

typedef struct UI_Color_Palette {
  HSV inactive;
  HSV idle;
  HSV hover;
  HSV down;

  HSV inner_fill;
} UI_Color_Palette;

typedef struct UI_Draw {
  FO_Font *font;
  I32      inner_fill_border;
} UI_Draw;

typedef struct UI_Animation {
  F32 hover_t;
  F32 down_t;
} UI_Animation;

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

typedef U32 UI_ID;
typedef struct UI_Key {
  UI_ID id;
  Str   label;
} UI_Key;

typedef struct UI_Node {
  UI_Node          *hash_next;
  UI_Node          *overlay_next;

  UI_Key            key;
  UI_Flags          flags;
  UI_Node_Tree      tree;
  UI_Layout         layout;
  UI_Draw           draw;
  UI_Color_Palette  palette;
  UI_Animation      animation;
  UI_Response       response;
  UI_Solved         solved;
} UI_Node;

typedef struct UI_Node_List {
  UI_Node *first;
  UI_Node *last;
} UI_Node_List;

typedef Array_Type(UI_Node) UI_Node_Array;
