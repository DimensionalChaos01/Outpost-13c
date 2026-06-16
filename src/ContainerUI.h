#pragma once
#include <string>
#include <vector>
#include <SDL2/SDL.h>

class Renderer;
class PlayerInventory;
class TextQueue;
struct EntityInstance;
struct ItemDef;

class ContainerUI {
public:
    enum class Action { None, Close };

    void open(EntityInstance& entity);

    Action handle_event(const SDL_Event& e,
                        PlayerInventory& inv,
                        const std::vector<ItemDef>& defs,
                        TextQueue& tq);

    void render(Renderer& rnd,
                const EntityInstance& entity,
                const std::vector<ItemDef>& defs) const;

    uint32_t entity_uid() const { return entity_uid_; }
    int      selected()   const { return selected_; }
    void     clamp(int count);

private:
    uint32_t entity_uid_ = 0;
    int      selected_   = 0;
    int      scroll_     = 0;
    bool     just_emptied_ = false;

    static constexpr int k_row_h   = 22;
    static constexpr int k_vis_rows = 8;
};
