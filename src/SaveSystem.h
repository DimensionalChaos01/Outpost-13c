#pragma once
#include <string>
#include <vector>
#include "Player.h"

class PlayerInventory;

struct SaveSlot {
    std::string path;
    std::string save_name;
    std::string timestamp;
    std::string map_name;
    int         play_time_s = 0;
};

class SaveSystem {
public:
    bool save_game(const std::string& save_name, const std::string& map_path,
                   int player_x, int player_y, int play_time_s,
                   bool is_autosave = false,
                   const std::string& player_facing = "south",
                   const PlayerInventory* inventory = nullptr,
                   const std::string& player_name = "SMITH",
                   const PlayerStats* stats = nullptr,
                   float sim_time_minutes = 0.0f);

    bool load_game(const std::string& path,
                   std::string& out_map_path,
                   int& out_x, int& out_y,
                   std::string& out_save_name,
                   std::string*      out_facing      = nullptr,
                   PlayerInventory*  out_inventory   = nullptr,
                   std::string*      out_player_name = nullptr,
                   PlayerStats*      out_stats       = nullptr,
                   float*            out_sim_time    = nullptr);

    void scan(const std::string& dir);
    void delete_save(const std::string& path);

    const std::vector<SaveSlot>& slots() const { return slots_; }

    static std::string saves_dir()    { return "Data/saves/"; }
    static std::string autosave_path(){ return "Data/saves/autosave.json"; }

private:
    std::vector<SaveSlot> slots_;
    static std::string timestamp_now();
};
