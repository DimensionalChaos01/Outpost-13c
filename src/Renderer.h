#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include <vector>
#include <set>
#include <map>
#include "TileDef.h"
#include "EntityDef.h"
#include "TextureAtlas.h"
#include "VoiceProfile.h"
#include "Hotbar.h"
#include "ItemDef.h"
#include "PlayerInventory.h"

class Map;
class FogOfWar;
class Camera;
class Player;
class MenuBar;
struct EntityInstance;
struct Tile;
struct RoomDef;

static constexpr int k_menu_bar_h   = 24;
static constexpr int k_status_bar_h = 40;

// Which text field is actively being edited in a props panel (0 = none).
static constexpr int k_tfield_none     = 0;
static constexpr int k_tfield_desc     = 1;
static constexpr int k_tfield_narrator = 2;
static constexpr int k_tfield_readable = 3;
static constexpr int k_tfield_pickup   = 4;
static constexpr int k_tfield_item_id        = 5;
static constexpr int k_tfield_container_loot = 6;
static constexpr int k_tfield_container_rolls= 7;
static constexpr int k_tfield_container_add  = 8;

struct TextFieldEdit {
    int         active = k_tfield_none;
    const char* buf    = nullptr;
};

struct EditorUIState {
    int         tile_id   = 2;
    const char* tool      = "Paint";
    int         cursor_tx = -1;
    int         cursor_ty = -1;
    bool        dirty     = false;
    bool        path_on   = false;
    bool        rooms_on      = false;  // O key: room tags overlay
    bool        room_ov_on    = false;  // R key: room flood-fill overlay
    bool        light_on      = false;
    bool        grid_on       = false;
    bool        entity_on = true;
    int         zoom_pct  = 100;
    int         map_w     = 0;
    int         map_h     = 0;
    const char* flash_msg = nullptr;
    // Entity mode
    bool        entity_mode  = false;
    const char* entity_type  = nullptr;
    char        entity_facing = 'N';
    // Texture mode
    bool        texture_mode = false;
    const char* texture_id   = nullptr;
};

class Renderer {
public:
    Renderer() = default;
    ~Renderer();

    bool init(const char* title, int width, int height);
    void shutdown();

    void clear(Uint8 r = 0, Uint8 g = 0, Uint8 b = 0);
    void present();
    void toggle_fullscreen();
    void draw_map(const Map& map, const FogOfWar& fog, const Camera& cam,
                  bool editor_mode = false);
    void draw_player(const Player& player, const Camera& cam);

    int screen_w() const;
    int screen_h() const;

    void to_logical(int wx, int wy, int& lx, int& ly) const;
    void set_title(const std::string& title);

    // ── Public drawing primitives ─────────────────────────────────────────
    void fill_rect(int x, int y, int w, int h,
                   Uint8 r, Uint8 g, Uint8 b, Uint8 a = 255);
    void draw_rect_outline(int x, int y, int w, int h,
                           Uint8 r, Uint8 g, Uint8 b);
    void draw_line(int x1, int y1, int x2, int y2,
                   Uint8 r, Uint8 g, Uint8 b);
    void draw_text(const std::string& text, int x, int y,
                   Uint8 r = 220, Uint8 g = 220, Uint8 b = 220);
    int  text_width(const std::string& text) const;

    // ── Editor tile overlays ──────────────────────────────────────────────
    void draw_grid(const Camera& cam);
    void draw_editor_cursor(int tx, int ty, const Camera& cam);
    void draw_pathability_overlay(const Map& map, const Camera& cam);
    void draw_room_tags_overlay(const Map& map, const Camera& cam);
    void draw_scar_overlay(const Map& map, const Camera& cam);
    void draw_rect_preview(int tx0, int ty0, int tx1, int ty1,
                           bool fill_mode, const Camera& cam);
    void draw_spawn_marker(int tx, int ty, const Camera& cam);

    // ── Entity layer ──────────────────────────────────────────────────────
    void draw_entity_layer(const Map& map,
                           const std::vector<EntityDef>& defs,
                           const Camera& cam);
    void draw_entity_link_lines(const Map& map, const Camera& cam,
                                bool link_selected = false,
                                uint32_t selected_src_uid = 0);
    void draw_entity_placement_cursor(const EntityDef& def, char facing,
                                      int tx, int ty, const Camera& cam);

    // ── Editor panels ─────────────────────────────────────────────────────
    void draw_tile_palette(const std::vector<TileDef>& defs, int selected_id,
                           int scroll, int y_top, int max_h, int panel_w);
    void draw_entity_panel(const std::vector<EntityDef>& defs,
                           const std::string& selected_id,
                           const std::set<std::string>& collapsed,
                           int scroll, int y_top, int max_h, int panel_w);
    void draw_right_panel(const EditorUIState& ui,
                          int x, int y, int w, int h);

    // ── Floating properties panels ────────────────────────────────────────
    static constexpr int k_props_w = 260;  // widened for text fields
    void draw_entity_props(const EntityInstance& ei, const EntityDef* def,
                           int px, int py,
                           const char* label_override = nullptr,
                           bool label_editing = false,
                           const TextFieldEdit* tfe = nullptr);
    // Returns the total pixel height drawn (for hit-testing in Editor).
    int  draw_tile_props(const Tile& tile, const TileDef* tdef,
                         int tile_x, int tile_y, int px, int py,
                         const TextFieldEdit* tfe = nullptr);

    // ── Room overlay + properties ──────────────────────────────────────────
    void draw_room_overlay(const std::vector<std::pair<int,int>>& tiles,
                           bool is_open, int seed_x, int seed_y,
                           const Camera& cam);
    void draw_room_props(const RoomDef& room,
                         const std::vector<std::string>& purposes,
                         int purpose_idx,
                         int px, int py,
                         bool name_editing,
                         const std::string& name_buf);

    // ── UI chrome ─────────────────────────────────────────────────────────
    void draw_editor_ui(const EditorUIState& ui);
    void draw_menu_bar(const MenuBar& mb);
    void draw_shortcuts_overlay();
    void draw_game_controls_overlay();
    void draw_tile_hover_hud(const std::vector<std::string>& lines);

    void set_tile_defs(const std::vector<TileDef>* defs) { tile_defs_ = defs; }
    void set_atlases(const std::vector<TextureAtlas>* atlases) { atlases_ = atlases; }
    void set_voice_profiles(const std::vector<VoiceProfile>* p) { voice_profiles_ = p; }

    // Draw one cell from a texture atlas at screen coords (x,y) at size (w,h).
    void draw_atlas_cell(const TextureAtlas& atlas, int cell_idx,
                         int x, int y, int w, int h, Uint8 alpha = 255);

    // ── HUD ───────────────────────────────────────────────────────────────────
    void draw_hotbar(const Hotbar& hb,
                     const PlayerInventory& inv,
                     const std::vector<ItemDef>& defs);

    // Draw a floating inspect tooltip near (mx, my). Only for entities.
    void draw_inspect_tooltip(const EntityInstance& ei,
                              const std::vector<EntityDef>& defs,
                              int mx, int my);

private:
    bool try_load_font(int pt_size);
    void draw_overlay_dot(int x, int y, bool active);
    void draw_checkbox(int x, int y, bool checked, bool overridden);
    static char category_letter(const std::string& cat);

    // Draw the text-properties section used by both tile and entity props panels.
    // Returns the y coordinate after the last row drawn.
    int draw_text_section(int px, int y, int has_pickup,
                          const std::string& desc, const std::string& narrator,
                          const std::string& readable, const std::string& pickup,
                          const std::string& voice_id, const std::string& dismiss,
                          const TextFieldEdit* tfe);

    SDL_Texture* load_or_get_texture(const std::string& path);

    SDL_Window*                          window_         = nullptr;
    SDL_Renderer*                        renderer_       = nullptr;
    TTF_Font*                            font_           = nullptr;
    const std::vector<TileDef>*          tile_defs_      = nullptr;
    const std::vector<TextureAtlas>*     atlases_        = nullptr;
    const std::vector<VoiceProfile>*     voice_profiles_ = nullptr;
    std::map<std::string,SDL_Texture*>   texture_cache_;
};
