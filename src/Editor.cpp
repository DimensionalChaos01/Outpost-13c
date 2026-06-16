#include "Editor.h"
#include "Map.h"
#include "Camera.h"
#include "Renderer.h"
#include "TileDef.h"
#include "EntityDef.h"
#include "json.hpp"
#include <iostream>
#include <fstream>
#include <vector>
#include <utility>
#include <algorithm>
#include <cstdio>
#include <cstring>

// ── Init ──────────────────────────────────────────────────────────────────────

void Editor::init(const std::string& map_path,
                  const std::vector<TileDef>*   tile_defs,
                  const std::vector<EntityDef>* entity_defs) {
    map_path_   = map_path;
    tile_defs_  = tile_defs;
    entity_defs_= entity_defs;

    // Load room purposes list
    {
        using json = nlohmann::json;
        std::ifstream f("Data/rooms/room_purposes.json");
        if (f.is_open()) {
            try {
                json j; f >> j;
                if (j.contains("purposes")) {
                    for (const auto& p : j["purposes"]) {
                        if (p.is_string()) {
                            room_purposes_.push_back(p.get<std::string>());
                            room_purpose_tags_.push_back(p.get<std::string>());
                        } else if (p.is_object()) {
                            room_purposes_.push_back(p.value("display_name", p.value("tag", "")));
                            room_purpose_tags_.push_back(p.value("tag", ""));
                        }
                    }
                }
            } catch (...) {}
        }
        if (room_purposes_.empty()) {
            room_purposes_.push_back("Untagged");
            room_purpose_tags_.push_back("untagged");
        }
    }
}

// ── Viewport ──────────────────────────────────────────────────────────────────

void Editor::update_viewport(Camera& cam, int window_w, int window_h) const {
    const int left  = (palette_open_ || entity_panel_open_) ? k_palette_w : 0;
    const int right = right_panel_open_ ? k_right_panel_w : 0;
    cam.set_map_origin(left, k_menu_bar_h);
    cam.set_screen(window_w - left - right,
                   window_h - k_menu_bar_h - k_status_bar_h);
}

// ── Flash helper ──────────────────────────────────────────────────────────────

void Editor::flash(const char* msg, int ms) {
    flash_msg_   = msg;
    flash_until_ = SDL_GetTicks() + ms;
}

void Editor::close_props() {
    entity_props_open_ = false;
    tile_props_open_   = false;
    room_props_open_   = false;
    if (label_editing_) { label_editing_ = false; SDL_StopTextInput(); }
    if (room_name_editing_) { room_name_editing_ = false; SDL_StopTextInput(); }
    if (active_text_field_) { active_text_field_ = k_tfield_none; SDL_StopTextInput(); }
}

// ── Tile operations ───────────────────────────────────────────────────────────

void Editor::paint(int tx, int ty, Map& map) {
    if (tx < 0 || ty < 0) return;
    const int old_id = map.get_tile(tx, ty).tile_id;
    if (old_id == paint_id_) return;
    if (painting_batch_)
        batch_tiles_.push_back({tx, ty, old_id, paint_id_});
    map.set_tile_id(tx, ty, paint_id_);
    dirty_ = true;
}

void Editor::paint_texture(int tx, int ty, Map& map) {
    if (tx < 0 || ty < 0 || texture_paint_id_.empty()) return;
    Tile* t = map.get_tile_mut(tx, ty);
    if (!t || t->texture_id == texture_paint_id_) return;
    if (texture_batching_) tex_batch_.push_back({tx, ty, t->texture_id, texture_paint_id_});
    t->texture_id = texture_paint_id_;
    dirty_ = true;
}

void Editor::erase_texture(int tx, int ty, Map& map) {
    if (tx < 0 || ty < 0) return;
    Tile* t = map.get_tile_mut(tx, ty);
    if (!t || t->texture_id.empty()) return;
    if (texture_batching_) tex_batch_.push_back({tx, ty, t->texture_id, ""});
    t->texture_id.clear();
    dirty_ = true;
}

void Editor::erase_tile(int tx, int ty, Map& map) {
    if (tx < 0 || ty < 0 || tx >= map.get_width() || ty >= map.get_height()) return;
    const int old_id = map.get_tile(tx, ty).tile_id;
    if (old_id == 2) return;
    if (painting_batch_)
        batch_tiles_.push_back({tx, ty, old_id, 2});
    map.set_tile_id(tx, ty, 2);
    dirty_ = true;
}

void Editor::eyedrop(int tx, int ty, const Map& map) {
    const Tile& t = map.get_tile(tx, ty);
    if (t.tile_id >= 0) {
        paint_id_ = t.tile_id;
        tool_     = Tool::Paint;
        entity_panel_open_ = false;
    }
}

void Editor::flood_fill(int tx, int ty, Map& map) {
    const int start_id = map.get_tile(tx, ty).tile_id;
    if (start_id == paint_id_) return;

    std::vector<TilePaint> batch;
    std::vector<std::pair<int,int>> stack;
    stack.reserve(256);
    stack.push_back({tx, ty});

    while (!stack.empty()) {
        auto [cx, cy] = stack.back(); stack.pop_back();
        if (cx < 0 || cy < 0 || cx >= map.get_width() || cy >= map.get_height()) continue;
        if (map.get_tile(cx, cy).tile_id != start_id) continue;
        batch.push_back({cx, cy, start_id, paint_id_});
        map.set_tile_id(cx, cy, paint_id_);
        dirty_ = true;
        stack.push_back({cx-1,cy}); stack.push_back({cx+1,cy});
        stack.push_back({cx,cy-1}); stack.push_back({cx,cy+1});
    }

    if (!batch.empty()) {
        UndoEntry u; u.kind = UndoEntry::TileBatch; u.tiles = std::move(batch);
        push_undo_(std::move(u));
    }
}

void Editor::finish_rect(int tx0, int ty0, int tx1, int ty1, bool fill_all, Map& map) {
    if (tx0 > tx1) std::swap(tx0, tx1);
    if (ty0 > ty1) std::swap(ty0, ty1);
    if (tx0 < 0) tx0 = 0;
    if (ty0 < 0) ty0 = 0;

    std::vector<TilePaint> batch;
    for (int ty = ty0; ty <= ty1; ++ty) {
        for (int tx = tx0; tx <= tx1; ++tx) {
            const int old_id = map.get_tile(tx, ty).tile_id;
            const bool border = (tx==tx0 || tx==tx1 || ty==ty0 || ty==ty1);
            const int  new_id = fill_all ? paint_id_ : (border ? 1 : 2);
            if (old_id != new_id) batch.push_back({tx, ty, old_id, new_id});
            map.set_tile_id(tx, ty, new_id);
        }
    }
    dirty_ = true;
    if (!batch.empty()) {
        UndoEntry u; u.kind = UndoEntry::TileBatch; u.tiles = std::move(batch);
        push_undo_(std::move(u));
    }
}

// ── Entity operations ─────────────────────────────────────────────────────────

void Editor::place_entity(int tx, int ty, Map& map) {
    if (tx < 0 || ty < 0 || selected_entity_id_.empty()) return;

    // Remove any existing entity at this tile first.
    const EntityInstance* existing = map.find_entity_at(tx, ty);
    if (existing) {
        EntityOp op{false, existing->uid, existing->type_id,
                    existing->x, existing->y, existing->facing};
        map.remove_entity(existing->uid);
        UndoEntry u; u.kind = UndoEntry::EntityErase; u.entity_op = op;
        push_undo_(std::move(u));
    }

    EntityInstance e;
    e.type_id = selected_entity_id_;
    e.x       = tx;
    e.y       = ty;
    e.facing  = entity_facing_;

    const EntityDef* def = entity_defs_ ?
        find_entity_def(*entity_defs_, selected_entity_id_) : nullptr;
    e.functional    = def ? def->functional : true;
    e.visible_shift = def ? def->visible_shift : "both";

    uint32_t uid = map.add_entity(e);
    dirty_ = true;

    EntityOp op{true, uid, e.type_id, tx, ty, entity_facing_};
    UndoEntry u; u.kind = UndoEntry::EntityPlace; u.entity_op = op;
    push_undo_(std::move(u));
}

void Editor::erase_entity_at(int tx, int ty, Map& map) {
    EntityInstance* e = map.find_entity_at(tx, ty);
    if (!e) return;
    EntityOp op{false, e->uid, e->type_id, e->x, e->y, e->facing};
    map.remove_entity(e->uid);
    dirty_ = true;
    UndoEntry u; u.kind = UndoEntry::EntityErase; u.entity_op = op;
    push_undo_(std::move(u));
}

// ── Undo / redo ───────────────────────────────────────────────────────────────

void Editor::push_undo_(UndoEntry e) {
    // Discard any redo history above current position.
    if (undo_pos_ < (int)undo_stack_.size())
        undo_stack_.erase(undo_stack_.begin() + undo_pos_, undo_stack_.end());
    // Cap history.
    if (undo_pos_ >= k_max_undo) {
        undo_stack_.erase(undo_stack_.begin());
        --undo_pos_;
    }
    undo_stack_.push_back(std::move(e));
    ++undo_pos_;
}

void Editor::undo_(Map& map) {
    if (undo_pos_ <= 0) { flash("Nothing to undo", 1500); return; }
    --undo_pos_;
    const UndoEntry& u = undo_stack_[undo_pos_];
    if (u.kind == UndoEntry::TileBatch) {
        for (auto it = u.tiles.rbegin(); it != u.tiles.rend(); ++it)
            map.set_tile_id(it->tx, it->ty, it->old_id);
        dirty_ = true;
    } else if (u.kind == UndoEntry::EntityPlace) {
        map.remove_entity(u.entity_op.uid);
        dirty_ = true;
    } else if (u.kind == UndoEntry::EntityErase) {
        EntityInstance e;
        e.uid     = u.entity_op.uid;
        e.type_id = u.entity_op.type_id;
        e.x       = u.entity_op.x;
        e.y       = u.entity_op.y;
        e.facing  = u.entity_op.facing;
        const EntityDef* def = entity_defs_ ?
            find_entity_def(*entity_defs_, e.type_id) : nullptr;
        e.functional    = def ? def->functional : true;
        e.visible_shift = def ? def->visible_shift : "both";
        map.add_entity(e);
        dirty_ = true;
    } else if (u.kind == UndoEntry::TextureBatch) {
        for (auto it = u.textures.rbegin(); it != u.textures.rend(); ++it) {
            Tile* t = map.get_tile_mut(it->tx, it->ty);
            if (t) t->texture_id = it->old_id;
        }
        dirty_ = true;
    }
    flash("Undo", 800);
}

void Editor::redo_(Map& map) {
    if (undo_pos_ >= (int)undo_stack_.size()) { flash("Nothing to redo", 1500); return; }
    const UndoEntry& u = undo_stack_[undo_pos_];
    ++undo_pos_;
    if (u.kind == UndoEntry::TileBatch) {
        for (const auto& tp : u.tiles)
            map.set_tile_id(tp.tx, tp.ty, tp.new_id);
        dirty_ = true;
    } else if (u.kind == UndoEntry::EntityPlace) {
        EntityInstance e;
        e.uid     = u.entity_op.uid;
        e.type_id = u.entity_op.type_id;
        e.x       = u.entity_op.x;
        e.y       = u.entity_op.y;
        e.facing  = u.entity_op.facing;
        const EntityDef* def = entity_defs_ ?
            find_entity_def(*entity_defs_, e.type_id) : nullptr;
        e.functional    = def ? def->functional : true;
        e.visible_shift = def ? def->visible_shift : "both";
        map.add_entity(e);
        dirty_ = true;
    } else if (u.kind == UndoEntry::EntityErase) {
        map.remove_entity(u.entity_op.uid);
        dirty_ = true;
    } else if (u.kind == UndoEntry::TextureBatch) {
        for (const auto& tp : u.textures) {
            Tile* t = map.get_tile_mut(tp.tx, tp.ty);
            if (t) t->texture_id = tp.new_id;
        }
        dirty_ = true;
    }
    flash("Redo", 800);
}

// ── Save ──────────────────────────────────────────────────────────────────────

bool Editor::do_save(const Map& map) {
    if (map.save(map_path_)) {
        dirty_ = false;
        flash("Saved!");
        return true;
    }
    flash("Save failed!", 3000);
    return false;
}

// ── Tool name ─────────────────────────────────────────────────────────────────

const char* Editor::tool_name() const {
    switch (tool_) {
        case Tool::Paint:      return "Paint";
        case Tool::Erase:      return "Erase";
        case Tool::Eyedropper: return "Eyedrop";
        case Tool::Fill:       return "Fill";
        case Tool::RectRoom:   return "RectRoom";
    }
    return "?";
}

// ── Cursor helpers ────────────────────────────────────────────────────────────

void Editor::update_cursor(int lx, int ly, const Camera& cam) {
    cursor_tx_ = cam.to_tile_x(lx);
    cursor_ty_ = cam.to_tile_y(ly);
}

int Editor::tile_id_at_palette_click(int lx, int ly, const Camera& cam) const {
    const int y_top = cam.map_top();
    const int max_h = cam.screen_h();
    if (!palette_open_ || lx >= k_palette_w || ly < y_top || ly >= y_top + max_h) return -1;
    const int row = (ly - y_top + palette_scroll_ * k_palette_row_h) / k_palette_row_h;
    if (!tile_defs_ || row < 0 || row >= (int)tile_defs_->size()) return -1;
    return (*tile_defs_)[row].id;
}

// Returns pointer to entity def clicked in entity panel, or nullptr.
const EntityDef* Editor::entity_at_panel_click(int lx, int ly, const Camera& cam) const {
    if (!entity_panel_open_ || !entity_defs_ || lx >= k_palette_w) return nullptr;
    const int y_top = cam.map_top();
    const int max_h = cam.screen_h();
    if (ly < y_top || ly >= y_top + max_h) return nullptr;

    static const char* k_cat_order[] = {
        "spawn","furniture","workstation","terminal",
        "wall_mounted","container","door","trigger", nullptr
    };

    int y = y_top - entity_scroll_;
    for (int ci = 0; k_cat_order[ci]; ++ci) {
        const std::string cat = k_cat_order[ci];
        std::vector<const EntityDef*> cat_defs;
        for (const auto& d : *entity_defs_)
            if (d.category == cat) cat_defs.push_back(&d);
        if (cat_defs.empty()) continue;

        // Header row
        if (ly >= y && ly < y + k_entity_hdr_h) return nullptr; // clicked header
        y += k_entity_hdr_h;

        if (collapsed_cats_.count(cat)) continue;

        for (const auto* d : cat_defs) {
            if (ly >= y && ly < y + k_entity_row_h) return d;
            y += k_entity_row_h;
        }
    }
    return nullptr;
}

// ── Menu action processing ────────────────────────────────────────────────────

void Editor::process_menu_action(const std::string& action, Map& map,
                                  Camera& cam, const Renderer& renderer) {
    if (action == "file.save") {
        do_save(map);
    } else if (action == "file.new" || action == "file.open" ||
               action == "file.save_as" || action == "file.resize") {
        flash("Not yet implemented");
    } else if (action == "map.play") {
        do_save(map);
    } else if (action == "view.path") {
        show_pathability_ = !show_pathability_;
    } else if (action == "view.rooms") {
        show_room_tags_ = !show_room_tags_;
    } else if (action == "view.light") {
        show_light_ = !show_light_;
    } else if (action == "view.grid") {
        show_grid_ = !show_grid_;
    } else if (action == "view.zoom_in" || action == "view.zoom_out" ||
               action == "view.zoom_reset") {
        if (action == "view.zoom_in")    cam.zoom_step(1);
        if (action == "view.zoom_out")   cam.zoom_step(-1);
        if (action == "view.zoom_reset") cam.zoom_to(1.0f, renderer.screen_w()/2, renderer.screen_h()/2);
    } else if (action == "help.shortcuts") {
        show_shortcuts_ = true;
    } else {
        flash("Not yet implemented");
    }
}

// ── Event handling ────────────────────────────────────────────────────────────

Editor::Action Editor::handle_event(const SDL_Event& e, Map& map,
                                     Camera& cam, const Renderer& renderer) {
    if (e.type == SDL_QUIT) return Action::Quit;

    // ── Shortcuts overlay ─────────────────────────────────────────────────────
    if (show_shortcuts_) {
        if (e.type == SDL_KEYDOWN || e.type == SDL_MOUSEBUTTONDOWN)
            show_shortcuts_ = false;
        return Action::Continue;
    }

    // ── Texture picker overlay ─────────────────────────────────────────────────
    if (texture_picker_.is_open()) {
        auto tr = texture_picker_.handle_event(e, renderer);
        if (tr == TexturePicker::Result::Selected) {
            const std::string sel = texture_picker_.selected_id();
            switch (tex_pick_target_) {
                case TexPickTarget::DoorOpen: {
                    Tile* t = map.get_tile_mut(tile_props_tx_, tile_props_ty_);
                    if (t) { t->texture_open = sel; dirty_ = true; } break;
                }
                case TexPickTarget::DoorClosed: {
                    Tile* t = map.get_tile_mut(tile_props_tx_, tile_props_ty_);
                    if (t) { t->texture_closed = sel; dirty_ = true; } break;
                }
                case TexPickTarget::DoorLocked: {
                    Tile* t = map.get_tile_mut(tile_props_tx_, tile_props_ty_);
                    if (t) { t->texture_locked = sel; dirty_ = true; } break;
                }
                case TexPickTarget::EntitySprite: {
                    EntityInstance* ei = map.find_entity_by_uid(tex_pick_uid_);
                    if (ei) { ei->texture_id = sel; dirty_ = true; } break;
                }
                default:
                    texture_paint_id_ = sel;
                    if (!texture_paint_id_.empty()) texture_mode_ = true;
                    break;
            }
            tex_pick_target_ = TexPickTarget::None;
        }
        if (tr != TexturePicker::Result::None) texture_picker_.close();
        return Action::Continue;
    }

    // ── Menu bar gets first look ───────────────────────────────────────────────
    {
        MenuEvent me = menu_bar_.handle_event(e, renderer.screen_w());
        if (!me.action.empty()) {
            if (me.action == "map.play") { do_save(map); return Action::Play; }
            process_menu_action(me.action, map, cam, renderer);
            return Action::Continue;
        }
        if (me.consumed) return Action::Continue;
    }

    // ── Label text editing (SDL_TEXTINPUT + key handling) ─────────────────────
    if (label_editing_) {
        if (e.type == SDL_TEXTINPUT) {
            if (label_edit_buf_.size() < 40)
                label_edit_buf_ += e.text.text;
            return Action::Continue;
        }
        if (e.type == SDL_KEYDOWN) {
            switch (e.key.keysym.sym) {
                case SDLK_BACKSPACE:
                    if (!label_edit_buf_.empty()) label_edit_buf_.pop_back();
                    return Action::Continue;
                case SDLK_RETURN: {
                    EntityInstance* ei = map.find_entity_by_uid(label_edit_uid_);
                    if (ei) { ei->label = label_edit_buf_; dirty_ = true; }
                    label_editing_ = false;
                    SDL_StopTextInput();
                    return Action::Continue;
                }
                case SDLK_ESCAPE:
                    label_editing_ = false;
                    SDL_StopTextInput();
                    return Action::Continue;
            }
        }
        // Swallow all other events while editing
        return Action::Continue;
    }

    // ── Text field editing (tile / entity text properties) ───────────────────
    if (active_text_field_ != k_tfield_none) {
        if (e.type == SDL_TEXTINPUT) {
            if (text_field_buf_.size() < 240)
                text_field_buf_ += e.text.text;
            return Action::Continue;
        }
        if (e.type == SDL_KEYDOWN) {
            switch (e.key.keysym.sym) {
                case SDLK_BACKSPACE:
                    if (!text_field_buf_.empty()) text_field_buf_.pop_back();
                    return Action::Continue;
                case SDLK_RETURN:
                case SDLK_ESCAPE: {
                    // Save the edited value back to the correct field
                    auto save_text_field = [&]() {
                        const int fid = active_text_field_;
                        if (text_field_is_tile_) {
                            Tile* t = map.get_tile_mut(tile_props_tx_, tile_props_ty_);
                            if (!t) return;
                            if (fid==k_tfield_desc)     t->text_desc     = text_field_buf_;
                            else if (fid==k_tfield_narrator) t->text_narrator = text_field_buf_;
                            else if (fid==k_tfield_readable) t->text_readable = text_field_buf_;
                        } else {
                            EntityInstance* ei = map.find_entity_by_uid(entity_props_uid_);
                            if (!ei) return;
                            if (fid==k_tfield_desc)          ei->text_desc     = text_field_buf_;
                            else if (fid==k_tfield_narrator) ei->text_narrator = text_field_buf_;
                            else if (fid==k_tfield_readable) ei->text_readable = text_field_buf_;
                            else if (fid==k_tfield_pickup)   ei->text_pickup   = text_field_buf_;
                            else if (fid==k_tfield_item_id)  ei->item_id       = text_field_buf_;
                            else if (fid==k_tfield_container_loot)  ei->container_loot_table = text_field_buf_;
                            else if (fid==k_tfield_container_rolls) {
                                try { ei->container_rng_rolls = std::stoi(text_field_buf_); } catch (...) {}
                            } else if (fid==k_tfield_container_add) {
                                if (!text_field_buf_.empty())
                                    ei->container_guaranteed.push_back(text_field_buf_);
                            }
                        }
                        dirty_ = true;
                    };
                    if (e.key.keysym.sym == SDLK_RETURN) save_text_field();
                    active_text_field_ = k_tfield_none;
                    SDL_StopTextInput();
                    return Action::Continue;
                }
            }
        }
        return Action::Continue;
    }

    // ── Room name text editing ────────────────────────────────────────────────
    if (room_name_editing_) {
        if (e.type == SDL_TEXTINPUT) {
            if (room_name_buf_.size() < 40) room_name_buf_ += e.text.text;
            return Action::Continue;
        }
        if (e.type == SDL_KEYDOWN) {
            switch (e.key.keysym.sym) {
                case SDLK_BACKSPACE:
                    if (!room_name_buf_.empty()) room_name_buf_.pop_back();
                    return Action::Continue;
                case SDLK_RETURN:
                case SDLK_ESCAPE:
                    room_name_editing_ = false; SDL_StopTextInput();
                    return Action::Continue;
            }
        }
        return Action::Continue;
    }

    // ── Props-panel Escape / click-outside close ───────────────────────────────
    if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE && !e.key.repeat) {
        if (entity_props_open_ || tile_props_open_ || room_props_open_) { close_props(); return Action::Continue; }
        if (menu_bar_.is_open()) { menu_bar_.close(); return Action::Continue; }
        // No active operation: open pause menu
        if (!entity_panel_open_ && !rect_active_ && !left_held_)
            return Action::Pause;
        entity_panel_open_ = false;
        rect_active_ = false;
        return Action::Continue;
    }

    // ── Mouse wheel ───────────────────────────────────────────────────────────
    if (e.type == SDL_MOUSEWHEEL) {
        int lx = 0, ly = 0; SDL_GetMouseState(&lx, &ly);
        if (SDL_GetModState() & KMOD_CTRL) {
            cam.zoom_step(e.wheel.y > 0 ? 1 : -1);
        } else if (lx < k_palette_w) {
            if (entity_panel_open_) {
                entity_scroll_ -= e.wheel.y * k_entity_row_h;
                entity_scroll_  = std::max(0, entity_scroll_);
            } else if (palette_open_) {
                palette_scroll_ -= e.wheel.y;
                if (tile_defs_) {
                    const int max_s = std::max(0,
                        (int)tile_defs_->size() - cam.screen_h() / k_palette_row_h);
                    palette_scroll_ = std::max(0, std::min(palette_scroll_, max_s));
                }
            }
        } else {
            cam.pan(0, -e.wheel.y * 32);
        }
        return Action::Continue;
    }

    // ── Mouse motion ──────────────────────────────────────────────────────────
    if (e.type == SDL_MOUSEMOTION) {
        const int lx = e.motion.x, ly = e.motion.y;
        update_cursor(lx, ly, cam);

        if (middle_held_) {
            cam.pan(mid_prev_lx_ - lx, mid_prev_ly_ - ly);
            mid_prev_lx_ = lx; mid_prev_ly_ = ly;
        }
        if (left_held_) {
            if (texture_mode_) {
                paint_texture(cursor_tx_, cursor_ty_, map);
            } else if (!entity_panel_open_) {
                if (tool_ == Tool::Paint) paint(cursor_tx_, cursor_ty_, map);
                if (tool_ == Tool::Erase) erase_tile(cursor_tx_, cursor_ty_, map);
            }
        }
        if (right_held_) {
            const int dx = std::abs(lx - right_press_lx_);
            const int dy = std::abs(ly - right_press_ly_);
            if (!right_dragging_ && (dx > k_right_drag_px || dy > k_right_drag_px))
                right_dragging_ = true;
            if (right_dragging_) {
                if (texture_mode_) {
                    erase_texture(cursor_tx_, cursor_ty_, map);
                } else {
                    erase_tile(cursor_tx_, cursor_ty_, map);
                    erase_entity_at(cursor_tx_, cursor_ty_, map);
                }
            }
        }
        return Action::Continue;
    }

    // ── Mouse button down ─────────────────────────────────────────────────────
    if (e.type == SDL_MOUSEBUTTONDOWN) {
        const int lx = e.button.x, ly = e.button.y;
        update_cursor(lx, ly, cam);

        // Dismiss props if clicking outside them.
        if (entity_props_open_ || tile_props_open_) {
            // Let clicks through — actual dismiss handled on Esc or new RMB.
        }

        if (e.button.button == SDL_BUTTON_MIDDLE) {
            middle_held_ = true; mid_prev_lx_ = lx; mid_prev_ly_ = ly;
        }

        if (e.button.button == SDL_BUTTON_LEFT) {
            // ── Props panels get first click priority (BEFORE close_props) ───
            if (tile_props_open_) {
                Tile* t = map.get_tile_mut(tile_props_tx_, tile_props_ty_);
                if (t) {
                    constexpr int PW  = Renderer::k_props_w;
                    constexpr int PAD = 8, ROW = 22;
                    const bool is_door = (t->tile_id == 3);
                    const int door_tex_h = is_door ? (10 + 8 + 16 + 3*28) : 0;
                    constexpr int k_ts_tile = 10+16+20*5+4; // text section: sep+header+5 rows+trail
                    const int ph = 24+18+4+ROW*6+16+28 + door_tex_h + k_ts_tile;
                    int ppx = std::min(props_sx_, renderer.screen_w() - PW - 4);
                    int ppy = std::min(props_sy_, renderer.screen_h() - ph - 4);
                    ppx = std::max(ppx, 0); ppy = std::max(ppy, k_menu_bar_h);
                    if (lx >= ppx && lx < ppx+PW && ly >= ppy && ly < ppy+ph) {
                        // Door texture Pick buttons
                        if (is_door) {
                            const int door_base_y = ppy + 24+18+4+ROW*6+16+28 + 10+8+16;
                            const int pick_x = ppx + PW - PAD - 38;
                            for (int slot = 0; slot < 3; ++slot) {
                                const int pick_y = door_base_y + slot * 28;
                                if (lx >= pick_x && lx < pick_x+38 &&
                                    ly >= pick_y  && ly < pick_y+20) {
                                    switch (slot) {
                                        case 0: tex_pick_target_ = TexPickTarget::DoorOpen;   break;
                                        case 1: tex_pick_target_ = TexPickTarget::DoorClosed; break;
                                        case 2: tex_pick_target_ = TexPickTarget::DoorLocked; break;
                                    }
                                    texture_picker_.open(atlases_, t->tile_id);
                                    return Action::Continue;
                                }
                            }
                        }
                        // Reset button
                        const int reset_y = ppy + 24+18+4+ROW*6+8;
                        if (lx >= ppx+PAD && lx < ppx+PW-PAD &&
                            ly >= reset_y  && ly < reset_y+22) {
                            t->clear_overrides();
                            dirty_ = true;
                            return Action::Continue;
                        }
                        const TileDef* tdef = tile_defs_ ?
                            &get_tile_def(*tile_defs_, t->tile_id) : nullptr;
                        struct OvInfo { int8_t* ov; bool def_val; };
                        OvInfo ov_rows[] = {
                            {&t->ov_walkable,       tdef?tdef->walkable:false},
                            {&t->ov_pathable,       tdef?tdef->walkable:false},
                            {&t->ov_transparent,    (t->tile_id==2||t->tile_id==3)},
                            {&t->ov_blocks_light,   (t->tile_id==1||t->tile_id==6||t->tile_id==7)},
                            {&t->ov_interactable,   false},
                            {&t->ov_scar_permeable, false},
                        };
                        int cy = ppy + 26 + 18 + 4 + 6;
                        for (auto& row : ov_rows) {
                            const bool overridden = (*row.ov >= 0);
                            const bool cur_val    = overridden ? (*row.ov == 1) : row.def_val;
                            if (lx >= ppx+PAD && lx < ppx+PAD+22 &&
                                ly >= cy       && ly < cy+ROW) {
                                *row.ov = cur_val ? 0 : 1;
                                dirty_  = true;
                                return Action::Continue;
                            }
                            cy += ROW;
                        }
                        // ── Text section click handling (tile props) ─────────────────
                        {
                            const int ts_y0 = ppy + 24+18+4+ROW*6+16+28 + door_tex_h + 10+16;
                            // Text field rows (Desc, Narr, Read — no Pickup for tiles)
                            for (int fid = k_tfield_desc; fid <= k_tfield_readable; ++fid) {
                                const int fy = ts_y0 + (fid-1)*20;
                                if (lx >= ppx+72 && lx < ppx+PW-8 && ly >= fy-2 && ly < fy+18) {
                                    if (active_text_field_ == fid && text_field_is_tile_) {
                                        active_text_field_ = k_tfield_none; SDL_StopTextInput();
                                    } else {
                                        label_editing_ = false;
                                        if (active_text_field_) SDL_StopTextInput();
                                        active_text_field_  = fid;
                                        text_field_is_tile_ = true;
                                        if (fid==k_tfield_desc)     text_field_buf_ = t->text_desc;
                                        else if (fid==k_tfield_narrator) text_field_buf_ = t->text_narrator;
                                        else                             text_field_buf_ = t->text_readable;
                                        SDL_StartTextInput();
                                    }
                                    return Action::Continue;
                                }
                            }
                            // Voice profile cycle
                            const int voice_fy = ts_y0 + 3*20;
                            if (lx >= ppx+72 && lx < ppx+PW-8 && ly >= voice_fy-2 && ly < voice_fy+18) {
                                if (voice_profiles_ && !voice_profiles_->empty()) {
                                    const int n = (int)voice_profiles_->size();
                                    const std::string cur = t->text_voice.empty()
                                        ? (*voice_profiles_)[0].id : t->text_voice;
                                    int idx = 0;
                                    for (int i = 0; i < n; ++i)
                                        if ((*voice_profiles_)[i].id == cur) { idx = i; break; }
                                    t->text_voice = (*voice_profiles_)[(idx+1)%n].id;
                                    dirty_ = true;
                                }
                                return Action::Continue;
                            }
                            // Dismiss cycle
                            const int dim_fy = ts_y0 + 4*20;
                            if (lx >= ppx+72 && lx < ppx+PW-8 && ly >= dim_fy-2 && ly < dim_fy+18) {
                                static const char* k_dm[] = {"input","auto","timed:5"};
                                const std::string cur = t->text_dismiss.empty() ? "input" : t->text_dismiss;
                                int idx = 0;
                                for (int i = 0; i < 3; ++i) if (cur==k_dm[i]) { idx=i; break; }
                                t->text_dismiss = k_dm[(idx+1)%3]; dirty_ = true;
                                return Action::Continue;
                            }
                        }
                        return Action::Continue; // inside panel, no specific control
                    }
                }
            }
            if (entity_props_open_) {
                EntityInstance* ei = map.find_entity_by_uid(entity_props_uid_);
                if (ei) {
                    const EntityDef* def2 = entity_defs_ ?
                        find_entity_def(*entity_defs_, ei->type_id) : nullptr;
                    constexpr int PW  = Renderer::k_props_w;
                    constexpr int PAD = 8, ROW = 20;
                    constexpr int k_ts = 10+16+ROW*6+4;
                    const bool is_ct_c = (def2 && def2->category == "container");
                    const int n_g_c = is_ct_c ? (int)ei->container_guaranteed.size() : 0;
                    const int k_item_s = (ei->type_id == "item_pickup") ? (10+5+16+ROW) : 0;
                    const int k_ct_s = is_ct_c ? (10+5+16 + ROW*3 + (n_g_c>0?6+n_g_c*ROW:0) + 6 + ROW) : 0;
                    const int ph = 24 + ROW*8 + 8 + (def2&&def2->facing_required?ROW+4:0) + 32 + k_ts + k_item_s + k_ct_s;
                    int ppx = std::min(props_sx_, renderer.screen_w() - PW - 4);
                    int ppy = std::min(props_sy_, renderer.screen_h() - ph - 4);
                    ppx = std::max(ppx, 0); ppy = std::max(ppy, k_menu_bar_h);
                    if (lx >= ppx && lx < ppx+PW && ly >= ppy && ly < ppy+ph) {
                        int y = ppy + 26;
                        y += ROW; // type row
                        y += ROW; // cat row
                        // Label click
                        if (lx >= ppx+60 && lx < ppx+PW-PAD && ly >= y && ly < y+ROW) {
                            label_editing_  = true;
                            label_edit_uid_ = entity_props_uid_;
                            label_edit_buf_ = ei->label;
                            SDL_StartTextInput();
                            return Action::Continue;
                        }
                        y += ROW;
                        // Facing buttons
                        if (def2 && def2->facing_required) {
                            static const char dirs[] = {'N','E','S','W'};
                            int bx = ppx + 60;
                            for (int d = 0; d < 4; ++d) {
                                if (lx >= bx && lx < bx+20 && ly >= y-1 && ly < y+18) {
                                    ei->facing = dirs[d]; dirty_ = true;
                                    return Action::Continue;
                                }
                                bx += 24;
                            }
                            y += ROW+4;
                        }
                        // Functional toggle
                        if (lx >= ppx+60 && lx < ppx+76 && ly >= y && ly < y+16) {
                            ei->functional = !ei->functional; dirty_ = true;
                            return Action::Continue;
                        }
                        y += ROW; // func row
                        y += ROW; // shift row
                        // Sprite texture Pick button
                        const int pick_x = ppx + PW - PAD - 38;
                        if (lx >= pick_x && lx < pick_x+38 && ly >= y && ly < y+18) {
                            tex_pick_target_ = TexPickTarget::EntitySprite;
                            tex_pick_uid_    = entity_props_uid_;
                            texture_picker_.open(atlases_, -1);
                            return Action::Continue;
                        }
                        y += ROW; // sprite texture row
                        y += ROW; // position row (read-only display)

                        // ── Item ID section click handling (item_pickup only) ─────────
                        if (ei->type_id == "item_pickup") {
                            constexpr int k_item_hdr = 10+5+16; // sep gap + sep + header
                            const int iid_y = y + k_item_hdr;
                            if (lx >= ppx+72 && lx < ppx+PW-PAD && ly >= iid_y-2 && ly < iid_y+18) {
                                if (active_text_field_ == k_tfield_item_id && !text_field_is_tile_) {
                                    active_text_field_ = k_tfield_none; SDL_StopTextInput();
                                } else {
                                    label_editing_ = false;
                                    if (active_text_field_) SDL_StopTextInput();
                                    active_text_field_  = k_tfield_item_id;
                                    text_field_is_tile_ = false;
                                    text_field_buf_     = ei->item_id;
                                    SDL_StartTextInput();
                                }
                                return Action::Continue;
                            }
                            y += k_item_hdr + 20; // advance past item section
                        }

                        // ── Text section click handling (entity props) ────────────────
                        {
                            const int ts_y0 = y + 10 + 16; // first text field y
                            for (int fid = k_tfield_desc; fid <= k_tfield_pickup; ++fid) {
                                const int fy = ts_y0 + (fid-1)*20;
                                if (lx >= ppx+72 && lx < ppx+PW-8 && ly >= fy-2 && ly < fy+18) {
                                    if (active_text_field_ == fid && !text_field_is_tile_) {
                                        active_text_field_ = k_tfield_none; SDL_StopTextInput();
                                    } else {
                                        label_editing_ = false;
                                        if (active_text_field_) SDL_StopTextInput();
                                        active_text_field_  = fid;
                                        text_field_is_tile_ = false;
                                        if (fid==k_tfield_desc)     text_field_buf_ = ei->text_desc;
                                        else if (fid==k_tfield_narrator) text_field_buf_ = ei->text_narrator;
                                        else if (fid==k_tfield_readable) text_field_buf_ = ei->text_readable;
                                        else                             text_field_buf_ = ei->text_pickup;
                                        SDL_StartTextInput();
                                    }
                                    return Action::Continue;
                                }
                            }
                            // Voice profile cycle
                            const int voice_fy = ts_y0 + 4*20;
                            if (lx >= ppx+72 && lx < ppx+PW-8 && ly >= voice_fy-2 && ly < voice_fy+18) {
                                if (voice_profiles_ && !voice_profiles_->empty()) {
                                    const int n = (int)voice_profiles_->size();
                                    const std::string cur = ei->text_voice.empty()
                                        ? (*voice_profiles_)[0].id : ei->text_voice;
                                    int idx = 0;
                                    for (int i = 0; i < n; ++i)
                                        if ((*voice_profiles_)[i].id == cur) { idx = i; break; }
                                    ei->text_voice = (*voice_profiles_)[(idx+1)%n].id;
                                    dirty_ = true;
                                }
                                return Action::Continue;
                            }
                            // Dismiss cycle
                            const int dim_fy = ts_y0 + 5*20;
                            if (lx >= ppx+72 && lx < ppx+PW-8 && ly >= dim_fy-2 && ly < dim_fy+18) {
                                static const char* k_dm[] = {"input","auto","timed:5"};
                                const std::string cur = ei->text_dismiss.empty() ? "input" : ei->text_dismiss;
                                int idx = 0;
                                for (int i = 0; i < 3; ++i) if (cur==k_dm[i]) { idx=i; break; }
                                ei->text_dismiss = k_dm[(idx+1)%3]; dirty_ = true;
                                return Action::Continue;
                            }
                            y += k_ts;
                        }

                        // ── Container section click handling ──────────────────────────
                        if (def2 && def2->category == "container") {
                            constexpr int k_ct_hdr = 10+5+16;
                            const int n_g = (int)ei->container_guaranteed.size();
                            const int loot_y = y + k_ct_hdr;
                            // Loot field
                            if (lx >= ppx+72 && lx < ppx+PW-PAD && ly >= loot_y-2 && ly < loot_y+18) {
                                if (active_text_field_ == k_tfield_container_loot && !text_field_is_tile_) {
                                    active_text_field_ = k_tfield_none; SDL_StopTextInput();
                                } else {
                                    label_editing_ = false;
                                    if (active_text_field_) SDL_StopTextInput();
                                    active_text_field_  = k_tfield_container_loot;
                                    text_field_is_tile_ = false;
                                    text_field_buf_     = ei->container_loot_table;
                                    SDL_StartTextInput();
                                }
                                return Action::Continue;
                            }
                            const int rolls_y = loot_y + ROW;
                            if (lx >= ppx+72 && lx < ppx+132 && ly >= rolls_y-2 && ly < rolls_y+18) {
                                if (active_text_field_ == k_tfield_container_rolls && !text_field_is_tile_) {
                                    active_text_field_ = k_tfield_none; SDL_StopTextInput();
                                } else {
                                    label_editing_ = false;
                                    if (active_text_field_) SDL_StopTextInput();
                                    active_text_field_  = k_tfield_container_rolls;
                                    text_field_is_tile_ = false;
                                    text_field_buf_     = std::to_string(ei->container_rng_rolls);
                                    SDL_StartTextInput();
                                }
                                return Action::Continue;
                            }
                            const int locked_y = loot_y + ROW*2;
                            if (lx >= ppx+72 && lx < ppx+88 && ly >= locked_y && ly < locked_y+16) {
                                ei->container_locked = !ei->container_locked; dirty_ = true;
                                return Action::Continue;
                            }
                            int gy = loot_y + ROW*3;
                            if (n_g > 0) {
                                gy += 6;
                                for (int gi = 0; gi < n_g; ++gi) {
                                    const int bx = ppx + PW - PAD - 20;
                                    if (lx >= bx && lx < bx+18 && ly >= gy-1 && ly < gy+15) {
                                        ei->container_guaranteed.erase(ei->container_guaranteed.begin() + gi);
                                        dirty_ = true;
                                        return Action::Continue;
                                    }
                                    gy += ROW;
                                }
                            }
                            const int add_y = gy + 6;
                            if (lx >= ppx+50 && lx < ppx+PW-PAD && ly >= add_y-2 && ly < add_y+18) {
                                if (active_text_field_ == k_tfield_container_add && !text_field_is_tile_) {
                                    active_text_field_ = k_tfield_none; SDL_StopTextInput();
                                } else {
                                    label_editing_ = false;
                                    if (active_text_field_) SDL_StopTextInput();
                                    active_text_field_  = k_tfield_container_add;
                                    text_field_is_tile_ = false;
                                    text_field_buf_     = "";
                                    SDL_StartTextInput();
                                }
                                return Action::Continue;
                            }
                            y += k_ct_hdr + ROW*3 + (n_g>0 ? 6+n_g*ROW : 0) + 6 + ROW;
                        }

                        // Delete button
                        if (lx >= ppx+PAD && lx < ppx+PW-PAD && ly >= y && ly < y+24) {
                            map.remove_entity(entity_props_uid_);
                            entity_props_open_ = false; dirty_ = true;
                            return Action::Continue;
                        }
                        return Action::Continue; // inside panel, no specific control
                    }
                }
            }
            // Room props panel
            if (room_props_open_) {
                constexpr int PW = Renderer::k_props_w;
                constexpr int PH = 180, PAD = 8, ROW = 22;
                int ppx = std::min(room_props_sx_, renderer.screen_w() - PW - 4);
                int ppy = std::min(room_props_sy_, renderer.screen_h() - PH - 4);
                ppx = std::max(ppx, 0); ppy = std::max(ppy, k_menu_bar_h);
                if (lx >= ppx && lx < ppx+PW && ly >= ppy && ly < ppy+PH) {
                    const int n_pur = (int)room_purposes_.size();
                    // Name field
                    if (lx >= ppx+60 && lx < ppx+PW-PAD && ly >= ppy+24 && ly < ppy+48) {
                        if (!room_name_editing_) { room_name_editing_ = true; SDL_StartTextInput(); }
                        return Action::Continue;
                    }
                    // Purpose cycle
                    if (lx >= ppx+60 && lx < ppx+PW-PAD && ly >= ppy+46 && ly < ppy+70) {
                        if (n_pur > 0) {
                            const int mid = ppx + 60 + (PW - PAD - 60) / 2;
                            if (lx < mid) room_purpose_idx_ = (room_purpose_idx_ - 1 + n_pur) % n_pur;
                            else          room_purpose_idx_ = (room_purpose_idx_ + 1) % n_pur;
                        }
                        return Action::Continue;
                    }
                    // Save button
                    if (lx >= ppx+PAD && lx < ppx+PW/2-4 && ly >= ppy+104 && ly < ppy+128) {
                        if (room_name_editing_) { room_name_editing_ = false; SDL_StopTextInput(); }
                        RoomDef rd;
                        rd.seed_x  = room_props_seed_x_;
                        rd.seed_y  = room_props_seed_y_;
                        rd.name    = room_name_buf_;
                        rd.purpose = (n_pur > 0 && !room_purpose_tags_.empty())
                            ? room_purpose_tags_[room_purpose_idx_ % n_pur] : "";
                        const RoomDef* existing = map.find_room(rd.seed_x, rd.seed_y);
                        if (existing) rd.notes = existing->notes;
                        map.set_room(rd);
                        dirty_ = true; room_props_open_ = false;
                        flash("Room saved", 1500);
                        return Action::Continue;
                    }
                    // Remove button
                    if (lx >= ppx+PW/2+4 && lx < ppx+PW-PAD && ly >= ppy+104 && ly < ppy+128) {
                        if (room_name_editing_) { room_name_editing_ = false; SDL_StopTextInput(); }
                        map.remove_room(room_props_seed_x_, room_props_seed_y_);
                        dirty_ = true; room_props_open_ = false;
                        flash("Room removed", 1500);
                        return Action::Continue;
                    }
                    return Action::Continue; // inside panel
                }
            }

            close_props();

            // ── Link line click detection ─────────────────────────────────
            link_selected_ = false;
            if (entity_layer_visible_) {
                const int half = cam.tile_px() / 2;
                for (const auto& lsrc : map.get_entities()) {
                    if (lsrc.linked_id.empty()) continue;
                    uint32_t tuid = 0;
                    try { tuid = (uint32_t)std::stoul(lsrc.linked_id); } catch (...) {}
                    const EntityInstance* ltgt = tuid ? map.find_entity_by_uid(tuid) : nullptr;
                    if (!ltgt) {
                        for (const auto& le : map.get_entities())
                            if (!le.label.empty() && le.label == lsrc.linked_id && le.uid != lsrc.uid)
                                { ltgt = &le; break; }
                    }
                    if (!ltgt) continue;
                    const float x1 = (float)(cam.to_screen_x(lsrc.x) + half);
                    const float y1 = (float)(cam.to_screen_y(lsrc.y) + half);
                    const float x2 = (float)(cam.to_screen_x(ltgt->x) + half);
                    const float y2 = (float)(cam.to_screen_y(ltgt->y) + half);
                    const float dx = x2-x1, dy = y2-y1, len2 = dx*dx+dy*dy;
                    float dist2;
                    if (len2 < 1.f) {
                        const float qx = lx-x1, qy = ly-y1; dist2 = qx*qx+qy*qy;
                    } else {
                        const float t = std::max(0.f, std::min(1.f, ((lx-x1)*dx+(ly-y1)*dy)/len2));
                        const float px2 = x1+t*dx, py2 = y1+t*dy;
                        const float qx = lx-px2, qy = ly-py2; dist2 = qx*qx+qy*qy;
                    }
                    if (dist2 < 36.f) {
                        link_selected_ = true;
                        link_src_uid_  = lsrc.uid;
                        return Action::Continue;
                    }
                }
            }

            // Click in left panel
            if ((palette_open_ || entity_panel_open_) && lx < k_palette_w && ly >= cam.map_top()) {
                if (entity_panel_open_ && entity_defs_) {
                    // Check for category header click (collapse toggle).
                    static const char* k_cat_order[] = {
                        "spawn","furniture","workstation","terminal",
                        "wall_mounted","container","door","trigger", nullptr
                    };
                    int y = cam.map_top() - entity_scroll_;
                    bool hit_hdr = false;
                    for (int ci = 0; k_cat_order[ci]; ++ci) {
                        const std::string cat = k_cat_order[ci];
                        bool has_any = false;
                        for (const auto& d : *entity_defs_)
                            if (d.category == cat) { has_any = true; break; }
                        if (!has_any) continue;

                        if (ly >= y && ly < y + k_entity_hdr_h) {
                            if (collapsed_cats_.count(cat)) collapsed_cats_.erase(cat);
                            else collapsed_cats_.insert(cat);
                            hit_hdr = true; break;
                        }
                        y += k_entity_hdr_h;
                        if (!collapsed_cats_.count(cat)) {
                            for (const auto& d : *entity_defs_)
                                if (d.category == cat) y += k_entity_row_h;
                        }
                    }
                    if (!hit_hdr) {
                        const EntityDef* def = entity_at_panel_click(lx, ly, cam);
                        if (def) {
                            selected_entity_id_ = def->id;
                            if (!def->facing_required) entity_facing_ = 'N';
                        }
                    }
                } else if (palette_open_) {
                    const int id = tile_id_at_palette_click(lx, ly, cam);
                    if (id >= 0) { paint_id_ = id; tool_ = Tool::Paint; }
                }
                return Action::Continue;
            }

            // Click on right panel
            if (right_panel_open_ && lx >= cam.map_left() + cam.screen_w())
                return Action::Continue;

            // Click in status bar
            const int strip_y = cam.map_top() + cam.screen_h();
            if (ly >= strip_y) {
                if (tile_defs_ && lx >= 4) {
                    const int idx = (lx - 4) / 36;
                    if (idx >= 0 && idx < (int)tile_defs_->size() && idx < 8) {
                        paint_id_ = (*tile_defs_)[idx].id;
                        tool_     = Tool::Paint;
                        entity_panel_open_ = false;
                    }
                }
                return Action::Continue;
            }

            left_held_ = true;

            if (texture_mode_) {
                texture_batching_ = true;
                paint_texture(cursor_tx_, cursor_ty_, map);
            } else if (entity_panel_open_ && !selected_entity_id_.empty()) {
                place_entity(cursor_tx_, cursor_ty_, map);
            } else if (tool_ == Tool::Eyedropper) {
                eyedrop(cursor_tx_, cursor_ty_, map);
            } else if (tool_ == Tool::Fill) {
                flood_fill(cursor_tx_, cursor_ty_, map);
            } else if (tool_ == Tool::RectRoom) {
                rect_active_   = true;
                rect_start_tx_ = cursor_tx_;
                rect_start_ty_ = cursor_ty_;
            } else if (tool_ == Tool::Paint) {
                painting_batch_ = true;
                paint(cursor_tx_, cursor_ty_, map);
            } else {
                painting_batch_ = true;
                erase_tile(cursor_tx_, cursor_ty_, map);
            }
        }

        if (e.button.button == SDL_BUTTON_RIGHT) {
            right_held_    = true;
            right_dragging_= false;
            right_press_lx_= lx;
            right_press_ly_= ly;
            if (texture_mode_) texture_batching_ = true;
            else painting_batch_ = true;
        }
    }

    // ── Mouse button up ───────────────────────────────────────────────────────
    if (e.type == SDL_MOUSEBUTTONUP) {
        if (e.button.button == SDL_BUTTON_LEFT) {
            if (rect_active_) {
                const bool shift = (SDL_GetModState() & KMOD_SHIFT) != 0;
                finish_rect(rect_start_tx_, rect_start_ty_,
                            cursor_tx_, cursor_ty_, shift, map);
                rect_active_ = false;
            }
            if (texture_batching_ && !tex_batch_.empty()) {
                UndoEntry u; u.kind = UndoEntry::TextureBatch;
                u.textures = std::move(tex_batch_);
                push_undo_(std::move(u));
                tex_batch_.clear();
            }
            texture_batching_ = false;
            if (painting_batch_ && !batch_tiles_.empty()) {
                UndoEntry u; u.kind = UndoEntry::TileBatch;
                u.tiles = std::move(batch_tiles_);
                push_undo_(std::move(u));
                batch_tiles_.clear();
            }
            painting_batch_ = false;
            left_held_ = false;
        }
        if (e.button.button == SDL_BUTTON_MIDDLE) middle_held_ = false;
        if (e.button.button == SDL_BUTTON_RIGHT) {
            if (texture_batching_ && !tex_batch_.empty()) {
                UndoEntry u; u.kind = UndoEntry::TextureBatch;
                u.textures = std::move(tex_batch_);
                push_undo_(std::move(u));
                tex_batch_.clear();
            }
            texture_batching_ = false;
            if (painting_batch_ && !batch_tiles_.empty()) {
                UndoEntry u; u.kind = UndoEntry::TileBatch;
                u.tiles = std::move(batch_tiles_);
                push_undo_(std::move(u));
                batch_tiles_.clear();
            }
            painting_batch_ = false;

            if (!right_dragging_) {
                // Single right-click: open properties panel.
                const int lx = e.button.x, ly = e.button.y;
                update_cursor(lx, ly, cam);

                // Room overlay: RMB on floor tile → room props panel
                if (show_room_overlay_ && cursor_tx_ >= 0 && cursor_ty_ >= 0 &&
                    !room_hover_tiles_.empty()) {
                    const Tile& rt = map.get_tile(cursor_tx_, cursor_ty_);
                    if (rt.tile_id == 2) {
                        room_props_open_   = true;
                        room_props_sx_     = lx;
                        room_props_sy_     = ly;
                        room_props_seed_x_ = room_hover_seed_x_;
                        room_props_seed_y_ = room_hover_seed_y_;
                        const RoomDef* rd = map.find_room(room_hover_seed_x_, room_hover_seed_y_);
                        if (rd) {
                            room_name_buf_ = rd->name;
                            room_purpose_idx_ = 0;
                            for (int i = 0; i < (int)room_purpose_tags_.size(); ++i)
                                if (room_purpose_tags_[i] == rd->purpose) { room_purpose_idx_ = i; break; }
                        } else {
                            room_name_buf_ = ""; room_purpose_idx_ = 0;
                        }
                        room_name_editing_ = false;
                        entity_props_open_ = false; tile_props_open_ = false;
                        right_held_ = false; right_dragging_ = false;
                        return Action::Continue;
                    }
                }

                const EntityInstance* ent = map.find_entity_at(cursor_tx_, cursor_ty_);
                if (ent) {
                    entity_props_open_ = true;
                    tile_props_open_   = false;
                    entity_props_uid_  = ent->uid;
                    props_sx_          = lx;
                    props_sy_          = ly;
                } else if (cursor_tx_ >= 0 && cursor_ty_ >= 0) {
                    tile_props_open_   = true;
                    entity_props_open_ = false;
                    tile_props_tx_     = cursor_tx_;
                    tile_props_ty_     = cursor_ty_;
                    props_sx_          = lx;
                    props_sy_          = ly;
                }
            }
            right_held_     = false;
            right_dragging_ = false;
        }
    }

    // ── Key down ──────────────────────────────────────────────────────────────
    if (e.type == SDL_KEYDOWN && !e.key.repeat) {
        const SDL_Keycode sym  = e.key.keysym.sym;
        const bool        ctrl = (e.key.keysym.mod & KMOD_CTRL) != 0;

        if (sym == SDLK_F5)       { do_save(map); return Action::Play; }
        if (sym == SDLK_s && ctrl) { do_save(map); return Action::Continue; }
        if (sym == SDLK_z && ctrl) { undo_(map);   return Action::Continue; }
        if (sym == SDLK_y && ctrl) { redo_(map);   return Action::Continue; }

        if (sym >= SDLK_0 && sym <= SDLK_9) {
            paint_id_           = sym - SDLK_0;
            tool_               = Tool::Paint;
            entity_panel_open_  = false;
        }

        // Entity mode: rotate facing with R
        if (entity_panel_open_ && !selected_entity_id_.empty()) {
            if (sym == SDLK_r) {
                static const char cycle[] = "NESW";
                const char* p = std::strchr(cycle, entity_facing_);
                entity_facing_ = p && p[1] ? p[1] : cycle[0];
                return Action::Continue;
            }
        }

        // Entity properties: facing buttons handled as mouse clicks in render
        if (entity_props_open_) {
            if (sym == SDLK_DELETE) {
                map.remove_entity(entity_props_uid_);
                entity_props_open_ = false;
                dirty_ = true;
                return Action::Continue;
            }
        }

        // Selected link line: Delete removes the link
        if (link_selected_) {
            if (sym == SDLK_DELETE) {
                EntityInstance* ls = map.find_entity_by_uid(link_src_uid_);
                if (ls) { ls->linked_id.clear(); dirty_ = true; }
                link_selected_ = false;
                flash("Link removed", 1500);
                return Action::Continue;
            }
        }

        // Tool keys (only when not in entity mode)
        if (!entity_panel_open_) {
            if (sym == SDLK_b) { tool_ = Tool::Paint; texture_mode_ = false; }
            if (sym == SDLK_x) tool_ = (tool_ == Tool::Erase)   ? Tool::Paint : Tool::Erase;
            if (sym == SDLK_f) tool_ = (tool_ == Tool::Fill)     ? Tool::Paint : Tool::Fill;
            if (sym == SDLK_q) tool_ = (tool_ == Tool::Eyedropper) ? Tool::Paint : Tool::Eyedropper;
            // R = room overlay toggle; Ctrl+R = rect room tool
            if (sym == SDLK_r && ctrl)
                tool_ = (tool_ == Tool::RectRoom) ? Tool::Paint : Tool::RectRoom;
            if (sym == SDLK_r && !ctrl) {
                show_room_overlay_ = !show_room_overlay_;
                room_hover_last_tx_ = -1; room_hover_last_ty_ = -1;
            }
        }

        // Zoom +/-
        if (sym == SDLK_PLUS || sym == SDLK_EQUALS || sym == SDLK_KP_PLUS)
            cam.zoom_step(1);
        if (sym == SDLK_MINUS || sym == SDLK_KP_MINUS)
            cam.zoom_step(-1);

        // Spawn marker — also moves/creates the player_spawn entity
        if (sym == SDLK_m && cursor_tx_ >= 0 && cursor_ty_ >= 0) {
            map.set_spawn(cursor_tx_, cursor_ty_);
            uint32_t spawn_uid = 0;
            for (const auto& ent : map.get_entities())
                if (ent.type_id == "player_spawn") { spawn_uid = ent.uid; break; }
            if (spawn_uid != 0) {
                EntityInstance* se = map.find_entity_by_uid(spawn_uid);
                if (se) { se->x = cursor_tx_; se->y = cursor_ty_; }
            } else {
                EntityInstance ne;
                ne.type_id       = "player_spawn";
                ne.x             = cursor_tx_;
                ne.y             = cursor_ty_;
                ne.facing        = 'N';
                ne.functional    = false;
                ne.visible_shift = "both";
                map.add_entity(ne);
            }
            dirty_ = true;
            flash("Spawn point set", 1500);
        }

        // View toggles
        if (sym == SDLK_p)  show_pathability_ = !show_pathability_;
        if (sym == SDLK_o)  show_room_tags_   = !show_room_tags_;
        if (sym == SDLK_l)  show_light_       = !show_light_;
        if (sym == SDLK_g)  show_grid_        = !show_grid_;
        if (sym == SDLK_v)  entity_layer_visible_ = !entity_layer_visible_;

        // Texture mode / picker
        if (sym == SDLK_t) {
            texture_mode_ = !texture_mode_;
            if (texture_mode_ && texture_paint_id_.empty())
                texture_picker_.open(atlases_, cursor_tx_ >= 0 ?
                    map.get_tile(cursor_tx_, cursor_ty_).tile_id : -1);
        }
        if (sym == SDLK_BACKSLASH) {
            texture_picker_.open(atlases_, cursor_tx_ >= 0 ?
                map.get_tile(cursor_tx_, cursor_ty_).tile_id : -1);
        }

        // Panel toggles
        if (sym == SDLK_e) {
            entity_panel_open_ = !entity_panel_open_;
            if (entity_panel_open_) palette_open_ = false;
            update_viewport(cam, renderer.screen_w(), renderer.screen_h());
        }
        if (sym == SDLK_TAB) {
            palette_open_ = !palette_open_;
            if (palette_open_) entity_panel_open_ = false;
            update_viewport(cam, renderer.screen_w(), renderer.screen_h());
        }
        if (sym == SDLK_RIGHTBRACKET) {
            right_panel_open_ = !right_panel_open_;
            update_viewport(cam, renderer.screen_w(), renderer.screen_h());
        }
    }

    return Action::Continue;
}

// ── Per-frame update ──────────────────────────────────────────────────────────

void Editor::update(Camera& cam, const Map& map) {
    if (!middle_held_) {
        const Uint8* ks = SDL_GetKeyboardState(nullptr);
        if (ks[SDL_SCANCODE_LEFT]  || ks[SDL_SCANCODE_A]) cam.pan(-k_pan_px, 0);
        if (ks[SDL_SCANCODE_RIGHT] || ks[SDL_SCANCODE_D]) cam.pan( k_pan_px, 0);
        if (ks[SDL_SCANCODE_UP]    || ks[SDL_SCANCODE_W]) cam.pan(0, -k_pan_px);
        if (ks[SDL_SCANCODE_DOWN]  || ks[SDL_SCANCODE_S]) cam.pan(0,  k_pan_px);
    }

    // Room flood-fill: recompute when cursor moves
    if (show_room_overlay_ && cursor_tx_ >= 0 && cursor_ty_ >= 0) {
        if (cursor_tx_ != room_hover_last_tx_ || cursor_ty_ != room_hover_last_ty_) {
            room_hover_last_tx_ = cursor_tx_;
            room_hover_last_ty_ = cursor_ty_;
            room_hover_tiles_.clear();
            room_hover_is_open_ = false;
            room_hover_seed_x_  = cursor_tx_;
            room_hover_seed_y_  = cursor_ty_;

            if (map.get_tile(cursor_tx_, cursor_ty_).tile_id == 2) {
                const int W = map.get_width(), H = map.get_height();
                std::vector<bool> visited(W * H, false);
                std::vector<std::pair<int,int>> stk;
                stk.reserve(256);
                stk.push_back({cursor_tx_, cursor_ty_});
                bool is_open = false;
                static constexpr int k_max_flood = 4096;
                while (!stk.empty() && (int)room_hover_tiles_.size() < k_max_flood) {
                    auto [cx, cy] = stk.back(); stk.pop_back();
                    if (cx < 0 || cy < 0 || cx >= W || cy >= H) { is_open = true; continue; }
                    const int idx = cy * W + cx;
                    if (visited[idx]) continue;
                    visited[idx] = true;
                    const int tid = map.get_tile(cx, cy).tile_id;
                    if (tid == 0)            { is_open = true; continue; }
                    if (tid == 1 || tid == 3) continue;
                    room_hover_tiles_.push_back({cx, cy});
                    stk.push_back({cx-1,cy}); stk.push_back({cx+1,cy});
                    stk.push_back({cx,cy-1}); stk.push_back({cx,cy+1});
                }
                if ((int)room_hover_tiles_.size() >= k_max_flood) is_open = true;
                room_hover_is_open_ = is_open;
            }
        }
    } else if (!show_room_overlay_) {
        room_hover_tiles_.clear();
        room_hover_last_tx_ = -1;
        room_hover_last_ty_ = -1;
    }
}

// ── Rendering ─────────────────────────────────────────────────────────────────

void Editor::render(Renderer& renderer, const Map& map, const Camera& cam) const {
    if (show_grid_)        renderer.draw_grid(cam);
    if (show_pathability_) renderer.draw_pathability_overlay(map, cam);
    if (show_room_tags_)   renderer.draw_room_tags_overlay(map, cam);
    if (show_room_overlay_ && !room_hover_tiles_.empty())
        renderer.draw_room_overlay(room_hover_tiles_, room_hover_is_open_,
                                   room_hover_seed_x_, room_hover_seed_y_, cam);

    if (entity_layer_visible_ && entity_defs_)
        renderer.draw_entity_layer(map, *entity_defs_, cam);

    if (entity_layer_visible_)
        renderer.draw_entity_link_lines(map, cam, link_selected_, link_src_uid_);

    renderer.draw_spawn_marker(map.get_spawn_x(), map.get_spawn_y(), cam);

    // Cursor / preview
    if (entity_panel_open_ && !selected_entity_id_.empty() &&
        cursor_tx_ >= 0 && cursor_ty_ >= 0 && entity_defs_) {
        const EntityDef* def = find_entity_def(*entity_defs_, selected_entity_id_);
        if (def) renderer.draw_entity_placement_cursor(*def, entity_facing_,
                                                       cursor_tx_, cursor_ty_, cam);
    } else if (rect_active_ && cursor_tx_ >= 0 && cursor_ty_ >= 0) {
        const bool shift = (SDL_GetModState() & KMOD_SHIFT) != 0;
        renderer.draw_rect_preview(rect_start_tx_, rect_start_ty_,
                                   cursor_tx_, cursor_ty_, shift, cam);
    } else if (cursor_tx_ >= 0 && cursor_ty_ >= 0) {
        renderer.draw_editor_cursor(cursor_tx_, cursor_ty_, cam);
    }

    // Left panel
    if (entity_panel_open_ && entity_defs_)
        renderer.draw_entity_panel(*entity_defs_, selected_entity_id_,
                                   collapsed_cats_, entity_scroll_,
                                   cam.map_top(), cam.screen_h(), k_palette_w);
    else if (palette_open_ && tile_defs_)
        renderer.draw_tile_palette(*tile_defs_, paint_id_, palette_scroll_,
                                   cam.map_top(), cam.screen_h(), k_palette_w);

    if (right_panel_open_) {
        const int rx = cam.map_left() + cam.screen_w();
        EditorUIState rui{};
        rui.tile_id     = paint_id_;
        rui.tool        = tool_name();
        rui.cursor_tx   = cursor_tx_;
        rui.cursor_ty   = cursor_ty_;
        rui.dirty       = dirty_;
        rui.zoom_pct    = (int)(cam.zoom() * 100.f + 0.5f);
        rui.map_w       = map.get_width();
        rui.map_h       = map.get_height();
        rui.entity_mode = entity_panel_open_;
        rui.entity_type = entity_panel_open_ && !selected_entity_id_.empty() ?
                          selected_entity_id_.c_str() : nullptr;
        rui.entity_facing = entity_facing_;
        renderer.draw_right_panel(rui, rx, cam.map_top(), k_right_panel_w, cam.screen_h());
    }

    // Floating properties panels
    if (tile_props_open_) {
        const Tile& tile = map.get_tile(tile_props_tx_, tile_props_ty_);
        const TileDef* tdef = tile_defs_ ?
            &get_tile_def(*tile_defs_, tile.tile_id) : nullptr;
        constexpr int W = Renderer::k_props_w;
        const bool is_door = (tile.tile_id == 3);
        const int door_tex_h = is_door ? (10 + 8 + 16 + 3*28) : 0;
        constexpr int k_ts = 10+16+20*5+4;
        const int h = 24+18+4+22*6+16+28 + door_tex_h + k_ts;
        const int ppx = std::max(0, std::min(props_sx_, renderer.screen_w()-W-4));
        const int ppy = std::max(k_menu_bar_h, std::min(props_sy_, renderer.screen_h()-h-4));
        TextFieldEdit tfe;
        if (text_field_is_tile_ && active_text_field_) {
            tfe.active = active_text_field_;
            tfe.buf    = text_field_buf_.c_str();
        }
        renderer.draw_tile_props(tile, tdef, tile_props_tx_, tile_props_ty_, ppx, ppy,
                                 active_text_field_ && text_field_is_tile_ ? &tfe : nullptr);
    }
    if (entity_props_open_) {
        const EntityInstance* ei = map.find_entity_by_uid(entity_props_uid_);
        if (ei) {
            const EntityDef* def = entity_defs_ ?
                find_entity_def(*entity_defs_, ei->type_id) : nullptr;
            constexpr int W = Renderer::k_props_w;
            constexpr int k_ts = 10+16+20*6+4;
            const bool is_ct_r = (def && def->category == "container");
            const int n_g_r = is_ct_r ? (int)ei->container_guaranteed.size() : 0;
            const int k_item_s_r = (ei->type_id == "item_pickup") ? (10+5+16+20) : 0;
            const int k_ct_s_r = is_ct_r ? (10+5+16 + 20*3 + (n_g_r>0?6+n_g_r*20:0) + 6 + 20) : 0;
            const int h = 24 + 20*8 + 8 + (def&&def->facing_required?24:0) + 32 + k_ts + k_item_s_r + k_ct_s_r;
            const int ppx = std::max(0, std::min(props_sx_, renderer.screen_w()-W-4));
            const int ppy = std::max(k_menu_bar_h, std::min(props_sy_, renderer.screen_h()-h-4));
            const bool editing = label_editing_ && label_edit_uid_ == entity_props_uid_;
            const char* lbl_ov = editing ? label_edit_buf_.c_str() : nullptr;
            TextFieldEdit tfe;
            if (!text_field_is_tile_ && active_text_field_) {
                tfe.active = active_text_field_;
                tfe.buf    = text_field_buf_.c_str();
            }
            renderer.draw_entity_props(*ei, def, ppx, ppy, lbl_ov, editing,
                                       active_text_field_ && !text_field_is_tile_ ? &tfe : nullptr);
        } else {
            entity_props_open_ = false;
        }
    }
    if (room_props_open_ && room_props_seed_x_ >= 0) {
        constexpr int PW = Renderer::k_props_w;
        constexpr int PH = 180;
        const int ppx = std::max(0, std::min(room_props_sx_, renderer.screen_w()-PW-4));
        const int ppy = std::max(k_menu_bar_h, std::min(room_props_sy_, renderer.screen_h()-PH-4));
        RoomDef disp_room;
        disp_room.seed_x = room_props_seed_x_;
        disp_room.seed_y = room_props_seed_y_;
        disp_room.name   = room_name_buf_;
        const RoomDef* rd = map.find_room(room_props_seed_x_, room_props_seed_y_);
        if (rd) { disp_room.purpose = rd->purpose; disp_room.notes = rd->notes; }
        renderer.draw_room_props(disp_room, room_purposes_, room_purpose_idx_,
                                  ppx, ppy, room_name_editing_, room_name_buf_);
    }

    const bool flash_active = SDL_GetTicks() < flash_until_;
    EditorUIState ui{};
    ui.tile_id      = paint_id_;
    ui.tool         = tool_name();
    ui.cursor_tx    = cursor_tx_;
    ui.cursor_ty    = cursor_ty_;
    ui.dirty        = dirty_;
    ui.path_on      = show_pathability_;
    ui.rooms_on     = show_room_tags_;
    ui.room_ov_on   = show_room_overlay_;
    ui.light_on     = show_light_;
    ui.grid_on      = show_grid_;
    ui.entity_on    = entity_layer_visible_;
    ui.zoom_pct     = (int)(cam.zoom() * 100.f + 0.5f);
    ui.map_w        = map.get_width();
    ui.map_h        = map.get_height();
    ui.flash_msg    = flash_active ? flash_msg_.c_str() : nullptr;
    ui.entity_mode   = entity_panel_open_;
    ui.entity_type   = entity_panel_open_ && !selected_entity_id_.empty() ?
                       selected_entity_id_.c_str() : nullptr;
    ui.entity_facing = entity_facing_;
    ui.texture_mode  = texture_mode_;
    ui.texture_id    = texture_mode_ && !texture_paint_id_.empty() ?
                       texture_paint_id_.c_str() : nullptr;
    renderer.draw_editor_ui(ui);

    renderer.draw_menu_bar(menu_bar_);

    if (show_shortcuts_)
        renderer.draw_shortcuts_overlay();

    if (texture_picker_.is_open())
        texture_picker_.render(renderer);
}
