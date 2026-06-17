#include <SDL2/SDL.h>
#include <iostream>
#include <string>
#include <vector>
#include "Map.h"
#include "Player.h"
#include "Camera.h"
#include "FogOfWar.h"
#include "Renderer.h"
#include "Editor.h"
#include "TileDef.h"
#include "EntityDef.h"
#include "Settings.h"
#include "PauseMenu.h"
#include "PlaytestNotes.h"
#include "SettingsPanel.h"
#include "MainMenu.h"
#include "SaveSystem.h"
#include "TextureAtlas.h"
#include "GameState.h"
#include "VoiceProfile.h"
#include "TextQueue.h"
#include "ItemDef.h"
#include "PlayerInventory.h"
#include "InventoryUI.h"
#include "LootTable.h"
#include "ContainerUI.h"
#include "TerminalPanel.h"
#include "Hotbar.h"
#include <random>

#if __has_include(<SDL2/SDL_mixer.h>)
#include <SDL2/SDL_mixer.h>
#define HAS_SDL_MIXER 1
#else
#define HAS_SDL_MIXER 0
#endif

// ── Door interaction helpers (unchanged) ──────────────────────────────────────

static bool find_adjacent_door(int px, int py, const Map& map, int& dx, int& dy) {
    const int dirs[4][2] = {{0,-1},{0,1},{-1,0},{1,0}};
    for (const auto& d : dirs) {
        int tx = px+d[0], ty = py+d[1];
        if (map.get_tile(tx,ty).tile_id == 3) { dx=tx; dy=ty; return true; }
    }
    return false;
}

static bool interact_door(int px, int py, Map& map, bool lock_action) {
    int dx, dy;
    if (!find_adjacent_door(px, py, map, dx, dy)) return false;
    const Tile& t = map.get_tile(dx, dy);
    if (lock_action) {
        map.set_door_state(dx, dy,
            t.door_state==DoorState::Locked ? DoorState::Closed : DoorState::Locked);
        return true;
    }
    if (t.door_state==DoorState::Closed){ map.set_door_state(dx,dy,DoorState::Open);   return true; }
    if (t.door_state==DoorState::Open)  { map.set_door_state(dx,dy,DoorState::Closed); return true; }
    return false;
}

// ── Value change notification ─────────────────────────────────────────────────

static void push_value_change(TextQueue& tq, int delta) {
    TextEntry te;
    te.text             = (delta >= 0 ? "+ " : "- ") + std::to_string(std::abs(delta)) + " VAL";
    te.voice_profile_id = "station_ambient";
    te.dismiss          = "timed:1.5";
    tq.push(te);
}

// ── Container content generation ─────────────────────────────────────────────

static void generate_container_contents(Map& map,
                                         const std::vector<EntityDef>& entity_defs,
                                         const std::vector<LootTable>& loot_tables,
                                         std::mt19937& rng) {
    const auto& ents = map.get_entities();
    std::vector<uint32_t> uids;
    uids.reserve(ents.size());
    for (const auto& e : ents) uids.push_back(e.uid);

    for (uint32_t uid : uids) {
        EntityInstance* ei = map.find_entity_by_uid(uid);
        if (!ei) continue;
        const EntityDef* def = find_entity_def(entity_defs, ei->type_id);
        if (!def || def->category != "container") continue;

        ei->container_contents.clear();
        for (const auto& item_id : ei->container_guaranteed) {
            bool found = false;
            for (auto& slot : ei->container_contents)
                if (slot.first == item_id) { ++slot.second; found = true; break; }
            if (!found) ei->container_contents.push_back({item_id, 1});
        }
        if (!ei->container_loot_table.empty()) {
            const LootTable* lt = find_loot_table(loot_tables, ei->container_loot_table);
            if (lt) {
                LootTable adj = *lt;
                adj.rolls = (ei->container_rng_rolls > 0) ? ei->container_rng_rolls : lt->rolls;
                for (const auto& item_id : roll_loot(adj, rng)) {
                    if (item_id == "nothing") continue;
                    bool found = false;
                    for (auto& slot : ei->container_contents)
                        if (slot.first == item_id) { ++slot.second; found = true; break; }
                    if (!found) ei->container_contents.push_back({item_id, 1});
                }
            }
        }
    }
}

// ── Game / Playtest loop ──────────────────────────────────────────────────────

// Returns true if the player requested to return to the editor (playtest only).
static bool run_game(Renderer& renderer, Map& map, const std::string& map_path,
                     FogOfWar& fog, Camera& cam,
                     int spawn_x, int spawn_y, bool from_editor,
                     PauseMenu& pause_menu, SettingsPanel& settings_panel,
                     PlaytestNotes& notes, SaveSystem& save_system,
                     AppSettings& settings,
                     const std::vector<TileDef>& tile_defs,
                     const std::vector<EntityDef>& entity_defs,
                     const std::vector<VoiceProfile>& voice_profiles,
                     const std::vector<ItemDef>& item_defs,
                     const std::vector<ItemCategory>& item_categories,
                     const std::vector<LootTable>& loot_tables,
                     const std::string& spawn_facing    = "south",
                     PlayerInventory initial_inventory  = PlayerInventory{},
                     const std::string& player_name     = "SMITH",
                     PlayerStats initial_stats          = PlayerStats{},
                     float initial_sim_time             = 0.0f) {
    Player player;
    if (!player.load("Data/player.json")) return false;
    if (from_editor) player.set_position(spawn_x, spawn_y);
    player.set_facing_from_str(spawn_facing);
    player.set_name(player_name);
    player.set_stats(initial_stats);
    player.stats().recalc_hp_max();

    PlayerInventory inventory = std::move(initial_inventory);
    float sim_time_minutes    = initial_sim_time;

    TextQueue text_queue;
    text_queue.set_profiles(&voice_profiles);

    InventoryUI   inventory_ui;
    ContainerUI   container_ui;
    TerminalPanel terminal_panel;
    Hotbar        hotbar;

    // Right-click inspect state (0 = none active).
    uint32_t inspect_uid = 0;
    int      inspect_sx  = 0, inspect_sy = 0;

    // Generate container contents once at game start
    {
        std::mt19937 rng(std::random_device{}());
        generate_container_contents(map, entity_defs, loot_tables, rng);
    }

    int ww=renderer.screen_w(), wh=renderer.screen_h();
    cam.set_map_origin(0, 0);
    cam.set_screen(ww, wh);
    fog.init(map.get_width(), map.get_height());
    cam.set_smooth_follow(settings.cam_smooth_follow);
    cam.set_follow_speed(settings.cam_follow_speed);
    // Apply saved default zoom (find closest step)
    {
        const float tz = settings.cam_default_zoom;
        int best = 2;
        for (int i = 0; i < Camera::k_num_zoom_steps; ++i)
            if (std::abs(Camera::k_zoom_steps[i] - tz) < 0.01f) { best = i; break; }
        cam.zoom_to(Camera::k_zoom_steps[best], ww/2, wh/2);
    }
    cam.follow(player.tile_x(), player.tile_y());
    fog.update(player.tile_x(), player.tile_y(), player.sight_radius(), map);

    const MenuMode pause_mode = from_editor ? MenuMode::PLAYTEST : MenuMode::GAMEPLAY;
    const std::string map_stem = map_path.substr(map_path.find_last_of("/\\")+1);
    const std::string map_name = map_stem.substr(0, map_stem.find_last_of('.'));

    static constexpr Uint32 k_lock_hold_ms   = 800;
    static constexpr Uint32 k_move_interval_ms = 250; // one tile per 0.25 s when held
    bool   e_held=false; Uint32 e_press_ticks=0;
    bool   running=true, window_close=false, back_to_editor=false;
    bool   show_controls_=false;
    Uint32 game_start   = SDL_GetTicks();
    Uint32 prev_tick    = game_start;
    Uint32 move_last_tick = game_start; // cooldown: when the last player move was made
    GameState gs           = GameState::EXPLORING;
    bool      tick_pending = false;
    SDL_Event event;

    auto do_render = [&]() {
        renderer.clear();
        renderer.draw_map(map, fog, cam);
        renderer.draw_player(player, cam);

        // ── Tile hover info HUD ──────────────────────────────────────────────
        {
            int mx = 0, my = 0;
            SDL_GetMouseState(&mx, &my);
            const int htx = cam.to_tile_x(mx), hty = cam.to_tile_y(my);
            const Tile& ht = map.get_tile(htx, hty);
            if (ht.tile_id > 0) {
                std::vector<std::string> hlines;
                const TileDef& tdef = get_tile_def(tile_defs, ht.tile_id);
                std::string tname = tdef.name;
                if (ht.tile_id == 3) {
                    if      (ht.door_state == DoorState::Open)   tname += " (open)";
                    else if (ht.door_state == DoorState::Locked) tname += " (locked)";
                    else                                          tname += " (closed)";
                }
                hlines.push_back(tname);
                if (!ht.texture_id.empty()) {
                    const auto col = ht.texture_id.find(':');
                    hlines.push_back(col != std::string::npos
                        ? ht.texture_id.substr(col + 1) : ht.texture_id);
                }
                for (const auto& ei : map.get_entities()) {
                    if (ei.x != htx || ei.y != hty) continue;
                    const EntityDef* edef = find_entity_def(entity_defs, ei.type_id);
                    std::string ename = edef ? edef->name : ei.type_id;
                    if (!ei.label.empty()) ename += " (" + ei.label + ")";
                    hlines.push_back(ename);
                }
                renderer.draw_tile_hover_hud(hlines);
            }
        }

        if (cam.is_detached())
            renderer.draw_text("Camera free \xe2\x80\x94 move to recentre",
                               cam.map_left()+4, cam.map_top()+4, 220,200,60);

        if (gs == GameState::INVENTORY)
            inventory_ui.render(renderer, inventory, item_defs, item_categories,
                                player.stats(), player.name());

        if (gs == GameState::TERMINAL_DISPLAY)
            terminal_panel.render(renderer, sim_time_minutes);

        if (gs == GameState::CONTAINER) {
            const EntityInstance* cei = map.find_entity_by_uid(container_ui.entity_uid());
            if (cei) container_ui.render(renderer, *cei, item_defs);
        }

        // ── Combat stat corner (HP / SAN reference, top-right) ───────────────
        if (gs == GameState::COMBAT) {
            const PlayerStats& ps = player.stats();
            constexpr int CW = 190, CH = 50, CM = 8;
            const int cx = renderer.screen_w() - CW - CM;
            const int cy = CM;
            renderer.fill_rect(cx, cy, CW, CH, 14, 16, 24, 220);
            renderer.draw_rect_outline(cx, cy, CW, CH, 50, 55, 70);

            constexpr int BAR_W = 80, PAD = 6;
            const int lx = cx + PAD, bx = cx + CW - BAR_W - PAD;

            {
                renderer.draw_text("HP", lx, cy+6, 160,170,180);
                const std::string hs = std::to_string(ps.hp_current)+"/"+std::to_string(ps.hp_max);
                renderer.draw_text(hs, bx - renderer.text_width(hs) - 4, cy+6, 170,175,190);
                const float f = ps.hp_max>0 ? (float)ps.hp_current/ps.hp_max : 0.f;
                Uint8 r,g,b;
                if(f>0.5f){r=50;g=200;b=50;}else if(f>0.25f){r=220;g=180;b=20;}else{r=220;g=40;b=40;}
                renderer.fill_rect(bx, cy+6, BAR_W, 9, 20,22,32);
                renderer.fill_rect(bx, cy+6, (int)(BAR_W*f), 9, r,g,b);
            }
            {
                renderer.draw_text("SAN", lx, cy+26, 160,170,180);
                const std::string ss = std::to_string(ps.sanity_current)+"/"+std::to_string(ps.sanity_max);
                renderer.draw_text(ss, bx - renderer.text_width(ss) - 4, cy+26, 170,175,190);
                const float f = ps.sanity_max>0 ? (float)ps.sanity_current/ps.sanity_max : 0.f;
                Uint8 r,g,b;
                if(f>0.5f){r=180;g=200;b=230;}else if(f>0.25f){r=140;g=60;b=180;}else{r=160;g=20;b=40;}
                renderer.fill_rect(bx, cy+26, BAR_W, 9, 20,22,32);
                renderer.fill_rect(bx, cy+26, (int)(BAR_W*f), 9, r,g,b);
            }
        }

        renderer.draw_hotbar(hotbar, inventory, item_defs);

        if (inspect_uid != 0 && gs == GameState::EXPLORING) {
            const EntityInstance* ei = map.find_entity_by_uid(inspect_uid);
            if (ei) renderer.draw_inspect_tooltip(*ei, entity_defs, inspect_sx, inspect_sy);
            else    inspect_uid = 0;
        }

        if (text_queue.is_active())
            text_queue.render(renderer, renderer.screen_w(), renderer.screen_h());

        if (show_controls_)           renderer.draw_game_controls_overlay();
        if (pause_menu.is_open())     pause_menu.render(renderer);
        if (settings_panel.is_open()) settings_panel.render(renderer);
        if (notes.is_open())          notes.render(renderer);
        renderer.present();
    };

    while (running) {
        // ── Controls overlay ──────────────────────────────────────────────────
        if (show_controls_) {
            SDL_Event ce;
            while (SDL_PollEvent(&ce)) {
                if (ce.type == SDL_QUIT) { running=false; window_close=true; break; }
                if (ce.type == SDL_KEYDOWN || ce.type == SDL_MOUSEBUTTONDOWN)
                    show_controls_ = false;
            }
            do_render(); continue;
        }

        // ── Pause menu / settings event handling ──────────────────────────────
        if (settings_panel.is_open() || pause_menu.is_open() || notes.is_open()) {
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) { running=false; window_close=true; break; }
                if (event.type == SDL_WINDOWEVENT &&
                    event.window.event == SDL_WINDOWEVENT_RESIZED) {
                    cam.set_screen(event.window.data1, event.window.data2); continue;
                }
                if (settings_panel.is_open()) {
                    auto sr = settings_panel.handle_event(event, renderer);
                    if (sr == SettingsPanel::Result::Apply)  { settings = settings_panel.working(); settings_panel.close(); }
                    if (sr == SettingsPanel::Result::Close)  { settings_panel.close(); }
                    continue;
                }
                if (notes.is_open()) {
                    auto nr = notes.handle_event(event);
                    if (nr.result == PlaytestNotes::Result::Close) notes.close();
                    continue;
                }
                // Pause menu events
                PauseResult pr = pause_menu.handle_event(event, renderer);
                switch (pr) {
                    case PauseResult::Close:         break;
                    case PauseResult::SaveGame:
                        save_system.save_game("Save", map_path, player.tile_x(), player.tile_y(),
                                              (SDL_GetTicks()-game_start)/1000, false,
                                              player.facing_str(), &inventory,
                                              player.name(), &player.stats(), sim_time_minutes);
                        break;
                    case PauseResult::Settings:
                        settings_panel.open(settings);
                        break;
                    case PauseResult::GoMainMenu:
                    case PauseResult::QuitDesktop:
                    case PauseResult::SaveAndQuit:
                        running = false; break;
                    case PauseResult::ReturnEditor:
                        running = false; back_to_editor = true; break;
                    case PauseResult::Controls:
                        show_controls_ = true;
                        pause_menu.close();
                        break;
                    case PauseResult::AddNote:
                        notes.open_add(map_name, player.tile_x(), player.tile_y());
                        break;
                    case PauseResult::ViewNotes:
                        notes.open_view();
                        break;
                    default: break;
                }
            }
            do_render(); continue;
        }

        // ── Container UI event routing ────────────────────────────────────────
        if (gs == GameState::CONTAINER) {
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) { running=false; window_close=true; break; }
                if (event.type != SDL_KEYDOWN) continue;
                const SDL_Keycode sym = event.key.keysym.sym;
                EntityInstance* cei = map.find_entity_by_uid(container_ui.entity_uid());
                if (sym == SDLK_ESCAPE || !cei) { gs = GameState::EXPLORING; break; }
                const int n = (int)cei->container_contents.size();
                // Navigation — delegate to ContainerUI then re-clamp
                if (sym == SDLK_w || sym == SDLK_UP || sym == SDLK_s || sym == SDLK_DOWN) {
                    container_ui.handle_event(event, inventory, item_defs, text_queue);
                    container_ui.clamp(n);
                    continue;
                }
                // Take selected item
                if (sym == SDLK_e && !event.key.repeat) {
                    const int sel = container_ui.selected();
                    if (sel >= 0 && sel < n) {
                        auto& slot = cei->container_contents[sel];
                        inventory.add(slot.first, 1);
                        const ItemDef* idef = find_item_def(item_defs, slot.first);
                        TextEntry te;
                        te.text = "Picked up: " + (idef ? idef->name : slot.first);
                        te.voice_profile_id = "station_ambient"; te.dismiss = "auto";
                        text_queue.push(te);
                        if (idef && !idef->pickup_text.empty()) {
                            TextEntry te2;
                            te2.text = idef->pickup_text;
                            te2.voice_profile_id = idef->voice_profile.empty() ? "station_ambient" : idef->voice_profile;
                            te2.dismiss = "auto"; text_queue.push(te2);
                        }
                        --slot.second;
                        if (slot.second <= 0)
                            cei->container_contents.erase(cei->container_contents.begin() + sel);
                        container_ui.clamp((int)cei->container_contents.size());
                        if (cei->container_contents.empty()) gs = GameState::EXPLORING;
                    }
                    continue;
                }
                // Take all items
                if (sym == SDLK_r) {
                    for (const auto& slot : cei->container_contents) {
                        if (slot.second <= 0) continue;
                        inventory.add(slot.first, slot.second);
                        const ItemDef* idef = find_item_def(item_defs, slot.first);
                        TextEntry te;
                        te.text = "Picked up: " + (idef ? idef->name : slot.first)
                                  + (slot.second > 1 ? " x" + std::to_string(slot.second) : "");
                        te.voice_profile_id = "station_ambient"; te.dismiss = "auto";
                        text_queue.push(te);
                    }
                    cei->container_contents.clear();
                    gs = GameState::EXPLORING; break;
                }
            }
            do_render(); continue;
        }

        // ── Terminal display event routing ────────────────────────────────────
        if (gs == GameState::TERMINAL_DISPLAY) {
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) { running=false; window_close=true; break; }
                if (terminal_panel.handle_event(event))
                    gs = GameState::EXPLORING;
            }
            do_render(); continue;
        }

        // ── Normal game events ────────────────────────────────────────────────
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) { running=false; window_close=true; break; }
            if (event.type == SDL_WINDOWEVENT &&
                event.window.event == SDL_WINDOWEVENT_RESIZED) {
                cam.set_screen(event.window.data1, event.window.data2); continue;
            }
            if (event.type == SDL_MOUSEWHEEL) {
                hotbar.cycle_ability(event.wheel.y > 0 ? -1 : 1); continue;
            }
            if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                if (gs == GameState::INVENTORY)
                    inventory_ui.handle_mouse(event.button.x, event.button.y, renderer.screen_w());
                continue;
            }
            if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_RIGHT) {
                if (gs == GameState::EXPLORING) {
                    int lx = event.button.x, ly = event.button.y;
                    renderer.to_logical(lx, ly, lx, ly);
                    const int tx = cam.to_tile_x(lx), ty = cam.to_tile_y(ly);
                    const EntityInstance* ei = map.find_entity_at(tx, ty);
                    if (ei) { inspect_uid = ei->uid; inspect_sx = lx; inspect_sy = ly; }
                    else    { inspect_uid = 0; }
                }
                continue;
            }
            if (event.type == SDL_KEYDOWN) {
                const SDL_Keycode sym = event.key.keysym.sym;
                if (sym == SDLK_ESCAPE) {
                    if (inspect_uid) { inspect_uid = 0; continue; }
                    if (gs == GameState::INVENTORY || gs == GameState::CONTAINER
                        || gs == GameState::TERMINAL_DISPLAY) {
                        gs = GameState::EXPLORING;
                        terminal_panel.close();
                        continue;
                    }
                    pause_menu.open(pause_mode); break;
                }

                // ── Hotbar: 1-9 = slots 0-8, 0 = slot 9 ─────────────────────
                if (gs == GameState::EXPLORING || gs == GameState::INVENTORY) {
                    int hslot = -1;
                    if (sym >= SDLK_1 && sym <= SDLK_9) hslot = sym - SDLK_1;
                    else if (sym == SDLK_0)              hslot = 9;
                    if (hslot >= 0) {
                        if (gs == GameState::EXPLORING) {
                            hotbar.set_active_item(hslot == hotbar.active_item_slot ? -1 : hslot);
                        } else {
                            // Assign selected inventory item to this hotbar slot.
                            const std::string iid =
                                inventory_ui.selected_item_id(inventory, item_defs);
                            if (!iid.empty()) {
                                hotbar.assign_item(hslot, iid);
                                TextEntry te;
                                const ItemDef* d = find_item_def(item_defs, iid);
                                te.text = (d ? d->name : iid)
                                          + " assigned to slot " + std::to_string((hslot + 1) % 10) + ".";
                                te.voice_profile_id = "station_ambient";
                                te.dismiss = "auto";
                                text_queue.push(te);
                            }
                        }
                        continue;
                    }
                }
                if (sym == SDLK_F11)    { renderer.toggle_fullscreen(); break; }
                if (sym==SDLK_PLUS||sym==SDLK_EQUALS||sym==SDLK_KP_PLUS)  { cam.zoom_step( 1); continue; }
                if (sym==SDLK_MINUS||sym==SDLK_KP_MINUS)                   { cam.zoom_step(-1); continue; }

                // Open inventory (I key). Close is via Esc (handled above) or
                // inventory_ui returning Action::Close.
                if (sym == SDLK_i && !event.key.repeat && gs == GameState::EXPLORING) {
                    inventory_ui.open(inventory, item_defs, item_categories);
                    gs = GameState::INVENTORY;
                    continue;
                }

                // Route events to inventory UI.
                if (gs == GameState::INVENTORY) {
                    const auto act = inventory_ui.handle_event(event, inventory, item_defs, text_queue, player.stats());
                    if (act == InventoryUI::Action::Close) {
                        gs = GameState::EXPLORING;
                    } else if (act == InventoryUI::Action::Inspecting) {
                        gs = GameState::TEXT;
                    } else if (act == InventoryUI::Action::Dropped) {
                        EntityInstance drop_ent;
                        drop_ent.type_id = "item_pickup";
                        drop_ent.item_id = inventory_ui.last_dropped_id();
                        drop_ent.x       = player.tile_x();
                        drop_ent.y       = player.tile_y();
                        map.add_entity(drop_ent);
                    }
                    continue;
                }

                if ((sym==SDLK_e||sym==SDLK_SPACE) && !event.key.repeat) {
                    e_held=true; e_press_ticks=SDL_GetTicks();
                }

                const bool is_move =
                    sym==SDLK_UP||sym==SDLK_DOWN||sym==SDLK_LEFT||sym==SDLK_RIGHT||
                    sym==SDLK_w||sym==SDLK_s||sym==SDLK_a||sym==SDLK_d||
                    sym==SDLK_KP_7||sym==SDLK_KP_8||sym==SDLK_KP_9||
                    sym==SDLK_KP_1||sym==SDLK_KP_2||sym==SDLK_KP_3||
                    sym==SDLK_KP_4||sym==SDLK_KP_6;

                // First press only — OS key-repeat events are handled by the held block below
                if (is_move && !event.key.repeat && gs == GameState::EXPLORING) {
                    int mx=0, my=0;
                    const Uint8* ks = SDL_GetKeyboardState(nullptr);
                    if (ks[SDL_SCANCODE_W]||ks[SDL_SCANCODE_UP]   ||ks[SDL_SCANCODE_KP_8]) my--;
                    if (ks[SDL_SCANCODE_S]||ks[SDL_SCANCODE_DOWN] ||ks[SDL_SCANCODE_KP_2]) my++;
                    if (ks[SDL_SCANCODE_A]||ks[SDL_SCANCODE_LEFT] ||ks[SDL_SCANCODE_KP_4]) mx--;
                    if (ks[SDL_SCANCODE_D]||ks[SDL_SCANCODE_RIGHT]||ks[SDL_SCANCODE_KP_6]) mx++;
                    if (ks[SDL_SCANCODE_KP_7]){mx=-1;my=-1;}
                    if (ks[SDL_SCANCODE_KP_9]){mx= 1;my=-1;}
                    if (ks[SDL_SCANCODE_KP_1]){mx=-1;my= 1;}
                    if (ks[SDL_SCANCODE_KP_3]){mx= 1;my= 1;}
                    if (mx!=0||my!=0) {
                        const int nx=player.tile_x()+mx, ny=player.tile_y()+my;
                        const Tile& tgt=map.get_tile(nx,ny);
                        if (tgt.tile_id==3&&tgt.door_state==DoorState::Closed)
                            map.set_door_state(nx,ny,DoorState::Open);
                        else {
                            player.try_move(mx,my,map);
                            cam.follow(player.tile_x(), player.tile_y());
                        }
                        map.update_doors();
                        move_last_tick = SDL_GetTicks();
                        tick_pending = true;
                    }
                }
            }
            if (event.type==SDL_KEYUP) {
                const SDL_Keycode sym=event.key.keysym.sym;
                if ((sym==SDLK_e||sym==SDLK_SPACE)&&e_held) {
                    if (gs == GameState::TEXT) {
                        // Advance or dismiss text queue
                        text_queue.advance();
                        if (!text_queue.is_active()) gs = GameState::EXPLORING;
                    } else if (gs == GameState::INVENTORY) {
                        // E key inside inventory is handled in KEYDOWN routing above.
                    } else {
                        const bool lock=(SDL_GetTicks()-e_press_ticks)>=k_lock_hold_ms;

                        // Build facing-first direction order.
                        int face_dx=0, face_dy=0;
                        switch (player.facing()) {
                            case Direction::NORTH: face_dy=-1; break;
                            case Direction::SOUTH: face_dy= 1; break;
                            case Direction::WEST:  face_dx=-1; break;
                            case Direction::EAST:  face_dx= 1; break;
                        }
                        static const int k_all_dirs[4][2] = {{0,-1},{0,1},{-1,0},{1,0}};
                        int ord[5][2]; int oi=0;
                        ord[oi][0]=face_dx; ord[oi][1]=face_dy; ++oi;
                        for (const auto& d : k_all_dirs)
                            if (d[0]!=face_dx || d[1]!=face_dy)
                                { ord[oi][0]=d[0]; ord[oi][1]=d[1]; ++oi; }
                        ord[oi][0]=0; ord[oi][1]=0; ++oi; // also check player's own tile

                        // Door: check facing-first.
                        bool door_found = false;
                        for (int di=0; di<4 && !door_found; ++di) {
                            const int tx=player.tile_x()+ord[di][0], ty=player.tile_y()+ord[di][1];
                            const Tile& t = map.get_tile(tx, ty);
                            if (t.tile_id != 3) continue;
                            if (lock) {
                                map.set_door_state(tx, ty,
                                    t.door_state==DoorState::Locked ? DoorState::Closed : DoorState::Locked);
                            } else {
                                if (t.door_state==DoorState::Closed) map.set_door_state(tx,ty,DoorState::Open);
                                else if (t.door_state==DoorState::Open) map.set_door_state(tx,ty,DoorState::Closed);
                            }
                            door_found = true;
                        }
                        if (door_found) {
                            map.update_doors();
                            tick_pending = true;
                        } else {
                            // Text / item pickup: check facing-first.
                            bool text_found      = false;
                            bool container_found = false;
                            bool terminal_found  = false;
                            for (int di=0; di<5 && !text_found; ++di) {
                                const int tx=player.tile_x()+ord[di][0], ty=player.tile_y()+ord[di][1];
                                const EntityInstance* ei = map.find_entity_at(tx, ty);
                                if (ei && ei->type_id == "item_pickup") {
                                    // ── Item pickup ──────────────────────────────────────────────
                                    const std::string pickup_id = ei->item_id;
                                    const ItemDef* idef = find_item_def(item_defs, pickup_id);
                                    inventory.add(pickup_id, 1);
                                    map.remove_entity(ei->uid);
                                    TextEntry te;
                                    te.text             = "Picked up: " + (idef ? idef->name : pickup_id);
                                    te.voice_profile_id = "station_ambient"; te.dismiss = "auto";
                                    text_queue.push(te);
                                    if (idef && !idef->pickup_text.empty()) {
                                        TextEntry te2;
                                        te2.text             = idef->pickup_text;
                                        te2.voice_profile_id = idef->voice_profile.empty() ? "station_ambient" : idef->voice_profile;
                                        te2.dismiss          = "auto";
                                        text_queue.push(te2);
                                    }
                                    text_found = true; tick_pending = true;
                                } else if (ei) {
                                    // ── Terminal / container / text entity ───────────────────────
                                    const EntityDef* ei_def = find_entity_def(entity_defs, ei->type_id);
                                    if (ei->type_id == "terminal_dayshift") {
                                        terminal_panel.open();
                                        gs = GameState::TERMINAL_DISPLAY;
                                        terminal_found = true;
                                        text_found     = true;
                                    } else if (ei_def && ei_def->category == "container") {
                                        EntityInstance* mut_ei = map.find_entity_by_uid(ei->uid);
                                        if (mut_ei && !mut_ei->container_locked) {
                                            container_ui.open(*mut_ei);
                                            gs = GameState::CONTAINER;
                                            container_found = true;
                                        } else {
                                            TextEntry te;
                                            te.text = (ei->label.empty() ? "Container" : ei->label) + " is locked.";
                                            te.voice_profile_id = "station_ambient"; te.dismiss = "auto";
                                            text_queue.push(te);
                                        }
                                        text_found = true;
                                    } else if (!ei->text_desc.empty() || !ei->text_narrator.empty()) {
                                        TextEntry te;
                                        te.text             = ei->text_desc;
                                        te.narrator         = ei->text_narrator;
                                        te.voice_profile_id = ei->text_voice.empty() ? "station_ambient" : ei->text_voice;
                                        te.dismiss          = ei->text_dismiss.empty() ? "input" : ei->text_dismiss;
                                        text_queue.push(te);
                                        if (!ei->text_readable.empty()) {
                                            TextEntry te2;
                                            te2.text             = ei->text_readable;
                                            te2.voice_profile_id = te.voice_profile_id;
                                            te2.dismiss          = "input";
                                            text_queue.push(te2);
                                        }
                                        text_found = true;
                                    }
                                }
                                // ── Tile text (always checked if nothing found yet) ───────────────
                                if (!text_found) {
                                    const Tile& t = map.get_tile(tx, ty);
                                    if (!t.text_desc.empty() || !t.text_narrator.empty()) {
                                        TextEntry te;
                                        te.text             = t.text_desc;
                                        te.narrator         = t.text_narrator;
                                        te.voice_profile_id = t.text_voice.empty() ? "station_ambient" : t.text_voice;
                                        te.dismiss          = t.text_dismiss.empty() ? "input" : t.text_dismiss;
                                        text_queue.push(te);
                                        if (!t.text_readable.empty()) {
                                            TextEntry te2;
                                            te2.text             = t.text_readable;
                                            te2.voice_profile_id = te.voice_profile_id;
                                            te2.dismiss          = "input";
                                            text_queue.push(te2);
                                        }
                                        text_found = true;
                                    }
                                }
                            }
                            if (text_found && !container_found && !terminal_found) {
                                gs = GameState::TEXT;
                                tick_pending = true;
                            }
                        }
                    }
                    e_held=false;
                }
            }
        }

        // ── Held movement at fixed interval ──────────────────────────────────
        if (gs == GameState::EXPLORING) {
            const Uint32 now = SDL_GetTicks();
            if (now - move_last_tick >= k_move_interval_ms) {
                const Uint8* ks = SDL_GetKeyboardState(nullptr);
                int mx=0, my=0;
                if (ks[SDL_SCANCODE_W]||ks[SDL_SCANCODE_UP]   ||ks[SDL_SCANCODE_KP_8]) my--;
                if (ks[SDL_SCANCODE_S]||ks[SDL_SCANCODE_DOWN] ||ks[SDL_SCANCODE_KP_2]) my++;
                if (ks[SDL_SCANCODE_A]||ks[SDL_SCANCODE_LEFT] ||ks[SDL_SCANCODE_KP_4]) mx--;
                if (ks[SDL_SCANCODE_D]||ks[SDL_SCANCODE_RIGHT]||ks[SDL_SCANCODE_KP_6]) mx++;
                if (ks[SDL_SCANCODE_KP_7]){mx=-1;my=-1;}
                if (ks[SDL_SCANCODE_KP_9]){mx= 1;my=-1;}
                if (ks[SDL_SCANCODE_KP_1]){mx=-1;my= 1;}
                if (ks[SDL_SCANCODE_KP_3]){mx= 1;my= 1;}
                if (mx!=0||my!=0) {
                    const int nx=player.tile_x()+mx, ny=player.tile_y()+my;
                    const Tile& tgt=map.get_tile(nx,ny);
                    if (tgt.tile_id==3&&tgt.door_state==DoorState::Closed)
                        map.set_door_state(nx,ny,DoorState::Open);
                    else {
                        player.try_move(mx,my,map);
                        cam.follow(player.tile_x(), player.tile_y());
                    }
                    map.update_doors();
                    move_last_tick = now;
                    tick_pending = true;
                }
            }
        }

        if (tick_pending && gs == GameState::EXPLORING) {
            fog.update(player.tile_x(), player.tile_y(), player.sight_radius(), map);
            tick_pending = false;
        }
        {
            const Uint32 now = SDL_GetTicks();
            const float dt = std::min(0.1f, (now - prev_tick) / 1000.0f);
            prev_tick = now;
            cam.update(dt);
            player.update_visual(dt, cam.follow_speed());
            cam.snap_to_tile_f(player.vis_tx(), player.vis_ty());
            text_queue.tick(dt);
            sim_time_minutes += dt * 10.0f; // 1 real sec = 10 in-game minutes
            // Auto-dismiss can return queue to empty and restore EXPLORING
            if (gs == GameState::TEXT && !text_queue.is_active())
                gs = GameState::EXPLORING;
        }

        do_render();
    }

    settings.cam_default_zoom = cam.zoom();
    return from_editor && !window_close && back_to_editor;
}

// ── Editor loop ───────────────────────────────────────────────────────────────

static std::pair<int,int> run_editor(Renderer& renderer, Map& map,
                                     const std::string& map_path,
                                     FogOfWar& fog, Camera& cam,
                                     const std::vector<TileDef>& tile_defs,
                                     const std::vector<EntityDef>& entity_defs,
                                     const std::vector<TextureAtlas>& atlases,
                                     PauseMenu& pause_menu,
                                     SettingsPanel& settings_panel,
                                     AppSettings& settings,
                                     const std::vector<VoiceProfile>* voice_profiles = nullptr) {
    fog.reveal_all();

    Editor editor;
    editor.init(map_path, &tile_defs, &entity_defs);
    editor.set_atlases(&atlases);
    if (voice_profiles) editor.set_voice_profiles(voice_profiles);
    editor.update_viewport(cam, renderer.screen_w(), renderer.screen_h());
    cam.follow(map.get_width()/2, map.get_height()/2);

    const std::string map_file = map_path.substr(map_path.find_last_of("/\\")+1);
    bool running=true, prev_dirty=false;
    SDL_Event event;
    Editor::Action last_action = Editor::Action::Continue;

    while (running) {
        const bool cur_dirty = editor.is_dirty();
        if (cur_dirty != prev_dirty) {
            const std::string t = std::string("Outpost 13-C \xe2\x80\x94 EDITOR MODE \xe2\x80\x94 ")
                + map_file + (cur_dirty ? " *" : "");
            renderer.set_title(t);
            prev_dirty = cur_dirty;
        }

        // ── Pause menu active ─────────────────────────────────────────────────
        if (pause_menu.is_open() || settings_panel.is_open()) {
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) { running=false; break; }
                if (event.type == SDL_WINDOWEVENT &&
                    event.window.event == SDL_WINDOWEVENT_RESIZED) {
                    editor.update_viewport(cam, event.window.data1, event.window.data2); continue;
                }
                if (settings_panel.is_open()) {
                    auto sr = settings_panel.handle_event(event, renderer);
                    if (sr == SettingsPanel::Result::Apply) { settings=settings_panel.working(); settings_panel.close(); }
                    if (sr == SettingsPanel::Result::Close) { settings_panel.close(); }
                    continue;
                }
                PauseResult pr = pause_menu.handle_event(event, renderer);
                switch (pr) {
                    case PauseResult::Settings:   settings_panel.open(settings); break;
                    case PauseResult::QuitDesktop:running=false; last_action=Editor::Action::Quit; break;
                    case PauseResult::SaveAndQuit: map.save(map_path); running=false; last_action=Editor::Action::Quit; break;
                    default: break;
                }
            }
            goto editor_render;
        }

        // ── Normal editor events ──────────────────────────────────────────────
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_WINDOWEVENT &&
                event.window.event == SDL_WINDOWEVENT_RESIZED) {
                editor.update_viewport(cam, event.window.data1, event.window.data2); continue;
            }
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F11) {
                renderer.toggle_fullscreen();
                editor.update_viewport(cam, renderer.screen_w(), renderer.screen_h()); continue;
            }
            last_action = editor.handle_event(event, map, cam, renderer);
            if (last_action == Editor::Action::Pause) {
                pause_menu.open(MenuMode::EDITOR, editor.is_dirty());
                last_action = Editor::Action::Continue;
                break;
            }
            if (last_action != Editor::Action::Continue) { running=false; break; }
        }

        editor.update(cam, map);

        editor_render:
        renderer.clear(18,28,18);
        renderer.draw_map(map, fog, cam, true);
        editor.render(renderer, map, cam);
        if (pause_menu.is_open())    pause_menu.render(renderer);
        if (settings_panel.is_open())settings_panel.render(renderer);
        renderer.present();
    }

    if (last_action == Editor::Action::Play)
        return {map.get_spawn_x(), map.get_spawn_y()};
    return {-1, -1};
}

// ── Main menu loop ────────────────────────────────────────────────────────────

static MainMenuResult run_main_menu(Renderer& renderer, MainMenu& menu,
                                    SettingsPanel& settings_panel,
                                    AppSettings& settings) {
    bool running = true;
    MainMenuResult result = MainMenuResult::None;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) { running=false; result=MainMenuResult::Quit; break; }
            if (event.type == SDL_WINDOWEVENT &&
                event.window.event == SDL_WINDOWEVENT_RESIZED) continue;
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F11)
                { renderer.toggle_fullscreen(); continue; }

            auto r = menu.handle_event(event, renderer, settings_panel);
            if (r == MainMenuResult::OpenSettings) {
                settings_panel.open(settings); continue;
            }
            if (r != MainMenuResult::None) {
                result = r; running = false; break;
            }
            // Settings panel close/apply handled inside handle_event
            if (!settings_panel.is_open() && r == MainMenuResult::None) {
                // Check if settings was just closed via Apply
                // (handled internally — settings_panel.is_open() goes false)
            }
        }

        renderer.clear(8,10,14);
        menu.render(renderer, settings_panel);
        renderer.present();
    }

    if (settings_panel.is_open()) {
        settings = settings_panel.working();
        settings_panel.close();
    }
    return result;
}

// ── Entry point ───────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    bool dev_mode = false;
    for (int i = 1; i < argc; ++i)
        if (std::string(argv[i]) == "--dev") dev_mode = true;

    const std::string map_path   = "Data/levels/test.json";
    const std::string defs_path  = "Data/tiles/tile_definitions.json";
    const std::string ents_path  = "Data/entities/entity_definitions.json";
    const std::string cfg_path   = "Data/settings.json";
    const std::string notes_path = "Data/playtest_notes.json";

    AppSettings settings = AppSettings::load(cfg_path);

    std::vector<TileDef>      tile_defs;     load_tile_defs(defs_path, tile_defs);
    std::vector<EntityDef>    entity_defs;   load_entity_defs(ents_path, entity_defs);
    std::vector<TextureAtlas> atlases;       load_atlases("Data/atlases", atlases);
    std::vector<VoiceProfile> voice_profiles;
    load_voice_profiles("Data/voices/voice_profiles.json", voice_profiles);
    std::vector<ItemCategory> item_categories;
    load_item_categories("Data/item_categories.json", item_categories);
    std::vector<ItemDef> item_defs;
    load_items("Data/items", item_defs);
    std::vector<LootTable> loot_tables;
    load_loot_tables("Data/loot_tables", loot_tables);

    Map map;
    map.set_tile_defs(&tile_defs);
    if (!map.load(map_path)) return 1;

    FogOfWar fog;
    fog.init(map.get_width(), map.get_height());
    Camera cam;

    Renderer renderer;
    if (!renderer.init("Outpost 13-C", settings.res_w, settings.res_h)) return 1;
    renderer.set_tile_defs(&tile_defs);
    renderer.set_atlases(&atlases);
    renderer.set_voice_profiles(&voice_profiles);
    if (settings.fullscreen) renderer.toggle_fullscreen();

    // Optional audio — silently disabled if SDL_mixer unavailable or init fails
#if HAS_SDL_MIXER
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 512) < 0)
        std::cerr << "Mix_OpenAudio: " << Mix_GetError() << " (audio disabled)\n";
#endif

    PauseMenu      pause_menu;
    SettingsPanel  settings_panel;
    PlaytestNotes  notes;
    SaveSystem     save_system;

    notes.load(notes_path);

    auto editor_game_loop = [&]() {
        while (true) {
            auto [sx, sy] = run_editor(renderer, map, map_path, fog, cam,
                                       tile_defs, entity_defs, atlases,
                                       pause_menu, settings_panel, settings,
                                       &voice_profiles);
            if (sx < 0) break;
            map.load(map_path);
            bool back = run_game(renderer, map, map_path, fog, cam, sx, sy, true,
                                 pause_menu, settings_panel, notes, save_system, settings,
                                 tile_defs, entity_defs, voice_profiles, item_defs, item_categories,
                                 loot_tables);
            if (!back) break;
            map.load(map_path);
            fog.init(map.get_width(), map.get_height());
        }
    };

    if (dev_mode) {
        renderer.set_title("Outpost 13-C \xe2\x80\x94 EDITOR MODE");
        editor_game_loop();
    } else {
        MainMenu main_menu;
        save_system.scan(SaveSystem::saves_dir());
        main_menu.rescan_saves(SaveSystem::saves_dir());

        bool app_running = true;
        while (app_running) {
            MainMenuResult result = run_main_menu(renderer, main_menu,
                                                  settings_panel, settings);
            switch (result) {
                case MainMenuResult::Quit:
                    app_running = false;
                    break;

                case MainMenuResult::OpenEditor:
                    editor_game_loop();
                    main_menu.rescan_saves(SaveSystem::saves_dir());
                    break;

                case MainMenuResult::NewGame: {
                    map.load(map_path);
                    fog.init(map.get_width(), map.get_height());
                    const int sx = map.get_spawn_x(), sy = map.get_spawn_y();
                    const std::string ng_name = main_menu.new_game_player_name();
                    run_game(renderer, map, map_path, fog, cam, sx, sy, false,
                             pause_menu, settings_panel, notes, save_system, settings,
                             tile_defs, entity_defs, voice_profiles, item_defs, item_categories,
                             loot_tables, "south", PlayerInventory{},
                             ng_name.empty() ? "SMITH" : ng_name);
                    main_menu.rescan_saves(SaveSystem::saves_dir());
                    break;
                }

                case MainMenuResult::LoadGame: {
                    std::string lmap; int lx=1, ly=1; std::string lname;
                    std::string lface; PlayerInventory linv;
                    std::string lpname; PlayerStats lstats; float lsim=0.f;
                    if (save_system.load_game(main_menu.selected_save_path(),
                                              lmap, lx, ly, lname, &lface, &linv,
                                              &lpname, &lstats, &lsim)) {
                        const std::string load_map = lmap.empty() ? map_path : lmap;
                        map.load(load_map);
                        fog.init(map.get_width(), map.get_height());
                        run_game(renderer, map, load_map, fog, cam, lx, ly, false,
                                 pause_menu, settings_panel, notes, save_system, settings,
                                 tile_defs, entity_defs, voice_profiles, item_defs, item_categories,
                                 loot_tables, lface, std::move(linv), lpname, lstats, lsim);
                    }
                    main_menu.rescan_saves(SaveSystem::saves_dir());
                    break;
                }

                default: break;
            }
        }
    }

    settings.save(cfg_path);
    notes.save(); // ensure latest notes persisted
    return 0;
}
