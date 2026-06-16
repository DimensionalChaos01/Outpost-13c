#include "Map.h"
#include "FileUtils.h"
#include "json.hpp"
#include <fstream>
#include <iostream>
#include <algorithm>

using json = nlohmann::json;

static const char* k_level_template = R"({
  "name": "New Floor",
  "floor": 0,
  "width": 20,
  "height": 15,
  "spawn_x": 10,
  "spawn_y": 7,
  "tiles": [
    [1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1],
    [1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1],
    [1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1],
    [1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1],
    [1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1],
    [1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1],
    [1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1],
    [1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1],
    [1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1],
    [1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1],
    [1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1],
    [1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1],
    [1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1],
    [1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1],
    [1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1]
  ],
  "entities": []
}
)";

static const Tile k_null_tile{};

// ── Private helpers ───────────────────────────────────────────────────────────

bool Map::is_walkable_id(int id) const {
    if (tile_defs_) return get_tile_def(*tile_defs_, id).walkable;
    return id == 2;
}

void Map::expand_to(int x, int y) {
    if (x < 0 || y < 0) return;
    if (x < width_ && y < height_) return;

    const int new_w = std::max(width_,  x + 1);
    const int new_h = std::max(height_, y + 1);

    for (int row = 0; row < height_; ++row)
        if ((int)tiles_[row].size() < new_w)
            tiles_[row].resize(new_w);

    if (new_h > height_) {
        tiles_.resize(new_h);
        for (int row = height_; row < new_h; ++row)
            tiles_[row].resize(new_w);
    }
    width_  = new_w;
    height_ = new_h;
}

// ── Spawn accessors ───────────────────────────────────────────────────────────

int Map::get_spawn_x() const {
    for (const auto& e : entities_)
        if (e.type_id == "player_spawn") return e.x;
    if (spawn_x_ > 0) return spawn_x_;
    return std::max(1, width_ / 2);
}

int Map::get_spawn_y() const {
    for (const auto& e : entities_)
        if (e.type_id == "player_spawn") return e.y;
    if (spawn_y_ > 0) return spawn_y_;
    return std::max(1, height_ / 2);
}

// ── Tile accessors ────────────────────────────────────────────────────────────

const Tile& Map::get_tile(int x, int y) const {
    if (x < 0 || x >= width_ || y < 0 || y >= height_) return k_null_tile;
    return tiles_[y][x];
}

Tile* Map::get_tile_mut(int x, int y) {
    if (x < 0 || x >= width_ || y < 0 || y >= height_) return nullptr;
    return &tiles_[y][x];
}

// ── Load ──────────────────────────────────────────────────────────────────────

bool Map::load(const std::string& path) {
    if (!ensure_file(path, k_level_template)) return false;

    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Map: cannot open " << path << "\n";
        return false;
    }

    json data;
    try { file >> data; }
    catch (const json::exception& e) {
        std::cerr << "Map: JSON parse error: " << e.what() << "\n";
        return false;
    }

    name_   = data.value("name",   "");
    floor_  = data.value("floor",  0);
    width_  = data.value("width",  0);
    height_ = data.value("height", 0);

    if (width_ <= 0 || height_ <= 0) {
        std::cerr << "Map: invalid dimensions " << width_ << "x" << height_ << "\n";
        return false;
    }

    spawn_x_ = data.value("spawn_x", width_  / 2);
    spawn_y_ = data.value("spawn_y", height_ / 2);

    // ── Tiles ──
    const auto& tile_array = data.at("tiles");
    tiles_.resize(height_);
    for (int y = 0; y < height_; ++y) {
        tiles_[y].resize(width_);
        const auto& row = tile_array.at(y);
        for (int x = 0; x < width_; ++x) {
            Tile& t      = tiles_[y][x];
            t.tile_id    = row.at(x).get<int>();
            t.walkable   = is_walkable_id(t.tile_id);
            t.pathable   = t.walkable;
            t.scar_intensity = 0;
            t.light_level    = (t.tile_id == 0) ? 0 : 3;
            t.door_state     = DoorState::Closed;
            t.door_moves     = 0;
            t.clear_overrides();
        }
    }

    // ── Tile overrides (sparse) ──
    if (data.contains("tile_overrides")) {
        for (const auto& ov : data["tile_overrides"]) {
            const int ox = ov.value("x", -1);
            const int oy = ov.value("y", -1);
            if (ox < 0 || oy < 0 || ox >= width_ || oy >= height_) continue;
            Tile& t = tiles_[oy][ox];
            if (ov.contains("walkable"))       t.ov_walkable       = ov["walkable"].get<bool>() ? 1 : 0;
            if (ov.contains("pathable"))       t.ov_pathable       = ov["pathable"].get<bool>() ? 1 : 0;
            if (ov.contains("transparent"))    t.ov_transparent    = ov["transparent"].get<bool>() ? 1 : 0;
            if (ov.contains("blocks_light"))   t.ov_blocks_light   = ov["blocks_light"].get<bool>() ? 1 : 0;
            if (ov.contains("interactable"))   t.ov_interactable   = ov["interactable"].get<bool>() ? 1 : 0;
            if (ov.contains("scar_permeable")) t.ov_scar_permeable = ov["scar_permeable"].get<bool>() ? 1 : 0;
        }
        // Apply walkable/pathable overrides to actual tile fields.
        for (int y = 0; y < height_; ++y)
            for (int x = 0; x < width_; ++x) {
                Tile& t = tiles_[y][x];
                if (t.ov_walkable >= 0) t.walkable = (t.ov_walkable == 1);
                if (t.ov_pathable >= 0) t.pathable = (t.ov_pathable == 1);
            }
    }

    // ── Texture IDs (sparse) ──
    if (data.contains("texture_ids")) {
        for (const auto& ti : data["texture_ids"]) {
            const int tx = ti.value("x", -1), ty = ti.value("y", -1);
            if (tx < 0 || ty < 0 || tx >= width_ || ty >= height_) continue;
            tiles_[ty][tx].texture_id = ti.value("id", "");
        }
    }

    // ── Door textures (sparse) ──
    if (data.contains("door_textures")) {
        for (const auto& dt : data["door_textures"]) {
            const int tx = dt.value("x", -1), ty = dt.value("y", -1);
            if (tx < 0 || ty < 0 || tx >= width_ || ty >= height_) continue;
            Tile& t = tiles_[ty][tx];
            t.texture_open   = dt.value("open",   "");
            t.texture_closed = dt.value("closed", "");
            t.texture_locked = dt.value("locked", "");
        }
    }

    // ── Tile texts (sparse) ──
    if (data.contains("tile_texts")) {
        for (const auto& tt : data["tile_texts"]) {
            const int tx = tt.value("x", -1), ty = tt.value("y", -1);
            if (tx < 0 || ty < 0 || tx >= width_ || ty >= height_) continue;
            Tile& t = tiles_[ty][tx];
            t.text_desc     = tt.value("desc",     "");
            t.text_narrator = tt.value("narrator",  "");
            t.text_readable = tt.value("readable",  "");
            t.text_voice    = tt.value("voice",     "");
            t.text_dismiss  = tt.value("dismiss",   "");
        }
    }

    // ── Entities ──
    entities_.clear();
    next_uid_ = 1;
    if (data.contains("entities")) {
        for (const auto& ei : data["entities"]) {
            EntityInstance e;
            e.type_id       = ei.value("type_id",       "");
            e.x             = ei.value("x",             0);
            e.y             = ei.value("y",             0);
            const std::string f = ei.value("facing", "N");
            e.facing        = f.empty() ? 'N' : (char)f[0];
            e.functional    = ei.value("functional",    true);
            e.visible_shift = ei.value("visible_shift", "both");
            e.label         = ei.value("label",         "");
            e.linked_id     = ei.value("linked_id",     "");
            e.notes         = ei.value("notes",         "");
            e.texture_id    = ei.value("texture_id",    "");
            e.uid           = ei.value("uid",           next_uid_);
            if (e.uid >= next_uid_) next_uid_ = e.uid + 1;
            e.text_desc     = ei.value("text_desc",     "");
            e.text_narrator = ei.value("text_narrator", "");
            e.text_readable = ei.value("text_readable", "");
            e.text_pickup   = ei.value("text_pickup",   "");
            e.text_voice    = ei.value("text_voice",    "");
            e.text_dismiss  = ei.value("text_dismiss",  "");
            e.item_id       = ei.value("item_id",       "");
            e.container_loot_table = ei.value("container_loot_table", "");
            e.container_rng_rolls  = ei.value("container_rng_rolls",  0);
            e.container_locked     = ei.value("container_locked",     false);
            if (ei.contains("container_guaranteed") && ei["container_guaranteed"].is_array())
                for (const auto& g : ei["container_guaranteed"])
                    if (g.is_string()) e.container_guaranteed.push_back(g.get<std::string>());
            if (!e.type_id.empty()) entities_.push_back(e);
        }
    }

    // ── Rooms ──
    rooms_.clear();
    if (data.contains("rooms")) {
        for (const auto& rd : data["rooms"]) {
            RoomDef r;
            r.seed_x  = rd.value("seed_x",  -1);
            r.seed_y  = rd.value("seed_y",  -1);
            r.name    = rd.value("name",    "");
            r.purpose = rd.value("purpose", "");
            r.notes   = rd.value("notes",   "");
            if (r.seed_x >= 0 && r.seed_y >= 0) rooms_.push_back(r);
        }
    }

    std::cout << "Map: loaded \"" << name_ << "\" " << width_ << "x" << height_
              << " (" << entities_.size() << " entities)\n";
    return true;
}

// ── Save ──────────────────────────────────────────────────────────────────────

bool Map::save(const std::string& path) const {
    json data;
    data["name"]    = name_;
    data["floor"]   = floor_;
    data["width"]   = width_;
    data["height"]  = height_;
    data["spawn_x"] = spawn_x_;
    data["spawn_y"] = spawn_y_;

    // Tiles (2D int array)
    json tile_array = json::array();
    for (int y = 0; y < height_; ++y) {
        json row = json::array();
        for (int x = 0; x < width_; ++x)
            row.push_back(tiles_[y][x].tile_id);
        tile_array.push_back(row);
    }
    data["tiles"] = tile_array;

    // Tile overrides (sparse)
    json ov_array = json::array();
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            const Tile& t = tiles_[y][x];
            if (!t.has_overrides()) continue;
            json ov;
            ov["x"] = x; ov["y"] = y;
            if (t.ov_walkable       >= 0) ov["walkable"]       = (t.ov_walkable       == 1);
            if (t.ov_pathable       >= 0) ov["pathable"]       = (t.ov_pathable       == 1);
            if (t.ov_transparent    >= 0) ov["transparent"]    = (t.ov_transparent    == 1);
            if (t.ov_blocks_light   >= 0) ov["blocks_light"]   = (t.ov_blocks_light   == 1);
            if (t.ov_interactable   >= 0) ov["interactable"]   = (t.ov_interactable   == 1);
            if (t.ov_scar_permeable >= 0) ov["scar_permeable"] = (t.ov_scar_permeable == 1);
            ov_array.push_back(ov);
        }
    }
    data["tile_overrides"] = ov_array;

    // Texture IDs (sparse)
    json tex_array = json::array();
    for (int y = 0; y < height_; ++y)
        for (int x = 0; x < width_; ++x) {
            if (tiles_[y][x].texture_id.empty()) continue;
            json ti; ti["x"] = x; ti["y"] = y; ti["id"] = tiles_[y][x].texture_id;
            tex_array.push_back(ti);
        }
    data["texture_ids"] = tex_array;

    // Door textures (sparse)
    json dtex_array = json::array();
    for (int y = 0; y < height_; ++y)
        for (int x = 0; x < width_; ++x) {
            const Tile& t = tiles_[y][x];
            if (t.tile_id != 3) continue;
            if (t.texture_open.empty() && t.texture_closed.empty() && t.texture_locked.empty()) continue;
            json dt; dt["x"] = x; dt["y"] = y;
            if (!t.texture_open.empty())   dt["open"]   = t.texture_open;
            if (!t.texture_closed.empty()) dt["closed"] = t.texture_closed;
            if (!t.texture_locked.empty()) dt["locked"] = t.texture_locked;
            dtex_array.push_back(dt);
        }
    data["door_textures"] = dtex_array;

    // Tile texts (sparse — only tiles with at least one non-empty text field)
    json tile_texts_array = json::array();
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            const Tile& t = tiles_[y][x];
            if (t.text_desc.empty() && t.text_narrator.empty() &&
                t.text_readable.empty() && t.text_voice.empty() && t.text_dismiss.empty())
                continue;
            json tt; tt["x"] = x; tt["y"] = y;
            if (!t.text_desc.empty())     tt["desc"]     = t.text_desc;
            if (!t.text_narrator.empty()) tt["narrator"]  = t.text_narrator;
            if (!t.text_readable.empty()) tt["readable"]  = t.text_readable;
            if (!t.text_voice.empty())    tt["voice"]     = t.text_voice;
            if (!t.text_dismiss.empty())  tt["dismiss"]   = t.text_dismiss;
            tile_texts_array.push_back(tt);
        }
    }
    data["tile_texts"] = tile_texts_array;

    // Entities
    json ent_array = json::array();
    for (const auto& e : entities_) {
        json ei;
        ei["type_id"]       = e.type_id;
        ei["x"]             = e.x;
        ei["y"]             = e.y;
        ei["facing"]        = std::string(1, e.facing);
        ei["functional"]    = e.functional;
        ei["visible_shift"] = e.visible_shift;
        ei["label"]         = e.label;
        ei["linked_id"]     = e.linked_id;
        ei["notes"]         = e.notes;
        if (!e.texture_id.empty())    ei["texture_id"]    = e.texture_id;
        if (!e.text_desc.empty())     ei["text_desc"]     = e.text_desc;
        if (!e.text_narrator.empty()) ei["text_narrator"] = e.text_narrator;
        if (!e.text_readable.empty()) ei["text_readable"] = e.text_readable;
        if (!e.text_pickup.empty())   ei["text_pickup"]   = e.text_pickup;
        if (!e.text_voice.empty())    ei["text_voice"]    = e.text_voice;
        if (!e.text_dismiss.empty())  ei["text_dismiss"]  = e.text_dismiss;
        if (!e.item_id.empty())                  ei["item_id"]                = e.item_id;
        if (!e.container_loot_table.empty())     ei["container_loot_table"]   = e.container_loot_table;
        if (e.container_rng_rolls != 0)          ei["container_rng_rolls"]    = e.container_rng_rolls;
        if (e.container_locked)                  ei["container_locked"]       = e.container_locked;
        if (!e.container_guaranteed.empty()) {
            json garr = json::array();
            for (const auto& g : e.container_guaranteed) garr.push_back(g);
            ei["container_guaranteed"] = garr;
        }
        ei["uid"]           = e.uid;
        ent_array.push_back(ei);
    }
    data["entities"] = ent_array;

    // Rooms
    json rooms_array = json::array();
    for (const auto& rd : rooms_) {
        json r;
        r["seed_x"]  = rd.seed_x;
        r["seed_y"]  = rd.seed_y;
        r["name"]    = rd.name;
        r["purpose"] = rd.purpose;
        r["notes"]   = rd.notes;
        rooms_array.push_back(r);
    }
    data["rooms"] = rooms_array;

    std::ofstream file(path);
    if (!file.is_open()) {
        std::cerr << "Map: cannot write to " << path << "\n";
        return false;
    }
    file << data.dump(2) << "\n";
    std::cout << "Map: saved \"" << name_ << "\" to " << path << "\n";
    return true;
}

// ── Door state ────────────────────────────────────────────────────────────────

void Map::set_tile_id(int x, int y, int tile_id) {
    if (x < 0 || y < 0) return;
    expand_to(x, y);
    Tile& t      = tiles_[y][x];
    t.tile_id    = tile_id;
    t.walkable   = is_walkable_id(tile_id);
    t.pathable   = t.walkable;
    t.door_state = DoorState::Closed;
    t.door_moves = 0;
    t.light_level = (tile_id == 0) ? 0 : 3;
    // Preserve overrides — the author set them intentionally.
}

void Map::set_door_state(int x, int y, DoorState state) {
    if (x < 0 || x >= width_ || y < 0 || y >= height_) return;
    Tile& t = tiles_[y][x];
    if (t.tile_id != 3) return;
    t.door_state = state;
    t.door_moves = 0;
    t.walkable   = (state == DoorState::Open);
    t.pathable   = t.walkable;
}

void Map::update_doors() {
    for (int y = 0; y < height_; ++y)
        for (int x = 0; x < width_; ++x) {
            Tile& t = tiles_[y][x];
            if (t.tile_id == 3 && t.door_state == DoorState::Open) {
                ++t.door_moves;
                if (t.door_moves >= k_door_auto_close_moves)
                    set_door_state(x, y, DoorState::Closed);
            }
        }
}

bool Map::blocks_vision(int x, int y) const {
    const Tile& t = get_tile(x, y);
    // Override: explicit transparent flag wins over tile-type default.
    if (t.ov_transparent == 1) return false;
    if (t.ov_transparent == 0) return true;
    if (t.tile_id == 1) return true;
    if (t.tile_id == 3 && t.door_state != DoorState::Open) return true;
    return false;
}

// ── Entity layer ──────────────────────────────────────────────────────────────

uint32_t Map::add_entity(EntityInstance e) {
    if (e.uid == 0) e.uid = next_uid_++;
    else if (e.uid >= next_uid_) next_uid_ = e.uid + 1;
    entities_.push_back(e);
    return e.uid;
}

void Map::remove_entity(uint32_t uid) {
    entities_.erase(
        std::remove_if(entities_.begin(), entities_.end(),
                       [uid](const EntityInstance& e){ return e.uid == uid; }),
        entities_.end());
}

EntityInstance* Map::find_entity_at(int x, int y) {
    for (auto& e : entities_)
        if (e.x == x && e.y == y) return &e;
    return nullptr;
}

const EntityInstance* Map::find_entity_at(int x, int y) const {
    for (const auto& e : entities_)
        if (e.x == x && e.y == y) return &e;
    return nullptr;
}

std::vector<EntityInstance*> Map::find_entities_at(int x, int y) {
    std::vector<EntityInstance*> result;
    for (auto& e : entities_)
        if (e.x == x && e.y == y) result.push_back(&e);
    return result;
}

std::vector<const EntityInstance*> Map::find_entities_at(int x, int y) const {
    std::vector<const EntityInstance*> result;
    for (const auto& e : entities_)
        if (e.x == x && e.y == y) result.push_back(&e);
    return result;
}

EntityInstance* Map::find_entity_by_uid(uint32_t uid) {
    for (auto& e : entities_)
        if (e.uid == uid) return &e;
    return nullptr;
}

const EntityInstance* Map::find_entity_by_uid(uint32_t uid) const {
    for (const auto& e : entities_)
        if (e.uid == uid) return &e;
    return nullptr;
}

bool Map::has_entity_of_type(const std::string& type_id) const {
    for (const auto& e : entities_)
        if (e.type_id == type_id) return true;
    return false;
}

// ── Room layer ────────────────────────────────────────────────────────────────

RoomDef* Map::find_room(int sx, int sy) {
    for (auto& r : rooms_)
        if (r.seed_x == sx && r.seed_y == sy) return &r;
    return nullptr;
}
const RoomDef* Map::find_room(int sx, int sy) const {
    for (const auto& r : rooms_)
        if (r.seed_x == sx && r.seed_y == sy) return &r;
    return nullptr;
}

void Map::set_room(const RoomDef& rd) {
    for (auto& r : rooms_) {
        if (r.seed_x == rd.seed_x && r.seed_y == rd.seed_y) { r = rd; return; }
    }
    rooms_.push_back(rd);
}

void Map::remove_room(int sx, int sy) {
    rooms_.erase(std::remove_if(rooms_.begin(), rooms_.end(),
        [sx, sy](const RoomDef& r){ return r.seed_x==sx && r.seed_y==sy; }),
        rooms_.end());
}
