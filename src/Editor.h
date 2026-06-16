#pragma once
#include <string>
#include <vector>
#include <set>
#include <cstdint>
#include <SDL2/SDL.h>
#include "MenuBar.h"
#include "TexturePicker.h"
#include "Renderer.h"  // for TextFieldEdit, k_tfield_*

class Map;
class Camera;
struct TileDef;
struct EntityDef;
struct TextureAtlas;
struct VoiceProfile;

class Editor {
public:
    enum class Action      { Continue, Quit, Play, Pause };
    enum class Tool        { Paint, Erase, Eyedropper, Fill, RectRoom };
    enum class TexPickTarget { None, TilePaint, DoorOpen, DoorClosed, DoorLocked, EntitySprite };

    void   init(const std::string& map_path,
                const std::vector<TileDef>*   tile_defs,
                const std::vector<EntityDef>* entity_defs = nullptr);
    void   set_atlases(const std::vector<TextureAtlas>* atlases) { atlases_ = atlases; }
    void   set_voice_profiles(const std::vector<VoiceProfile>* p) { voice_profiles_ = p; }

    Action handle_event(const SDL_Event& e, Map& map, Camera& cam,
                        const Renderer& renderer);

    void update(Camera& cam, const Map& map);
    void render(Renderer& renderer, const Map& map, const Camera& cam) const;

    void update_viewport(Camera& cam, int window_w, int window_h) const;

    int  cursor_tx() const { return cursor_tx_; }
    int  cursor_ty() const { return cursor_ty_; }
    bool is_dirty()  const { return dirty_; }

private:
    // ── Tile ops ──────────────────────────────────────────────────────────────
    void paint(int tx, int ty, Map& map);
    void erase_tile(int tx, int ty, Map& map);

    // ── Texture ops ───────────────────────────────────────────────────────────
    void paint_texture(int tx, int ty, Map& map);
    void erase_texture(int tx, int ty, Map& map);
    void eyedrop(int tx, int ty, const Map& map);
    void flood_fill(int tx, int ty, Map& map);
    void finish_rect(int tx0, int ty0, int tx1, int ty1, bool fill_all, Map& map);

    // ── Entity ops ────────────────────────────────────────────────────────────
    void place_entity(int tx, int ty, Map& map);
    void erase_entity_at(int tx, int ty, Map& map);

    // ── Undo / redo ───────────────────────────────────────────────────────────
    struct TilePaint   { int tx, ty, old_id, new_id; };
    struct TexPaint    { int tx, ty; std::string old_id, new_id; };
    struct EntityOp    { bool placed; uint32_t uid; std::string type_id;
                         int x, y; char facing; };
    struct UndoEntry   {
        enum Kind { TileBatch, EntityPlace, EntityErase, TextureBatch } kind;
        std::vector<TilePaint> tiles;
        std::vector<TexPaint>  textures;
        EntityOp               entity_op;
    };
    void push_undo_(UndoEntry e);
    void undo_(Map& map);
    void redo_(Map& map);

    // ── Misc helpers ──────────────────────────────────────────────────────────
    bool do_save(const Map& map);
    void update_cursor(int lx, int ly, const Camera& cam);
    const char* tool_name() const;
    int  tile_id_at_palette_click(int lx, int ly, const Camera& cam) const;
    const EntityDef* entity_at_panel_click(int lx, int ly, const Camera& cam) const;
    void process_menu_action(const std::string& action, Map& map,
                             Camera& cam, const Renderer& renderer);
    void flash(const char* msg, int ms = 2000);
    void close_props();

    // ── Tile tool state ───────────────────────────────────────────────────────
    Tool        tool_     = Tool::Paint;
    int         paint_id_ = 2;
    bool        dirty_    = false;
    std::string map_path_;

    const std::vector<TileDef>*   tile_defs_   = nullptr;
    const std::vector<EntityDef>* entity_defs_ = nullptr;

    int  cursor_tx_  = -1;
    int  cursor_ty_  = -1;
    bool left_held_  = false;

    bool middle_held_  = false;
    int  mid_prev_lx_  = 0;
    int  mid_prev_ly_  = 0;

    bool rect_active_   = false;
    int  rect_start_tx_ = -1;
    int  rect_start_ty_ = -1;

    // Undo batching for tile paint strokes
    bool                   painting_batch_  = false;
    std::vector<TilePaint> batch_tiles_;

    // Undo batching for texture paint strokes
    bool                   texture_batching_ = false;
    std::vector<TexPaint>  tex_batch_;

    // ── Right-click state ─────────────────────────────────────────────────────
    bool right_held_    = false;
    bool right_dragging_= false; // true once drag threshold exceeded
    int  right_press_lx_= 0;
    int  right_press_ly_= 0;

    // ── Entity mode ───────────────────────────────────────────────────────────
    bool        entity_panel_open_   = false;
    bool        entity_layer_visible_= true;
    std::string selected_entity_id_;        // empty = no selection
    char        entity_facing_       = 'N';
    int         entity_scroll_       = 0;
    std::set<std::string> collapsed_cats_;

    // ── Properties panels ─────────────────────────────────────────────────────
    mutable bool entity_props_open_ = false;
    uint32_t entity_props_uid_  = 0;
    int      props_sx_          = 0; // screen-space anchor
    int      props_sy_          = 0;

    bool tile_props_open_ = false;
    int  tile_props_tx_   = -1;
    int  tile_props_ty_   = -1;

    // ── Undo / redo ───────────────────────────────────────────────────────────
    std::vector<UndoEntry> undo_stack_;
    int                    undo_pos_   = 0; // next write index
    static constexpr int   k_max_undo  = 100;

    // ── Panels / overlays ─────────────────────────────────────────────────────
    bool palette_open_     = false;
    bool right_panel_open_ = false;
    int  palette_scroll_   = 0;

    bool show_pathability_ = false;
    bool show_room_tags_   = false;
    bool show_light_       = false;
    bool show_grid_        = false;
    bool show_shortcuts_   = false;

    // ── Texture mode ─────────────────────────────────────────────────────────
    bool        texture_mode_     = false;
    std::string texture_paint_id_;
    const std::vector<TextureAtlas>* atlases_ = nullptr;
    TexturePicker texture_picker_;

    // Label editing (entity props panel)
    bool        label_editing_  = false;
    std::string label_edit_buf_;
    uint32_t    label_edit_uid_ = 0;

    // Text field editing (tile and entity props panels)
    // active_text_field_ matches k_tfield_* constants; 0 = none
    int         active_text_field_ = 0;
    bool        text_field_is_tile_= false;  // true = tile, false = entity
    std::string text_field_buf_;
    const std::vector<VoiceProfile>* voice_profiles_ = nullptr;

    std::string flash_msg_;
    Uint32      flash_until_ = 0;

    // ── Room overlay ──────────────────────────────────────────────────────────
    bool show_room_overlay_  = false;
    int  room_hover_last_tx_ = -1;
    int  room_hover_last_ty_ = -1;
    std::vector<std::pair<int,int>> room_hover_tiles_;
    bool room_hover_is_open_ = false;
    int  room_hover_seed_x_  = -1;
    int  room_hover_seed_y_  = -1;

    // ── Room props panel ──────────────────────────────────────────────────────
    bool room_props_open_    = false;
    int  room_props_sx_      = 0;
    int  room_props_sy_      = 0;
    int  room_props_seed_x_  = -1;
    int  room_props_seed_y_  = -1;
    bool room_name_editing_  = false;
    std::string room_name_buf_;
    int  room_purpose_idx_   = 0;
    std::vector<std::string> room_purposes_;      // display names
    std::vector<std::string> room_purpose_tags_;  // stored identifiers

    // ── Entity link selection ─────────────────────────────────────────────────
    bool     link_selected_ = false;
    uint32_t link_src_uid_  = 0;

    // ── Texture pick routing ──────────────────────────────────────────────────
    TexPickTarget tex_pick_target_ = TexPickTarget::None;
    uint32_t      tex_pick_uid_    = 0;

    MenuBar menu_bar_;

    static constexpr int k_pan_px        = 4;
    static constexpr int k_palette_w     = 160;
    static constexpr int k_right_panel_w = 160;
    static constexpr int k_palette_row_h = 42;
    static constexpr int k_entity_row_h  = 28;
    static constexpr int k_entity_hdr_h  = 20;
    static constexpr int k_right_drag_px = 4; // pixels before RMB becomes drag
};
