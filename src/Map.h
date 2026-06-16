#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "TileDef.h"

enum class DoorState { Closed, Open, Locked };

// ── Tile ──────────────────────────────────────────────────────────────────────

struct Tile {
    int         tile_id        = 0;
    bool        walkable       = false;
    bool        pathable       = false;
    int         scar_intensity = 0;
    int         light_level    = 0;
    std::string room_tag;
    std::string texture_id;      // tile/general texture: "atlas_id:entry_name"
    std::string texture_open;    // door: texture when open
    std::string texture_closed;  // door: texture when closed
    std::string texture_locked;  // door: texture when locked
    DoorState   door_state     = DoorState::Closed;
    int         door_moves     = 0;

    // Text fields — set in editor, inspected by E key in-game.
    std::string text_desc;      // main description shown on inspect
    std::string text_narrator;  // secondary atmospheric line
    std::string text_readable;  // pushed as a separate entry after description
    std::string text_voice;     // voice profile id (empty = station_ambient)
    std::string text_dismiss;   // "input" / "auto" / "timed:N" (empty = input)

    // Per-tile property overrides. -1 = use tile type default.
    int8_t ov_walkable       = -1;
    int8_t ov_pathable       = -1;
    int8_t ov_transparent    = -1;
    int8_t ov_blocks_light   = -1;
    int8_t ov_interactable   = -1;
    int8_t ov_scar_permeable = -1;

    bool has_overrides() const {
        return ov_walkable != -1 || ov_pathable != -1  || ov_transparent != -1
            || ov_blocks_light != -1 || ov_interactable != -1 || ov_scar_permeable != -1;
    }
    void clear_overrides() {
        ov_walkable = ov_pathable = ov_transparent =
        ov_blocks_light = ov_interactable = ov_scar_permeable = -1;
    }
};

// ── Entity instance ───────────────────────────────────────────────────────────

struct EntityInstance {
    std::string type_id;          // references EntityDef.id
    int         x = 0, y = 0;
    char        facing = 'N';     // N / S / E / W
    bool        functional = true;
    std::string visible_shift = "both";
    std::string label;
    std::string linked_id;
    std::string notes;
    std::string texture_id;        // sprite texture placeholder
    uint32_t    uid = 0;           // unique instance ID within this map
    // Text fields — set in editor, inspected by E key in-game.
    std::string text_desc;
    std::string text_narrator;
    std::string text_readable;
    std::string text_pickup;       // auto-dismiss line shown on item pickup
    std::string text_voice;        // voice profile id (empty = station_ambient)
    std::string text_dismiss;      // "input" / "auto" / "timed:N" (empty = input)
    std::string item_id;           // for item_pickup entities: references ItemDef.id

    // Container fields (category == "container" entities). Serialised except container_contents.
    std::vector<std::string>              container_guaranteed; // always-present item ids
    std::string                           container_loot_table; // loot table id
    int                                   container_rng_rolls  = 0;
    bool                                  container_locked     = false;
    // Runtime-generated contents; populated after map load, never saved.
    std::vector<std::pair<std::string,int>> container_contents;
};

// ── Room definition ───────────────────────────────────────────────────────────
// Seed tile anchors the room; volume is re-derived at runtime via flood fill.

struct RoomDef {
    int         seed_x = -1, seed_y = -1;
    std::string name;
    std::string purpose;
    std::string notes;
};

// ── Map ───────────────────────────────────────────────────────────────────────

class Map {
public:
    void set_tile_defs(const std::vector<TileDef>* defs) { tile_defs_ = defs; }

    bool load(const std::string& path);
    bool save(const std::string& path) const;

    int                get_width()  const { return width_; }
    int                get_height() const { return height_; }
    const std::string& get_name()   const { return name_; }

    int  get_spawn_x() const; // reads player_spawn entity first, then stored field, then centre
    int  get_spawn_y() const;
    void set_spawn(int x, int y) { spawn_x_ = x; spawn_y_ = y; }

    // Returns a null tile for out-of-bounds coordinates.
    const Tile& get_tile(int x, int y) const;
          Tile* get_tile_mut(int x, int y);

    void set_tile_id(int x, int y, int tile_id);

    void set_door_state(int x, int y, DoorState state);
    void update_doors();
    bool blocks_vision(int x, int y) const;

    // ── Entity layer ──────────────────────────────────────────────────────
    uint32_t    add_entity(EntityInstance e);
    void        remove_entity(uint32_t uid);

    const std::vector<EntityInstance>& get_entities() const { return entities_; }

    EntityInstance*       find_entity_at(int x, int y);
    const EntityInstance* find_entity_at(int x, int y) const;
    std::vector<EntityInstance*>       find_entities_at(int x, int y);
    std::vector<const EntityInstance*> find_entities_at(int x, int y) const;
    EntityInstance*       find_entity_by_uid(uint32_t uid);
    const EntityInstance* find_entity_by_uid(uint32_t uid) const;

    bool has_entity_of_type(const std::string& type_id) const;

    // ── Room layer ────────────────────────────────────────────────────────
    void              set_room(const RoomDef& rd);   // upsert by seed position
    void              remove_room(int seed_x, int seed_y);
    RoomDef*          find_room(int seed_x, int seed_y);
    const RoomDef*    find_room(int seed_x, int seed_y) const;
    const std::vector<RoomDef>& get_rooms() const { return rooms_; }

private:
    void expand_to(int x, int y);
    bool is_walkable_id(int tile_id) const;

    static constexpr int k_door_auto_close_moves = 30;

    const std::vector<TileDef>* tile_defs_ = nullptr;

    std::vector<std::vector<Tile>> tiles_;
    int         width_   = 0;
    int         height_  = 0;
    int         floor_   = 0;
    int         spawn_x_ = 1;
    int         spawn_y_ = 1;
    std::string name_;

    std::vector<EntityInstance> entities_;
    uint32_t next_uid_ = 1;
    std::vector<RoomDef> rooms_;
};
