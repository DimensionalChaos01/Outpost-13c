#include "SaveSystem.h"
#include "PlayerInventory.h"
#include "FileUtils.h"
#include "json.hpp"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <filesystem>
#include <ctime>
#include <cstdio>

namespace fs = std::filesystem;
using json   = nlohmann::json;

// ── Helpers ───────────────────────────────────────────────────────────────────

std::string SaveSystem::timestamp_now() {
    std::time_t t = std::time(nullptr);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", std::localtime(&t));
    return buf;
}

// ── Save ──────────────────────────────────────────────────────────────────────

bool SaveSystem::save_game(const std::string& save_name,
                            const std::string& map_path,
                            int player_x, int player_y, int play_time_s,
                            bool is_autosave,
                            const std::string& player_facing,
                            const PlayerInventory* inventory,
                            const std::string& player_name,
                            const PlayerStats* stats,
                            float sim_time_minutes) {
    std::error_code ec;
    fs::create_directories(saves_dir(), ec);

    const std::string path = is_autosave ? autosave_path()
                           : saves_dir() + save_name + ".json";

    json data;
    data["save_name"]         = save_name;
    data["timestamp"]         = timestamp_now();
    data["play_time_seconds"] = play_time_s;
    data["current_map"]       = map_path;
    data["player_position"]   = {{"x", player_x}, {"y", player_y}};
    data["player_facing"]     = player_facing;
    data["player_name"]       = player_name;

    json inv_array = json::array();
    if (inventory) {
        for (const auto& entry : inventory->get_all())
            inv_array.push_back({{"id", entry.item_id}, {"qty", entry.qty}});
    }
    data["inventory"] = inv_array;

    if (stats) {
        data["player_stats"] = {
            {"hp_current",     stats->hp_current},
            {"hp_max",         stats->hp_max},
            {"sanity_current", stats->sanity_current},
            {"sanity_max",     stats->sanity_max},
            {"value",          stats->value},
            {"strength",       stats->strength},
            {"agility",        stats->agility},
            {"defense",        stats->defense},
            {"perception",     stats->perception},
            {"endurance",      stats->endurance},
            {"fortitude",      stats->fortitude},
            {"discipline",     stats->discipline}
        };
    } else {
        data["player_stats"] = json::object();
    }

    const int st       = (int)sim_time_minutes;
    const int sim_hour = (st / 60) % 24;
    data["simulated_time"] = st;
    data["day_counter"]    = st / 1440 + 1;
    data["current_shift"]  = (sim_hour >= 6 && sim_hour < 18) ? "day" : "night";
    data["flags"]          = json::object();
    data["morale"]         = 75;

    std::ofstream f(path);
    if (!f.is_open()) { std::cerr << "SaveSystem: cannot write " << path << "\n"; return false; }
    f << data.dump(2) << "\n";
    std::cout << "SaveSystem: saved to " << path << "\n";
    return true;
}

// ── Load ──────────────────────────────────────────────────────────────────────

bool SaveSystem::load_game(const std::string& path,
                            std::string& out_map_path,
                            int& out_x, int& out_y,
                            std::string& out_save_name,
                            std::string*     out_facing,
                            PlayerInventory* out_inventory,
                            std::string*     out_player_name,
                            PlayerStats*     out_stats,
                            float*           out_sim_time) {
    std::ifstream f(path);
    if (!f.is_open()) { std::cerr << "SaveSystem: cannot open " << path << "\n"; return false; }
    json data;
    try { f >> data; } catch (...) { return false; }

    out_map_path  = data.value("current_map", "Data/levels/test.json");
    out_save_name = data.value("save_name",   "");
    const auto& pp = data.value("player_position", json::object());
    out_x = pp.value("x", 1);
    out_y = pp.value("y", 1);
    if (out_facing)      *out_facing      = data.value("player_facing", "south");
    if (out_player_name) *out_player_name = data.value("player_name",   "SMITH");
    if (out_sim_time)    *out_sim_time    = (float)data.value("simulated_time", 0);

    if (out_inventory) {
        out_inventory->clear();
        if (data.contains("inventory") && data["inventory"].is_array()) {
            for (const auto& e : data["inventory"]) {
                const std::string iid = e.value("id",  "");
                const int         qty = e.value("qty", 0);
                if (!iid.empty() && qty > 0) out_inventory->add(iid, qty);
            }
        }
    }

    if (out_stats) {
        PlayerStats s;
        if (data.contains("player_stats") && data["player_stats"].is_object()) {
            const auto& ps  = data["player_stats"];
            s.hp_current     = ps.value("hp_current",     6);
            s.hp_max         = ps.value("hp_max",         6);
            s.sanity_current = ps.value("sanity_current", 100);
            s.sanity_max     = ps.value("sanity_max",     100);
            s.value          = ps.value("value",          100);
            s.strength       = ps.value("strength",       1);
            s.agility        = ps.value("agility",        1);
            s.defense        = ps.value("defense",        1);
            s.perception     = ps.value("perception",     1);
            s.endurance      = ps.value("endurance",      1);
            s.fortitude      = ps.value("fortitude",      1);
            s.discipline     = ps.value("discipline",     1);
        }
        *out_stats = s;
    }

    std::cout << "SaveSystem: loaded " << out_save_name << " from " << path << "\n";
    return true;
}

// ── Scan ──────────────────────────────────────────────────────────────────────

void SaveSystem::scan(const std::string& dir) {
    slots_.clear();
    std::error_code ec;
    if (!fs::exists(dir, ec)) return;

    const std::string ap = autosave_path();
    if (file_exists(ap)) {
        std::ifstream f(ap);
        json data; try { f >> data; } catch (...) {}
        SaveSlot s;
        s.path        = ap;
        s.save_name   = "Autosave";
        s.timestamp   = data.value("timestamp", "");
        s.map_name    = data.value("current_map", "");
        s.play_time_s = data.value("play_time_seconds", 0);
        slots_.push_back(s);
    }

    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (entry.path().extension() != ".json") continue;
        if (entry.path().filename() == "autosave.json") continue;
        std::ifstream f(entry.path());
        json data; try { f >> data; } catch (...) { continue; }
        SaveSlot s;
        s.path        = entry.path().string();
        s.save_name   = data.value("save_name", entry.path().stem().string());
        s.timestamp   = data.value("timestamp", "");
        s.map_name    = data.value("current_map", "");
        s.play_time_s = data.value("play_time_seconds", 0);
        slots_.push_back(s);
    }
}

void SaveSystem::delete_save(const std::string& path) {
    std::error_code ec;
    fs::remove(path, ec);
    slots_.erase(std::remove_if(slots_.begin(), slots_.end(),
                                [&path](const SaveSlot& s){ return s.path == path; }),
                 slots_.end());
}
