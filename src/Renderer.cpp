#include "Renderer.h"
#include "Map.h"
#include "FogOfWar.h"
#include "Camera.h"
#include "Player.h"
#include "TileDef.h"
#include "MenuBar.h"
#include <SDL2/SDL_image.h>
#include <iostream>
#include <cstdio>
#include <cstring>
#include <algorithm>

static constexpr float k_seen_factor = 0.35f;

struct TileColor { Uint8 r, g, b; };
static constexpr TileColor k_tile_colors[] = {
    {  0,   0,   0},
    { 40,  40,  50},
    { 78,  78,  90},
    {108,  72,  35},
};
static constexpr int k_color_count =
    static_cast<int>(sizeof(k_tile_colors)/sizeof(k_tile_colors[0]));

// ── Lifecycle ─────────────────────────────────────────────────────────────────

Renderer::~Renderer() { shutdown(); }

bool Renderer::init(const char* title, int width, int height) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL_Init: " << SDL_GetError() << "\n"; return false;
    }
    if (TTF_Init() < 0)
        std::cerr << "TTF_Init: " << TTF_GetError() << " (text disabled)\n";
    if (IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG) == 0)
        std::cerr << "IMG_Init: " << IMG_GetError() << " (textures disabled)\n";

    window_ = SDL_CreateWindow(title,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!window_) { std::cerr << "SDL_CreateWindow: " << SDL_GetError() << "\n"; return false; }

    renderer_ = SDL_CreateRenderer(window_, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer_) { std::cerr << "SDL_CreateRenderer: " << SDL_GetError() << "\n"; return false; }

    // No SDL_RenderSetLogicalSize — window coords equal render coords directly.
    try_load_font(12);
    return true;
}

bool Renderer::try_load_font(int pt_size) {
    static const char* paths[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
        "/usr/share/fonts/truetype/ubuntu/UbuntuMono-R.ttf",
        "/usr/share/fonts/truetype/freefont/FreeMono.ttf",
        "/usr/share/fonts/truetype/noto/NotoMono-Regular.ttf",
        nullptr
    };
    for (int i = 0; paths[i]; ++i) {
        font_ = TTF_OpenFont(paths[i], pt_size);
        if (font_) return true;
    }
    std::cerr << "Renderer: no TTF font found — text disabled\n";
    return false;
}

void Renderer::shutdown() {
    for (auto& [path, tex] : texture_cache_)
        if (tex) SDL_DestroyTexture(tex);
    texture_cache_.clear();
    if (font_)     { TTF_CloseFont(font_);           font_     = nullptr; }
    if (renderer_) { SDL_DestroyRenderer(renderer_);  renderer_ = nullptr; }
    if (window_)   { SDL_DestroyWindow(window_);      window_   = nullptr; }
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();
}

SDL_Texture* Renderer::load_or_get_texture(const std::string& path) {
    auto it = texture_cache_.find(path);
    if (it != texture_cache_.end()) return it->second;
    SDL_Texture* tex = nullptr;
    SDL_Surface* surf = IMG_Load(path.c_str());
    if (surf) {
        tex = SDL_CreateTextureFromSurface(renderer_, surf);
        SDL_FreeSurface(surf);
    }
    if (!tex) std::cerr << "Renderer: cannot load texture: " << path << "\n";
    texture_cache_[path] = tex;
    return tex;
}

void Renderer::draw_atlas_cell(const TextureAtlas& atlas, int cell_idx,
                                int x, int y, int w, int h, Uint8 alpha) {
    SDL_Texture* tex = load_or_get_texture(atlas.image_path);
    if (!tex) {
        // Fallback: tinted placeholder
        fill_rect(x, y, w, h, 60, 80, 100, alpha);
        return;
    }
    int cx, cy, cw, ch;
    atlas.cell_rect(cell_idx, cx, cy, cw, ch);
    SDL_Rect src{cx, cy, cw, ch};
    SDL_Rect dst{x,  y,  w,  h};
    SDL_SetTextureAlphaMod(tex, alpha);
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    SDL_RenderCopy(renderer_, tex, &src, &dst);
    SDL_SetTextureAlphaMod(tex, 255);
}

// ── Core ──────────────────────────────────────────────────────────────────────

void Renderer::clear(Uint8 r, Uint8 g, Uint8 b) {
    SDL_SetRenderDrawColor(renderer_, r, g, b, 255);
    SDL_RenderClear(renderer_);
}
void Renderer::present() { SDL_RenderPresent(renderer_); }
void Renderer::toggle_fullscreen() {
    const Uint32 f = SDL_GetWindowFlags(window_);
    SDL_SetWindowFullscreen(window_,
        (f & SDL_WINDOW_FULLSCREEN_DESKTOP) ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
}
int Renderer::screen_w() const { int w=0,h=0; SDL_GetWindowSize(window_,&w,&h); return w; }
int Renderer::screen_h() const { int w=0,h=0; SDL_GetWindowSize(window_,&w,&h); return h; }
void Renderer::to_logical(int wx, int wy, int& lx, int& ly) const { lx=wx; ly=wy; }
void Renderer::set_title(const std::string& t) { SDL_SetWindowTitle(window_, t.c_str()); }

// ── Public primitives ─────────────────────────────────────────────────────────

void Renderer::fill_rect(int x, int y, int w, int h,
                         Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
    if (a < 255) SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer_, r, g, b, a);
    SDL_Rect rect{x, y, w, h};
    SDL_RenderFillRect(renderer_, &rect);
    if (a < 255) SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_NONE);
}
void Renderer::draw_rect_outline(int x, int y, int w, int h,
                                 Uint8 r, Uint8 g, Uint8 b) {
    SDL_SetRenderDrawColor(renderer_, r, g, b, 255);
    SDL_Rect rect{x, y, w, h};
    SDL_RenderDrawRect(renderer_, &rect);
}
void Renderer::draw_line(int x1, int y1, int x2, int y2,
                         Uint8 r, Uint8 g, Uint8 b) {
    SDL_SetRenderDrawColor(renderer_, r, g, b, 255);
    SDL_RenderDrawLine(renderer_, x1, y1, x2, y2);
}
void Renderer::draw_text(const std::string& text, int x, int y,
                         Uint8 r, Uint8 g, Uint8 b) {
    if (!font_ || text.empty()) return;
    SDL_Color col{r, g, b, 255};
    SDL_Surface* s = TTF_RenderText_Solid(font_, text.c_str(), col);
    if (!s) return;
    SDL_Texture* t = SDL_CreateTextureFromSurface(renderer_, s);
    SDL_FreeSurface(s);
    if (!t) return;
    int tw, th; SDL_QueryTexture(t, nullptr, nullptr, &tw, &th);
    SDL_Rect dst{x, y, tw, th};
    SDL_RenderCopy(renderer_, t, nullptr, &dst);
    SDL_DestroyTexture(t);
}
int Renderer::text_width(const std::string& text) const {
    if (!font_ || text.empty()) return (int)text.size() * 7;
    int w = 0, h = 0;
    TTF_SizeText(font_, text.c_str(), &w, &h);
    return w;
}

// ── Map / world rendering ─────────────────────────────────────────────────────

void Renderer::draw_map(const Map& map, const FogOfWar& fog, const Camera& cam,
                         bool editor_mode) {
    const int ox=cam.offset_x(), oy=cam.offset_y(), ts=cam.tile_px();
    const int ml=cam.map_left(), mt=cam.map_top();
    const int sw=cam.screen_w(), sh=cam.screen_h();

    SDL_Rect clip{ml, mt, sw, sh};
    SDL_RenderSetClipRect(renderer_, &clip);

    const int stx=std::max(0,(ox)/ts-1),  etx=(ox+sw)/ts+1;
    const int sty=std::max(0,(oy)/ts-1),  ety=(oy+sh)/ts+1;

    for (int ty=sty; ty<=ety; ++ty) {
        for (int tx=stx; tx<=etx; ++tx) {
            const FogState fs = fog.get(tx, ty);
            if (fs == FogState::Unseen) continue;

            const Tile& tile = map.get_tile(tx, ty);
            int id = tile.tile_id; if (id < 0) id = 0;

            TileColor c;
            if (tile_defs_) {
                const TileDef& def = get_tile_def(*tile_defs_, id);
                c = {def.r, def.g, def.b};
            } else {
                c = (id < k_color_count) ? k_tile_colors[id] : k_tile_colors[0];
            }
            if (id == 3) {
                switch (tile.door_state) {
                    case DoorState::Open:   c = {145,115, 70}; break;
                    case DoorState::Closed: c = {108, 72, 35}; break;
                    case DoorState::Locked: c = { 80, 28, 28}; break;
                }
            }
            const float f = (fs == FogState::Seen) ? k_seen_factor : 1.0f;
            const int sx = cam.to_screen_x(tx), sy = cam.to_screen_y(ty);
            // Seamless tiles: full ts×ts. Grid overlay is drawn separately on top.
            SDL_Rect rect{sx, sy, ts, ts};
            SDL_SetRenderDrawColor(renderer_,
                (Uint8)(c.r*f), (Uint8)(c.g*f), (Uint8)(c.b*f), 255);
            SDL_RenderFillRect(renderer_, &rect);

            // ── Texture layer ────────────────────────────────────────────────
            // For doors, pick texture based on door state; else use tile.texture_id
            const std::string* tex_ptr = &tile.texture_id;
            if (tile.tile_id == 3) {
                switch (tile.door_state) {
                    case DoorState::Open:   tex_ptr = &tile.texture_open;   break;
                    case DoorState::Closed: tex_ptr = &tile.texture_closed; break;
                    case DoorState::Locked: tex_ptr = &tile.texture_locked; break;
                }
                if (tex_ptr->empty()) tex_ptr = &tile.texture_id; // fallback
            }
            if (!tex_ptr->empty() && atlases_) {
                const TextureAtlas* atlas = nullptr;
                const AtlasEntry*   entry = nullptr;
                if (find_atlas_entry(*atlases_, *tex_ptr, atlas, entry)) {
                    const Uint8 tex_alpha = (Uint8)(editor_mode
                        ? (fs == FogState::Seen ? (int)(215*k_seen_factor) : 215)
                        : (fs == FogState::Seen ? (int)(255*k_seen_factor) : 255));
                    draw_atlas_cell(*atlas, entry->cell, sx, sy, ts, ts, tex_alpha);

                    // Mismatch indicator in editor (orange dot top-right corner)
                    if (editor_mode && !entry->compatible_types.empty()) {
                        bool compat = false;
                        for (int ct : entry->compatible_types)
                            if (ct == tile.tile_id) { compat = true; break; }
                        if (!compat) {
                            SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
                            SDL_Rect dot{sx+ts-8, sy+1, 5, 5};
                            SDL_SetRenderDrawColor(renderer_, 255, 90, 20, 220);
                            SDL_RenderFillRect(renderer_, &dot);
                            SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_NONE);
                        }
                    }
                }
            }
        }
    }
    SDL_RenderSetClipRect(renderer_, nullptr);
}

void Renderer::draw_player(const Player& player, const Camera& cam) {
    const int ts=cam.tile_px();
    const int size=std::max(4, ts*20/Camera::k_tile_size);
    const int margin=(ts-size)/2;
    SDL_Rect rect{cam.to_screen_x_f(player.vis_tx())+margin,
                  cam.to_screen_y_f(player.vis_ty())+margin, size, size};
    SDL_SetRenderDrawColor(renderer_,
        (Uint8)player.color_r(), (Uint8)player.color_g(), (Uint8)player.color_b(), 255);
    SDL_RenderFillRect(renderer_, &rect);

    // Facing indicator: small filled triangle on leading edge, brighter colour.
    const int depth = std::max(2, size / 4);
    const int cx    = rect.x + size / 2;
    const int cy    = rect.y + size / 2;
    const Uint8 br  = (Uint8)std::min(255, player.color_r() + 60);
    const Uint8 bg  = (Uint8)std::min(255, player.color_g() + 60);
    const Uint8 bb  = (Uint8)std::min(255, player.color_b() + 60);
    SDL_SetRenderDrawColor(renderer_, br, bg, bb, 255);
    switch (player.facing()) {
        case Direction::NORTH:
            for (int row = 0; row <= depth; ++row)
                SDL_RenderDrawLine(renderer_, cx-row, rect.y+1+row, cx+row, rect.y+1+row);
            break;
        case Direction::SOUTH:
            for (int row = 0; row <= depth; ++row)
                SDL_RenderDrawLine(renderer_, cx-row, rect.y+size-2-row, cx+row, rect.y+size-2-row);
            break;
        case Direction::WEST:
            for (int row = 0; row <= depth; ++row)
                SDL_RenderDrawLine(renderer_, rect.x+1+row, cy-row, rect.x+1+row, cy+row);
            break;
        case Direction::EAST:
            for (int row = 0; row <= depth; ++row)
                SDL_RenderDrawLine(renderer_, rect.x+size-2-row, cy-row, rect.x+size-2-row, cy+row);
            break;
    }
}

// ── Editor tile overlays ──────────────────────────────────────────────────────

void Renderer::draw_grid(const Camera& cam) {
    const int ox=cam.offset_x(), oy=cam.offset_y(), ts=cam.tile_px();
    const int ml=cam.map_left(), mt=cam.map_top(), sw=cam.screen_w(), sh=cam.screen_h();
    SDL_Rect clip{ml,mt,sw,sh}; SDL_RenderSetClipRect(renderer_, &clip);
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer_, 60, 65, 75, 60);
    for (int tx=ox/ts-1; tx<=(ox+sw)/ts+2; ++tx) {
        const int sx=tx*ts-ox+ml; SDL_RenderDrawLine(renderer_,sx,mt,sx,mt+sh);
    }
    for (int ty=oy/ts-1; ty<=(oy+sh)/ts+2; ++ty) {
        const int sy=ty*ts-oy+mt; SDL_RenderDrawLine(renderer_,ml,sy,ml+sw,sy);
    }
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_NONE);
    SDL_RenderSetClipRect(renderer_, nullptr);
}

void Renderer::draw_editor_cursor(int tx, int ty, const Camera& cam) {
    const int ts=cam.tile_px();
    SDL_Rect rect{cam.to_screen_x(tx), cam.to_screen_y(ty), ts, ts};
    SDL_Rect clip{cam.map_left(),cam.map_top(),cam.screen_w(),cam.screen_h()};
    SDL_RenderSetClipRect(renderer_, &clip);
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer_, 255,255,255, 45); SDL_RenderFillRect(renderer_, &rect);
    SDL_SetRenderDrawColor(renderer_, 255,255,255,210); SDL_RenderDrawRect(renderer_, &rect);
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_NONE);
    SDL_RenderSetClipRect(renderer_, nullptr);
}

void Renderer::draw_pathability_overlay(const Map& map, const Camera& cam) {
    const int ox=cam.offset_x(),oy=cam.offset_y(),ts=cam.tile_px();
    const int sw=cam.screen_w(),sh=cam.screen_h(),ml=cam.map_left(),mt=cam.map_top();
    SDL_Rect clip{ml,mt,sw,sh}; SDL_RenderSetClipRect(renderer_,&clip);
    SDL_SetRenderDrawBlendMode(renderer_,SDL_BLENDMODE_BLEND);
    for (int ty=std::max(0,oy/ts-1); ty<=(oy+sh)/ts+1; ++ty)
        for (int tx=std::max(0,ox/ts-1); tx<=(ox+sw)/ts+1; ++tx) {
            const Tile& t=map.get_tile(tx,ty); if(t.tile_id==0) continue;
            SDL_Rect r{cam.to_screen_x(tx),cam.to_screen_y(ty),ts-1,ts-1};
            SDL_SetRenderDrawColor(renderer_, t.pathable?0:220, t.pathable?220:0, 0, 70);
            SDL_RenderFillRect(renderer_,&r);
        }
    SDL_SetRenderDrawBlendMode(renderer_,SDL_BLENDMODE_NONE);
    SDL_RenderSetClipRect(renderer_,nullptr);
}

void Renderer::draw_room_tags_overlay(const Map& map, const Camera& cam) {
    const int ox=cam.offset_x(),oy=cam.offset_y(),ts=cam.tile_px();
    const int sw=cam.screen_w(),sh=cam.screen_h(),ml=cam.map_left(),mt=cam.map_top();
    SDL_Rect clip{ml,mt,sw,sh}; SDL_RenderSetClipRect(renderer_,&clip);
    SDL_SetRenderDrawBlendMode(renderer_,SDL_BLENDMODE_BLEND);
    for (int ty=std::max(0,oy/ts-1); ty<=(oy+sh)/ts+1; ++ty)
        for (int tx=std::max(0,ox/ts-1); tx<=(ox+sw)/ts+1; ++tx) {
            const Tile& t=map.get_tile(tx,ty); if(t.room_tag.empty()) continue;
            uint32_t h=5381; for(char c:t.room_tag) h=h*33+(unsigned char)c;
            SDL_Rect r{cam.to_screen_x(tx),cam.to_screen_y(ty),ts-1,ts-1};
            SDL_SetRenderDrawColor(renderer_,
                (Uint8)((h&0x7F)+80),(Uint8)(((h>>8)&0x7F)+80),(Uint8)(((h>>16)&0x7F)+80),90);
            SDL_RenderFillRect(renderer_,&r);
        }
    SDL_SetRenderDrawBlendMode(renderer_,SDL_BLENDMODE_NONE);
    SDL_RenderSetClipRect(renderer_,nullptr);
}

void Renderer::draw_scar_overlay(const Map& map, const Camera& cam) {
    const int ox=cam.offset_x(),oy=cam.offset_y(),ts=cam.tile_px();
    const int sw=cam.screen_w(),sh=cam.screen_h(),ml=cam.map_left(),mt=cam.map_top();
    SDL_Rect clip{ml,mt,sw,sh}; SDL_RenderSetClipRect(renderer_,&clip);
    SDL_SetRenderDrawBlendMode(renderer_,SDL_BLENDMODE_BLEND);
    static constexpr Uint8 ka[]={0,50,90,130,180};
    for (int ty=std::max(0,oy/ts-1); ty<=(oy+sh)/ts+1; ++ty)
        for (int tx=std::max(0,ox/ts-1); tx<=(ox+sw)/ts+1; ++tx) {
            const Tile& t=map.get_tile(tx,ty);
            const int si=t.scar_intensity; if(si<=0||si>4) continue;
            SDL_Rect r{cam.to_screen_x(tx),cam.to_screen_y(ty),ts-1,ts-1};
            SDL_SetRenderDrawColor(renderer_,200,80,255,ka[si]);
            SDL_RenderFillRect(renderer_,&r);
        }
    SDL_SetRenderDrawBlendMode(renderer_,SDL_BLENDMODE_NONE);
    SDL_RenderSetClipRect(renderer_,nullptr);
}

void Renderer::draw_rect_preview(int tx0,int ty0,int tx1,int ty1,bool fill_mode,const Camera& cam) {
    if(tx0>tx1)std::swap(tx0,tx1); if(ty0>ty1)std::swap(ty0,ty1);
    const int ts=cam.tile_px();
    const int sx=cam.to_screen_x(tx0),sy=cam.to_screen_y(ty0);
    const int pw=(tx1-tx0+1)*ts, ph=(ty1-ty0+1)*ts;
    SDL_Rect clip{cam.map_left(),cam.map_top(),cam.screen_w(),cam.screen_h()};
    SDL_RenderSetClipRect(renderer_,&clip);
    SDL_SetRenderDrawBlendMode(renderer_,SDL_BLENDMODE_BLEND);
    if(fill_mode){
        SDL_Rect f{sx,sy,pw,ph};
        SDL_SetRenderDrawColor(renderer_,255,200,60,40); SDL_RenderFillRect(renderer_,&f);
        SDL_SetRenderDrawColor(renderer_,255,220,80,220); SDL_RenderDrawRect(renderer_,&f);
    } else {
        if(tx1-tx0>=2&&ty1-ty0>=2){
            SDL_Rect in{sx+ts,sy+ts,pw-ts*2,ph-ts*2};
            SDL_SetRenderDrawColor(renderer_,78,78,90,50); SDL_RenderFillRect(renderer_,&in);
        }
        SDL_Rect out{sx,sy,pw,ph};
        SDL_SetRenderDrawColor(renderer_,200,220,255,220); SDL_RenderDrawRect(renderer_,&out);
        if(tx1-tx0>=2&&ty1-ty0>=2){
            SDL_Rect in2{sx+ts-1,sy+ts-1,pw-ts*2+2,ph-ts*2+2};
            SDL_SetRenderDrawColor(renderer_,140,160,200,100); SDL_RenderDrawRect(renderer_,&in2);
        }
    }
    char buf[32]; std::snprintf(buf,sizeof(buf),"%dx%d",tx1-tx0+1,ty1-ty0+1);
    draw_text(buf,sx+3,sy+3,220,230,255);
    SDL_SetRenderDrawBlendMode(renderer_,SDL_BLENDMODE_NONE);
    SDL_RenderSetClipRect(renderer_,nullptr);
}

void Renderer::draw_spawn_marker(int tx, int ty, const Camera& cam) {
    const int ts=cam.tile_px(), sx=cam.to_screen_x(tx), sy=cam.to_screen_y(ty);
    const int pad=std::max(2,ts/8);
    SDL_Rect clip{cam.map_left(),cam.map_top(),cam.screen_w(),cam.screen_h()};
    SDL_RenderSetClipRect(renderer_,&clip);
    SDL_SetRenderDrawBlendMode(renderer_,SDL_BLENDMODE_BLEND);
    SDL_Rect fill{sx+pad,sy+pad,ts-pad*2,ts-pad*2};
    SDL_SetRenderDrawColor(renderer_,0,220,80,90); SDL_RenderFillRect(renderer_,&fill);
    SDL_SetRenderDrawColor(renderer_,0,255,100,230); SDL_RenderDrawRect(renderer_,&fill);
    const int cx=sx+ts/2,cy=sy+ts/2;
    SDL_SetRenderDrawColor(renderer_,0,255,100,200);
    SDL_RenderDrawLine(renderer_,cx-ts/4,cy,cx+ts/4,cy);
    SDL_RenderDrawLine(renderer_,cx,cy-ts/4,cx,cy+ts/4);
    SDL_SetRenderDrawBlendMode(renderer_,SDL_BLENDMODE_NONE);
    SDL_RenderSetClipRect(renderer_,nullptr);
    if(ts>=24) draw_text("S",sx+ts/2-3,sy-14,0,255,100);
}

// ── Entity layer rendering ────────────────────────────────────────────────────

char Renderer::category_letter(const std::string& cat) {
    if(cat=="spawn")        return 'S';
    if(cat=="furniture")    return 'F';
    if(cat=="workstation")  return 'W';
    if(cat=="terminal")     return 'T';
    if(cat=="wall_mounted") return 'M';
    if(cat=="container")    return 'C';
    if(cat=="door")         return 'D';
    if(cat=="trigger")      return 'Z';
    return '?';
}

void Renderer::draw_entity_layer(const Map& map,
                                  const std::vector<EntityDef>& defs,
                                  const Camera& cam) {
    const int ts=cam.tile_px(), ml=cam.map_left(), mt=cam.map_top();
    const int sw=cam.screen_w(), sh=cam.screen_h();
    const int ox=cam.offset_x(), oy=cam.offset_y();

    SDL_Rect clip{ml,mt,sw,sh}; SDL_RenderSetClipRect(renderer_,&clip);
    SDL_SetRenderDrawBlendMode(renderer_,SDL_BLENDMODE_BLEND);

    const int stx=std::max(0,(ox)/ts-1),  etx=(ox+sw)/ts+1;
    const int sty=std::max(0,(oy)/ts-1),  ety=(oy+sh)/ts+1;

    for (const auto& e : map.get_entities()) {
        if (e.x < stx || e.x > etx || e.y < sty || e.y > ety) continue;

        const EntityDef* def = find_entity_def(defs, e.type_id);
        const int sx=cam.to_screen_x(e.x), sy=cam.to_screen_y(e.y);
        const int pad=std::max(2,ts/10);

        if (def && def->category=="trigger") {
            // Trigger zones: dashed outline only.
            SDL_Rect r{sx+pad,sy+pad,ts-pad*2-1,ts-pad*2-1};
            SDL_SetRenderDrawColor(renderer_,def->r,def->g,def->b,160);
            SDL_RenderDrawRect(renderer_,&r);
            const int cx=sx+ts/2,cy=sy+ts/2;
            SDL_RenderDrawLine(renderer_,cx-4,cy,cx+4,cy);
            SDL_RenderDrawLine(renderer_,cx,cy-4,cx,cy+4);
            continue;
        }

        if (def && def->wall_mounted) {
            // Wall-mounted: thin strip on the facing edge.
            const int thick=std::max(3,ts/5);
            SDL_Rect strip{};
            switch(e.facing){
                case 'N': strip={sx,sy,ts-1,thick}; break;
                case 'S': strip={sx,sy+ts-thick-1,ts-1,thick}; break;
                case 'W': strip={sx,sy,thick,ts-1}; break;
                default:  strip={sx+ts-thick-1,sy,thick,ts-1}; break;
            }
            SDL_SetRenderDrawColor(renderer_,
                def?def->r:128, def?def->g:128, def?def->b:128, 220);
            SDL_RenderFillRect(renderer_,&strip);
            continue;
        }

        // Normal entity: filled square with category letter.
        const Uint8 er=def?def->r:128, eg=def?def->g:128, eb=def?def->b:128;
        SDL_Rect fill{sx+pad,sy+pad,ts-pad*2-1,ts-pad*2-1};
        SDL_SetRenderDrawColor(renderer_,er,eg,eb,200); SDL_RenderFillRect(renderer_,&fill);
        SDL_SetRenderDrawColor(renderer_,
            std::min(255,(int)er+60), std::min(255,(int)eg+60), std::min(255,(int)eb+60),230);
        SDL_RenderDrawRect(renderer_,&fill);

        // Category letter badge (top-right corner).
        if (ts >= 20 && def) {
            char badge[3]={category_letter(def->category),'\0'};
            draw_text(badge, sx+ts-10, sy+1, 255,255,255);
        }

        // Label text below spawn entities (enemy type, NPC role, etc.)
        if (def && def->category == "spawn" && !e.label.empty() && ts >= 18) {
            const std::string nm = e.label.length() > 8 ? e.label.substr(0,7)+"~" : e.label;
            draw_text(nm, sx+1, sy+ts+2, 230,200,80);
        }
    }

    SDL_SetRenderDrawBlendMode(renderer_,SDL_BLENDMODE_NONE);
    SDL_RenderSetClipRect(renderer_,nullptr);
}

void Renderer::draw_entity_link_lines(const Map& map, const Camera& cam,
                                       bool link_selected, uint32_t selected_src_uid) {
    const int ts = cam.tile_px(), half = ts / 2;
    SDL_Rect clip{cam.map_left(), cam.map_top(), cam.screen_w(), cam.screen_h()};
    SDL_RenderSetClipRect(renderer_, &clip);
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);

    for (const auto& src : map.get_entities()) {
        if (src.linked_id.empty()) continue;

        // Resolve target: try UID string first, then label fallback
        uint32_t tgt_uid = 0;
        try { tgt_uid = (uint32_t)std::stoul(src.linked_id); } catch (...) {}
        const EntityInstance* tgt = tgt_uid ? map.find_entity_by_uid(tgt_uid) : nullptr;
        if (!tgt) {
            for (const auto& e2 : map.get_entities())
                if (!e2.label.empty() && e2.label == src.linked_id && e2.uid != src.uid)
                    { tgt = &e2; break; }
        }
        if (!tgt) continue;

        const bool sel = link_selected && src.uid == selected_src_uid;
        const Uint8 lr = sel ? 255 : 80;
        const Uint8 lg = sel ? 220 : 220;
        const Uint8 lb = sel ? 60  : 200;
        const Uint8 la = sel ? 240 : 170;

        const int x1 = cam.to_screen_x(src.x) + half;
        const int y1 = cam.to_screen_y(src.y) + half;
        const int x2 = cam.to_screen_x(tgt->x) + half;
        const int y2 = cam.to_screen_y(tgt->y) + half;

        SDL_SetRenderDrawColor(renderer_, lr, lg, lb, la);
        SDL_RenderDrawLine(renderer_, x1, y1, x2, y2);

        // Midpoint dot — visible click target for selection
        const int mx = (x1 + x2) / 2, my = (y1 + y2) / 2;
        SDL_Rect dot{mx-3, my-3, 6, 6};
        SDL_RenderFillRect(renderer_, &dot);
    }

    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_NONE);
    SDL_RenderSetClipRect(renderer_, nullptr);
}

void Renderer::draw_entity_placement_cursor(const EntityDef& def, char facing,
                                             int tx, int ty, const Camera& cam) {
    const int ts=cam.tile_px(), sx=cam.to_screen_x(tx), sy=cam.to_screen_y(ty);
    const int pad=std::max(2,ts/10);
    SDL_Rect clip{cam.map_left(),cam.map_top(),cam.screen_w(),cam.screen_h()};
    SDL_RenderSetClipRect(renderer_,&clip);
    SDL_SetRenderDrawBlendMode(renderer_,SDL_BLENDMODE_BLEND);

    SDL_Rect r{sx+pad,sy+pad,ts-pad*2-1,ts-pad*2-1};
    SDL_SetRenderDrawColor(renderer_,def.r,def.g,def.b,100); SDL_RenderFillRect(renderer_,&r);
    SDL_SetRenderDrawColor(renderer_,def.r,def.g,def.b,230); SDL_RenderDrawRect(renderer_,&r);

    // Facing arrow for wall_mounted
    if (def.facing_required) {
        const int cx=sx+ts/2, cy=sy+ts/2, asz=ts/4;
        SDL_SetRenderDrawColor(renderer_,255,255,255,200);
        switch(facing){
            case 'N': SDL_RenderDrawLine(renderer_,cx,cy,cx,cy-asz); break;
            case 'S': SDL_RenderDrawLine(renderer_,cx,cy,cx,cy+asz); break;
            case 'W': SDL_RenderDrawLine(renderer_,cx,cy,cx-asz,cy); break;
            default:  SDL_RenderDrawLine(renderer_,cx,cy,cx+asz,cy); break;
        }
    }

    SDL_SetRenderDrawBlendMode(renderer_,SDL_BLENDMODE_NONE);
    SDL_RenderSetClipRect(renderer_,nullptr);
}

// ── Left panels ───────────────────────────────────────────────────────────────

void Renderer::draw_tile_palette(const std::vector<TileDef>& defs,
                                  int selected_id, int scroll,
                                  int y_top, int max_h, int panel_w) {
    constexpr int ROW_H=42;
    fill_rect(0,y_top,panel_w,max_h,16,20,26,240);
    draw_rect_outline(0,y_top,panel_w,max_h,50,55,65);

    const int end=std::min(scroll+(max_h/ROW_H)+1,(int)defs.size());
    for (int i=scroll; i<end; ++i) {
        const TileDef& d=defs[i];
        const int y=y_top+(i-scroll)*ROW_H;
        if(y+ROW_H>y_top+max_h) break;
        const bool sel=(d.id==selected_id);
        if(sel) fill_rect(1,y,panel_w-2,ROW_H-1,40,55,80);
        SDL_Rect sw{4,y+4,ROW_H-8,ROW_H-8};
        SDL_SetRenderDrawColor(renderer_,d.r,d.g,d.b,255); SDL_RenderFillRect(renderer_,&sw);
        SDL_SetRenderDrawColor(renderer_,sel?255:70,sel?255:75,sel?255:85,255);
        SDL_RenderDrawRect(renderer_,&sw);
        draw_text(d.name,ROW_H+2,y+14,sel?220:160,sel?225:165,sel?230:175);
    }
}

void Renderer::draw_entity_panel(const std::vector<EntityDef>& defs,
                                  const std::string& selected_id,
                                  const std::set<std::string>& collapsed,
                                  int scroll, int y_top, int max_h, int panel_w) {
    fill_rect(0, y_top, panel_w, max_h, 14, 18, 24, 245);
    draw_rect_outline(0, y_top, panel_w, max_h, 50, 55, 65);

    // Order categories for display.
    static const char* k_cat_order[] = {
        "spawn","furniture","workstation","terminal",
        "wall_mounted","container","door","trigger", nullptr
    };
    static const char* k_cat_names[] = {
        "Spawn","Furniture","Workstation","Terminals",
        "Wall Mounted","Containers","Doors","Triggers", nullptr
    };

    constexpr int HDR_H=20, ROW_H=28, SWATCH=16;

    int y = y_top - scroll;

    for (int ci=0; k_cat_order[ci]; ++ci) {
        const std::string cat = k_cat_order[ci];

        // Collect entities in this category.
        std::vector<const EntityDef*> cat_defs;
        for (const auto& d : defs)
            if (d.category == cat) cat_defs.push_back(&d);
        if (cat_defs.empty()) continue;

        // Category header.
        if (y + HDR_H > y_top && y < y_top + max_h) {
            fill_rect(0, y, panel_w, HDR_H, 28, 34, 44);
            const bool coll = collapsed.count(cat) > 0;
            draw_text(coll ? "+" : "-", 4, y+3, 140,150,165);
            draw_text(k_cat_names[ci], 16, y+3, 160,170,185);
        }
        y += HDR_H;

        if (collapsed.count(cat)) continue;

        for (const auto* d : cat_defs) {
            if (y + ROW_H > y_top && y < y_top + max_h) {
                const bool sel = (d->id == selected_id);
                if (sel) fill_rect(1, y, panel_w-2, ROW_H-1, 40,55,80);
                SDL_Rect sw{4, y+6, SWATCH, SWATCH};
                SDL_SetRenderDrawColor(renderer_,d->r,d->g,d->b,255);
                SDL_RenderFillRect(renderer_,&sw);
                SDL_SetRenderDrawColor(renderer_,sel?255:70,sel?255:75,sel?255:85,255);
                SDL_RenderDrawRect(renderer_,&sw);
                // Truncate name to fit panel.
                std::string nm = d->name;
                while (!nm.empty() && text_width(nm) > panel_w - SWATCH - 12)
                    nm.pop_back();
                draw_text(nm, SWATCH+8, y+7, sel?220:170, sel?225:175, sel?230:180);
            }
            y += ROW_H;
        }
    }
}

// ── Right info panel ──────────────────────────────────────────────────────────

void Renderer::draw_right_panel(const EditorUIState& ui,
                                 int x, int y, int w, int h) {
    fill_rect(x,y,w,h,16,20,26,245);
    draw_line(x,y,x,y+h,50,55,65);
    int ty=y+8;
    draw_text("MAP INFO",x+8,ty,120,130,145); ty+=18;
    draw_line(x+4,ty,x+w-4,ty,45,50,60); ty+=6;
    char buf[64];
    std::snprintf(buf,sizeof(buf),"Size: %dx%d",ui.map_w,ui.map_h);
    draw_text(buf,x+8,ty,170,175,185); ty+=16;
    std::snprintf(buf,sizeof(buf),"Zoom: %d%%",ui.zoom_pct);
    draw_text(buf,x+8,ty,170,175,185); ty+=24;
    draw_text("SELECTED",x+8,ty,120,130,145); ty+=18;
    draw_line(x+4,ty,x+w-4,ty,45,50,60); ty+=6;
    std::snprintf(buf,sizeof(buf),"Tile: %d",ui.tile_id);
    draw_text(buf,x+8,ty,170,175,185); ty+=16;
    if(ui.entity_mode && ui.entity_type) {
        draw_text(ui.entity_type,x+8,ty,180,200,160); ty+=16;
    }
    if(ui.cursor_tx>=0){
        std::snprintf(buf,sizeof(buf),"Cur: %d,%d",ui.cursor_tx,ui.cursor_ty);
        draw_text(buf,x+8,ty,170,175,185);
    }
}

// ── Text-properties section (shared by tile and entity props) ─────────────────

int Renderer::draw_text_section(int px, int y, int has_pickup,
                                 const std::string& desc,    const std::string& narrator,
                                 const std::string& readable, const std::string& pickup,
                                 const std::string& voice_id, const std::string& dismiss,
                                 const TextFieldEdit* tfe) {
    constexpr int W=k_props_w, PAD=8, ROW=20;

    draw_line(px+PAD, y+4, px+W-PAD, y+4, 45,52,62); y+=10;
    draw_text("TEXT", px+PAD, y, 110,130,160); y+=16;

    auto draw_field = [&](const char* label, int field_id, const std::string& val) {
        const bool editing = (tfe && tfe->active == field_id);
        draw_text(label, px+PAD, y, 110,115,125);
        fill_rect(px+72, y-2, W-PAD-74, 18, 18,22,36);
        draw_rect_outline(px+72, y-2, W-PAD-74, 18,
                          editing?80:45, editing?110:52, editing?180:62);
        std::string disp;
        if (editing && tfe->buf) {
            disp = tfe->buf;
            if ((SDL_GetTicks()/500)%2==0) disp += '|';
        } else {
            disp = val.empty() ? "(none)" : val;
        }
        while (!disp.empty() && text_width(disp) > W-PAD-78) disp.pop_back();
        const Uint8 vr=(val.empty()&&!editing)?50:175, vg=(val.empty()&&!editing)?55:185,
                    vb=(val.empty()&&!editing)?68:205;
        draw_text(disp, px+75, y, editing?210:vr, editing?220:vg, editing?240:vb);
        y+=ROW;
    };

    draw_field("Desc:",   k_tfield_desc,     desc);
    draw_field("Narr:",   k_tfield_narrator,  narrator);
    draw_field("Read:",   k_tfield_readable,  readable);
    if (has_pickup)
        draw_field("Pickup:", k_tfield_pickup, pickup);

    // Voice profile row (click to cycle)
    draw_text("Voice:", px+PAD, y, 110,115,125);
    fill_rect(px+72, y-2, W-PAD-74, 18, 28,34,50);
    draw_rect_outline(px+72, y-2, W-PAD-74, 18, 55,65,90);
    {
        std::string vdisp = voice_id.empty() ? "station_ambient" : voice_id;
        if (voice_profiles_) {
            const VoiceProfile* vp = find_voice_profile(*voice_profiles_, vdisp);
            if (vp && !vp->display_name.empty()) vdisp = vp->display_name;
        }
        while (!vdisp.empty() && text_width(vdisp) > W-PAD-82) vdisp.pop_back();
        draw_text(vdisp, px+75, y, 160,175,200);
    }
    draw_text("v", px+W-PAD-10, y, 120,130,155);
    y+=ROW;

    // Dismiss mode row (click to cycle)
    draw_text("Dismiss:", px+PAD, y, 110,115,125);
    fill_rect(px+72, y-2, W-PAD-74, 18, 28,34,50);
    draw_rect_outline(px+72, y-2, W-PAD-74, 18, 55,65,90);
    {
        const std::string ddisp = dismiss.empty() ? "input" : dismiss;
        draw_text(ddisp, px+75, y, 160,175,200);
    }
    draw_text("v", px+W-PAD-10, y, 120,130,155);
    y+=ROW;

    return y+4;
}

// ── Entity properties panel ───────────────────────────────────────────────────

void Renderer::draw_entity_props(const EntityInstance& ei, const EntityDef* def,
                                  int px, int py,
                                  const char* label_override, bool label_editing,
                                  const TextFieldEdit* tfe) {
    constexpr int W=k_props_w, PAD=8, ROW=20;
    constexpr int k_text_sect = 10+16+ROW*6+4;  // 6 rows incl. pickup
    const int k_item_sect = (ei.type_id=="item_pickup") ? (10+5+16+ROW) : 0;
    const bool is_ct = (def && def->category == "container");
    const int n_g_h = is_ct ? (int)ei.container_guaranteed.size() : 0;
    const int k_container_sect = is_ct ? (10+5+16 + ROW*3 + (n_g_h>0?6+n_g_h*ROW:0) + 6 + ROW) : 0;
    int h = 24 + ROW*8 + 8 + (def&&def->facing_required?ROW+4:0) + 32 + k_text_sect + k_item_sect + k_container_sect;

    fill_rect(px,py,W,h,22,26,34,248);
    draw_rect_outline(px,py,W,h,70,80,100);
    fill_rect(px,py,W,22,32,38,52);
    draw_text("ENTITY PROPERTIES",px+PAD,py+4,200,210,220);
    // Close hint
    draw_text("[Esc]",px+W-40,py+5,90,95,105);

    int y=py+26;
    draw_text("Type:",px+PAD,y,110,115,125);
    draw_text(def?def->name.c_str():ei.type_id.c_str(),px+60,y,200,210,220); y+=ROW;

    draw_text("Cat:",px+PAD,y,110,115,125);
    draw_text(def?def->category.c_str():"?",px+60,y,160,170,185); y+=ROW;

    draw_text("Label:",px+PAD,y,110,115,125);
    if (label_editing) {
        fill_rect(px+60,y-2,W-PAD-62,18,18,24,38);
        draw_rect_outline(px+60,y-2,W-PAD-62,18,80,110,180);
        std::string disp = label_override ? label_override : "";
        if ((SDL_GetTicks()/500)%2==0) disp += '|';
        draw_text(disp.substr(0, 22),px+63,y,210,220,240);
    } else {
        const std::string lbl = label_override ? label_override :
                                (ei.label.empty() ? "(click to edit)" : ei.label);
        const Uint8 lr = ei.label.empty()?70:180;
        const Uint8 lg = ei.label.empty()?75:185;
        const Uint8 lb = ei.label.empty()?90:195;
        draw_text(lbl,px+60,y,lr,lg,lb);
    }
    y+=ROW;

    if (def && def->facing_required) {
        draw_text("Facing:",px+PAD,y,110,115,125);
        static const char* dirs[]{"N","E","S","W",nullptr};
        int bx=px+60;
        for (int d=0; dirs[d]; ++d) {
            const bool act=(ei.facing==dirs[d][0]);
            fill_rect(bx,y-1,20,18, act?60:30, act?90:36, act?130:48);
            draw_rect_outline(bx,y-1,20,18, act?120:60, act?160:70, act?200:90);
            draw_text(dirs[d],bx+5,y+1, act?230:160, act?235:165, act?240:175);
            bx+=24;
        }
        y+=ROW+4;
    }

    draw_text("Func:",px+PAD,y,110,115,125);
    draw_checkbox(px+60,y,ei.functional, false);
    draw_text(ei.functional?"On":"Off",px+78,y,180,185,195); y+=ROW;

    draw_text("Shift:",px+PAD,y,110,115,125);
    draw_text(ei.visible_shift,px+60,y,170,175,185); y+=ROW;

    // Sprite texture placeholder
    draw_text("Sprite:",px+PAD,y,110,115,125);
    constexpr int TH=16;
    fill_rect(px+60,y,TH,TH,28,34,48);
    draw_rect_outline(px+60,y,TH,TH,55,65,80);
    if (!ei.texture_id.empty() && atlases_) {
        const TextureAtlas* atl=nullptr; const AtlasEntry* ent=nullptr;
        if (find_atlas_entry(*atlases_,ei.texture_id,atl,ent))
            draw_atlas_cell(*atl,ent->cell,px+61,y+1,TH-2,TH-2);
    }
    const int bx=px+W-PAD-38;
    fill_rect(bx,y,38,18,38,52,80);
    draw_rect_outline(bx,y,38,18,65,88,130);
    draw_text("Pick",bx+7,y+2,155,175,220);
    if (!ei.texture_id.empty()) {
        const auto col=ei.texture_id.find(':');
        std::string nm = (col!=std::string::npos)?ei.texture_id.substr(col+1):ei.texture_id;
        while(!nm.empty()&&text_width(nm)>bx-px-60-TH-6) nm.pop_back();
        draw_text(nm,px+60+TH+3,y+2,140,150,170);
    }
    y+=ROW;

    // Position (read-only)
    draw_text("Pos:",px+PAD,y,110,115,125);
    char posbuf[32]; std::snprintf(posbuf,sizeof(posbuf),"%d, %d",ei.x,ei.y);
    draw_text(posbuf,px+60,y,170,175,185);
    if (def && def->id == "player_spawn")
        draw_text("M=move",px+W-PAD-50,y,80,160,80);
    y+=ROW;

    // Item ID section (item_pickup only)
    if (ei.type_id == "item_pickup") {
        y += 10;
        fill_rect(px+PAD, y, W-PAD*2, 1, 60,70,90);
        y += 5;
        draw_text("ITEM", px+PAD, y, 120,130,145);
        y += 16;
        draw_text("Item:", px+PAD, y, 110,115,125);
        const bool iid_editing = tfe && tfe->active == k_tfield_item_id;
        fill_rect(px+72, y-2, W-PAD-74, 18,
                  iid_editing?18:12, iid_editing?24:16, iid_editing?38:26);
        if (iid_editing) draw_rect_outline(px+72, y-2, W-PAD-74, 18, 80,110,180);
        std::string iid_disp = iid_editing
            ? (tfe->buf ? tfe->buf : "")
            : (ei.item_id.empty() ? "(none)" : ei.item_id);
        if (iid_editing && (SDL_GetTicks()/500)%2==0) iid_disp += '|';
        const Uint8 iid_r = (!iid_editing && ei.item_id.empty()) ? 70 : (iid_editing ? 210 : 160);
        const Uint8 iid_g = (!iid_editing && ei.item_id.empty()) ? 75 : (iid_editing ? 220 : 165);
        const Uint8 iid_b = (!iid_editing && ei.item_id.empty()) ? 90 : (iid_editing ? 240 : 180);
        draw_text(iid_disp.substr(0, 24), px+75, y, iid_r, iid_g, iid_b);
        y += ROW;
    }

    // Text properties section
    y = draw_text_section(px, y,
                          /*has_pickup=*/1,
                          ei.text_desc, ei.text_narrator, ei.text_readable,
                          ei.text_pickup, ei.text_voice, ei.text_dismiss,
                          tfe);

    // Container section (category == "container")
    if (def && def->category == "container") {
        y += 10;
        fill_rect(px+PAD, y, W-PAD*2, 1, 60,70,90);
        y += 5;
        draw_text("CONTAINER", px+PAD, y, 120,130,145);
        y += 16;

        // Loot table field
        draw_text("Loot:", px+PAD, y, 110,115,125);
        const bool lt_ed = tfe && tfe->active == k_tfield_container_loot;
        fill_rect(px+72, y-2, W-PAD-74, 18, lt_ed?18:12, lt_ed?24:16, lt_ed?38:26);
        if (lt_ed) draw_rect_outline(px+72, y-2, W-PAD-74, 18, 80,110,180);
        std::string lt_disp = lt_ed ? (tfe->buf ? tfe->buf : "")
                                    : (ei.container_loot_table.empty() ? "(none)" : ei.container_loot_table);
        if (lt_ed && (SDL_GetTicks()/500)%2==0) lt_disp += '|';
        draw_text(lt_disp.substr(0,24), px+75, y,
                  lt_ed?210:( ei.container_loot_table.empty()?70:160),
                  lt_ed?220:(ei.container_loot_table.empty()?75:165),
                  lt_ed?240:(ei.container_loot_table.empty()?90:180));
        y += ROW;

        // Rolls field
        draw_text("Rolls:", px+PAD, y, 110,115,125);
        const bool ro_ed = tfe && tfe->active == k_tfield_container_rolls;
        fill_rect(px+72, y-2, 60, 18, ro_ed?18:12, ro_ed?24:16, ro_ed?38:26);
        if (ro_ed) draw_rect_outline(px+72, y-2, 60, 18, 80,110,180);
        std::string ro_disp = ro_ed ? (tfe->buf ? tfe->buf : "")
                                    : std::to_string(ei.container_rng_rolls);
        if (ro_ed && (SDL_GetTicks()/500)%2==0) ro_disp += '|';
        draw_text(ro_disp, px+75, y, ro_ed?210:160, ro_ed?220:165, ro_ed?240:180);
        y += ROW;

        // Locked toggle
        draw_text("Locked:", px+PAD, y, 110,115,125);
        draw_checkbox(px+72, y, ei.container_locked, false);
        draw_text(ei.container_locked ? "Yes" : "No", px+90, y, 170,175,185);
        y += ROW;

        // Guaranteed items list
        const int n_g = (int)ei.container_guaranteed.size();
        if (n_g > 0) {
            draw_line(px+PAD, y+2, px+W-PAD, y+2, 45,52,65);
            y += 6;
            for (int gi = 0; gi < n_g; ++gi) {
                draw_text(ei.container_guaranteed[gi].substr(0,22), px+PAD, y, 160,165,180);
                // Remove button
                const int bx = px+W-PAD-20;
                fill_rect(bx, y-1, 18, 16, 80,28,28);
                draw_rect_outline(bx, y-1, 18, 16, 130,50,50);
                draw_text("x", bx+5, y, 220,160,160);
                y += ROW;
            }
        }

        // Add guaranteed item field
        draw_line(px+PAD, y+2, px+W-PAD, y+2, 45,52,65);
        y += 6;
        draw_text("Add:", px+PAD, y, 110,115,125);
        const bool add_ed = tfe && tfe->active == k_tfield_container_add;
        fill_rect(px+50, y-2, W-PAD-52, 18, add_ed?18:12, add_ed?24:16, add_ed?38:26);
        if (add_ed) draw_rect_outline(px+50, y-2, W-PAD-52, 18, 80,110,180);
        std::string add_disp = add_ed ? (tfe->buf ? tfe->buf : "") : "";
        if (add_ed && (SDL_GetTicks()/500)%2==0) add_disp += '|';
        if (!add_ed) { add_disp = "(item id)"; }
        draw_text(add_disp.substr(0,24), px+53, y,
                  add_ed?210:60, add_ed?220:65, add_ed?240:80);
        y += ROW;
    }

    // Delete button
    fill_rect(px+PAD,y,W-PAD*2,24,120,30,30);
    draw_rect_outline(px+PAD,y,W-PAD*2,24,180,50,50);
    draw_text("DELETE ENTITY",px+PAD+30,y+5,230,180,180);
}

// ── Tile properties panel ─────────────────────────────────────────────────────

void Renderer::draw_checkbox(int x, int y, bool checked, bool overridden) {
    fill_rect(x,y,14,14,30,35,45);
    draw_rect_outline(x,y,14,14,70,80,100);
    if(checked){
        draw_line(x+2,y+7,x+5,y+11,100,220,100);
        draw_line(x+5,y+11,x+12,y+3,100,220,100);
    }
    if(overridden){ // small orange dot indicator
        fill_rect(x-5,y+4,5,5,255,160,40);
    }
}

int Renderer::draw_tile_props(const Tile& tile, const TileDef* tdef,
                               int /*tile_x*/, int /*tile_y*/, int px, int py,
                               const TextFieldEdit* tfe) {
    constexpr int W=k_props_w, PAD=8, ROW=22;
    const bool is_door = (tile.tile_id == 3);
    const int door_tex_h = is_door ? (10 + 8 + 16 + 3*28) : 0;
    constexpr int k_text_sect = 10+16+20*5+4;  // 5 rows (no pickup for tiles)
    const int h = 24+18+4+ROW*6+16+28 + door_tex_h + k_text_sect;

    fill_rect(px,py,W,h,22,26,34,248);
    draw_rect_outline(px,py,W,h,70,80,100);
    fill_rect(px,py,W,22,32,38,52);
    draw_text("TILE PROPERTIES",px+PAD,py+4,200,210,220);
    draw_text("[Esc]",px+W-40,py+5,90,95,105);

    int y=py+26;
    char buf[64];
    std::snprintf(buf,sizeof(buf),"%s (id:%d)",
        tdef?tdef->name.c_str():"Unknown",tile.tile_id);
    draw_text(buf,px+PAD,y,160,170,185); y+=22;
    draw_line(px+PAD,y,px+W-PAD,y,45,52,62); y+=6;

    struct OvRow { const char* label; int8_t ov; bool def_val; };
    const OvRow rows[]={
        {"Walkable",      tile.ov_walkable,       tdef?tdef->walkable:false},
        {"Pathable",      tile.ov_pathable,       tdef?tdef->walkable:false},
        {"Transparent",   tile.ov_transparent,    (tile.tile_id==2||tile.tile_id==3)},
        {"Blocks Light",  tile.ov_blocks_light,   (tile.tile_id==1||tile.tile_id==6||tile.tile_id==7)},
        {"Interactable",  tile.ov_interactable,   false},
        {"Scar-Permeable",tile.ov_scar_permeable, false},
    };

    for (const auto& row : rows) {
        const bool overridden = (row.ov >= 0);
        const bool val        = overridden ? (row.ov == 1) : row.def_val;
        draw_checkbox(px+PAD+6, y, val, overridden);
        draw_text(row.label, px+PAD+26, y+1, overridden?220:160, overridden?210:165, overridden?180:175);
        y+=ROW;
    }

    draw_line(px+PAD,y,px+W-PAD,y,45,52,62); y+=8;
    fill_rect(px+PAD,y,W-PAD*2,22,30,45,35);
    draw_rect_outline(px+PAD,y,W-PAD*2,22,50,80,60);
    draw_text("RESET TO DEFAULTS",px+PAD+16,y+4,160,200,160);
    y+=28;

    // Door texture slots
    if (is_door) {
        y+=10;
        draw_line(px+PAD,y,px+W-PAD,y,45,52,62); y+=8;
        draw_text("DOOR TEXTURES",px+PAD,y,110,130,160); y+=16;

        auto draw_slot = [&](const char* label, const std::string& tex_id) {
            constexpr int TH=22;
            fill_rect(px+PAD,y,TH,TH,28,34,48);
            draw_rect_outline(px+PAD,y,TH,TH,55,65,80);
            if (!tex_id.empty() && atlases_) {
                const TextureAtlas* atl=nullptr; const AtlasEntry* ent=nullptr;
                if (find_atlas_entry(*atlases_,tex_id,atl,ent))
                    draw_atlas_cell(*atl,ent->cell,px+PAD+1,y+1,TH-2,TH-2);
            }
            draw_text(label,px+PAD+TH+4,y+4,130,135,150);
            const int bx=px+W-PAD-38;
            fill_rect(bx,y,38,20,38,52,80);
            draw_rect_outline(bx,y,38,20,65,88,130);
            draw_text("Pick",bx+7,y+3,155,175,220);
            y+=28;
        };
        draw_slot("Open:",   tile.texture_open);
        draw_slot("Closed:", tile.texture_closed);
        draw_slot("Locked:", tile.texture_locked);
    }

    // Text properties section (no pickup field for tiles)
    draw_text_section(px, y,
                      /*has_pickup=*/0,
                      tile.text_desc, tile.text_narrator, tile.text_readable,
                      /*pickup=*/"", tile.text_voice, tile.text_dismiss,
                      tfe);

    return h;
}

// ── Menu bar ──────────────────────────────────────────────────────────────────

void Renderer::draw_menu_bar(const MenuBar& mb) { mb.draw(*this, screen_w()); }

// ── Status bar ────────────────────────────────────────────────────────────────

void Renderer::draw_overlay_dot(int x, int y, bool active) {
    SDL_Rect box{x,y,8,10};
    SDL_SetRenderDrawColor(renderer_,active?55:40,active?180:44,active?55:52,255);
    SDL_RenderFillRect(renderer_,&box);
    SDL_SetRenderDrawColor(renderer_,80,90,100,255);
    SDL_RenderDrawRect(renderer_,&box);
}

void Renderer::draw_editor_ui(const EditorUIState& ui) {
    const int sw=screen_w(), sh=screen_h(), sy=sh-k_status_bar_h;

    fill_rect(0,sy,sw,k_status_bar_h,18,22,28);
    draw_line(0,sy,sw,sy,45,52,62);

    // Tile swatches.
    static constexpr TileColor k_sw[]={
        {5,5,10},{40,40,50},{78,78,90},{108,72,35}
    };
    const int nsw=tile_defs_?(int)tile_defs_->size():4;
    const int show=std::min(nsw,8);
    for(int i=0;i<show;++i){
        TileColor c;
        if(tile_defs_&&i<(int)tile_defs_->size()) c={(*tile_defs_)[i].r,(*tile_defs_)[i].g,(*tile_defs_)[i].b};
        else c=(i<4)?k_sw[i]:k_sw[0];
        const int id=tile_defs_?(*tile_defs_)[i].id:i;
        const bool sel=(id==ui.tile_id)&&!ui.entity_mode;
        SDL_Rect border{4+i*36,sy+2,32,36};
        SDL_SetRenderDrawColor(renderer_,sel?255:70,sel?255:75,sel?255:85,255);
        SDL_RenderDrawRect(renderer_,&border);
        SDL_Rect fill{5+i*36,sy+3,30,34};
        SDL_SetRenderDrawColor(renderer_,c.r,c.g,c.b,255); SDL_RenderFillRect(renderer_,&fill);
        char num[4]; std::snprintf(num,sizeof(num),"%d",id);
        draw_text(num,7+i*36,sy+24,140,145,155);
    }

    // Tool / cursor info.
    const int mid_x=8*36+12;
    char row1[80];
    if(ui.texture_mode) {
        if(ui.texture_id)
            std::snprintf(row1,sizeof(row1),"Texture: %-22s",ui.texture_id);
        else
            std::snprintf(row1,sizeof(row1),"Texture Mode (\\:picker  T:off)");
    } else if(ui.entity_mode && ui.entity_type)
        std::snprintf(row1,sizeof(row1),"Entity: %-12s [%c]",ui.entity_type,ui.entity_facing);
    else if(ui.cursor_tx>=0)
        std::snprintf(row1,sizeof(row1),"%-8s  x:%-4d y:%-4d",ui.tool,ui.cursor_tx,ui.cursor_ty);
    else
        std::snprintf(row1,sizeof(row1),"%-8s",ui.tool);
    draw_text(row1,mid_x,sy+6,ui.dirty?220:170,ui.dirty?160:175,ui.dirty?80:185);
    if(ui.dirty) draw_text("*",mid_x+290,sy+6,220,180,60);

    // Overlay toggles (key labels reflect actual keys).
    const int ov_y=sy+22;
    draw_overlay_dot(mid_x,ov_y,ui.path_on);      draw_text("P",mid_x+11,ov_y-1,ui.path_on?100:80,ui.path_on?210:90,95);
    draw_overlay_dot(mid_x+28,ov_y,ui.rooms_on);  draw_text("O",mid_x+39,ov_y-1,ui.rooms_on?100:80,ui.rooms_on?210:90,95);
    draw_overlay_dot(mid_x+56,ov_y,ui.room_ov_on);draw_text("R",mid_x+67,ov_y-1,ui.room_ov_on?100:80,ui.room_ov_on?210:90,95);
    draw_overlay_dot(mid_x+84,ov_y,ui.light_on);  draw_text("L",mid_x+95,ov_y-1,ui.light_on?220:80,ui.light_on?200:90,95);
    draw_overlay_dot(mid_x+112,ov_y,ui.grid_on);  draw_text("G",mid_x+123,ov_y-1,ui.grid_on?220:80,ui.grid_on?220:90,ui.grid_on?80:95);
    draw_overlay_dot(mid_x+140,ov_y,ui.entity_on);draw_text("V",mid_x+151,ov_y-1,ui.entity_on?80:80,ui.entity_on?180:90,ui.entity_on?220:95);
    draw_overlay_dot(mid_x+168,ov_y,ui.texture_mode);draw_text("T",mid_x+179,ov_y-1,ui.texture_mode?200:80,ui.texture_mode?160:90,ui.texture_mode?60:95);

    // Right: zoom + map size + hints.
    char rbuf[80];
    std::snprintf(rbuf,sizeof(rbuf),"Zoom:%d%%  Map:%dx%d",ui.zoom_pct,ui.map_w,ui.map_h);
    const int rx=sw-(int)strlen(rbuf)*7-90;
    draw_text(rbuf,rx,sy+6,130,135,145);
    if(ui.flash_msg) draw_text(ui.flash_msg,rx,sy+22,100,220,100);
    else             draw_text("F5:play  Ctrl+S:save  Tab:tiles  E:entities  \\:textures  B:brush",rx,sy+22,60,65,75);
}

// ── Room overlay ──────────────────────────────────────────────────────────────

void Renderer::draw_room_overlay(const std::vector<std::pair<int,int>>& tiles,
                                  bool is_open, int seed_x, int seed_y,
                                  const Camera& cam) {
    if (tiles.empty()) return;
    const int ts=cam.tile_px();
    SDL_Rect clip{cam.map_left(),cam.map_top(),cam.screen_w(),cam.screen_h()};
    SDL_RenderSetClipRect(renderer_,&clip);
    SDL_SetRenderDrawBlendMode(renderer_,SDL_BLENDMODE_BLEND);

    // Deterministic color from seed position
    static const Uint8 palette[][3]={
        {80,160,220},{100,200,100},{200,130,80},{180,80,200},
        {80,200,180},{200,180,80},{80,100,200},{200,80,120}
    };
    const int ci = is_open ? -1 : ((seed_x*3+seed_y*7)&7);
    const Uint8 cr = is_open?200:palette[ci][0];
    const Uint8 cg = is_open? 60:palette[ci][1];
    const Uint8 cb = is_open? 60:palette[ci][2];

    // Fill
    SDL_SetRenderDrawColor(renderer_,cr,cg,cb,45);
    for (const auto& [tx,ty] : tiles) {
        SDL_Rect r{cam.to_screen_x(tx),cam.to_screen_y(ty),ts,ts};
        SDL_RenderFillRect(renderer_,&r);
    }
    // Outline (brightest on border tiles adjacent to walls/doors)
    SDL_SetRenderDrawColor(renderer_,cr,cg,cb,130);
    for (const auto& [tx,ty] : tiles) {
        SDL_Rect r{cam.to_screen_x(tx),cam.to_screen_y(ty),ts,ts};
        SDL_RenderDrawRect(renderer_,&r);
    }

    SDL_SetRenderDrawBlendMode(renderer_,SDL_BLENDMODE_NONE);
    SDL_RenderSetClipRect(renderer_,nullptr);
}

// ── Room properties panel ─────────────────────────────────────────────────────

void Renderer::draw_room_props(const RoomDef& room,
                                const std::vector<std::string>& purposes,
                                int purpose_idx,
                                int px, int py,
                                bool name_editing,
                                const std::string& name_buf) {
    constexpr int W=k_props_w, PAD=8, ROW=22, H=180;
    fill_rect(px,py,W,H,22,26,34,248);
    draw_rect_outline(px,py,W,H,70,80,100);
    fill_rect(px,py,W,22,32,38,52);
    draw_text("ROOM PROPERTIES",px+PAD,py+4,200,210,220);
    draw_text("[Esc]",px+W-40,py+5,90,95,105);

    int y=py+26;
    // Name
    draw_text("Name:",px+PAD,y,110,115,125);
    if (name_editing) {
        fill_rect(px+60,y-2,W-PAD-62,18,18,24,38);
        draw_rect_outline(px+60,y-2,W-PAD-62,18,80,110,180);
        std::string disp=name_buf;
        if((SDL_GetTicks()/500)%2==0) disp+='|';
        draw_text(disp.substr(0,22),px+63,y,210,220,240);
    } else {
        const std::string nm=room.name.empty()?"(click to name)":room.name;
        const Uint8 lr=room.name.empty()?70:180, lg=room.name.empty()?75:190, lb=room.name.empty()?90:200;
        draw_text(nm,px+60,y,lr,lg,lb);
    }
    y+=ROW;

    // Purpose cycle
    draw_text("Purpose:",px+PAD,y,110,115,125);
    const std::string pur = purposes.empty()?"(none)":purposes[purpose_idx%std::max(1,(int)purposes.size())];
    fill_rect(px+60,y-2,W-PAD-62,18,28,36,52);
    draw_rect_outline(px+60,y-2,W-PAD-62,18,55,65,90);
    draw_text(pur,px+63,y,160,175,200);
    draw_text("<>",px+W-PAD-16,y,80,100,140);
    y+=ROW;

    // Notes display
    draw_text("Notes:",px+PAD,y,110,115,125);
    const std::string notes_disp=room.notes.empty()?"—":room.notes;
    draw_text(notes_disp.substr(0,22),px+60,y,120,125,140);
    y+=ROW+4;

    draw_line(px+PAD,y,px+W-PAD,y,45,52,62); y+=8;

    // Buttons
    fill_rect(px+PAD,y,W/2-PAD-4,24,30,80,50);
    draw_rect_outline(px+PAD,y,W/2-PAD-4,24,50,130,80);
    draw_text("Save",px+PAD+18,y+5,160,220,170);

    fill_rect(px+W/2+4,y,W/2-PAD-4,24,80,30,30);
    draw_rect_outline(px+W/2+4,y,W/2-PAD-4,24,130,50,50);
    draw_text("Remove",px+W/2+12,y+5,220,160,160);
}

// ── Keyboard shortcuts overlay ────────────────────────────────────────────────

void Renderer::draw_shortcuts_overlay() {
    const int sw=screen_w(), sh=screen_h();
    fill_rect(0,0,sw,sh,0,0,0,180);
    constexpr int W=560,H=480;
    const int ox=(sw-W)/2, oy=(sh-H)/2;
    fill_rect(ox,oy,W,H,20,24,32);
    draw_rect_outline(ox,oy,W,H,70,80,100);
    fill_rect(ox,oy,W,26,30,36,50);
    draw_text("Keyboard Shortcuts",ox+10,oy+6,210,215,220);
    draw_text("(any key to close)",ox+W-130,oy+6,100,105,115);

    struct Row{const char*key;const char*desc;};
    static const Row rows[]={
        {"TOOLS",""},
        {"LMB/drag","Paint tile  (B = return to tile brush)"},
        {"RMB (no drag)","Open properties panel"},
        {"RMB+drag","Erase tiles"},
        {"B","Tile Paint (brush mode)"},
        {"E","Toggle entity panel"},
        {"Q","Eyedropper"},
        {"X","Erase tool"},
        {"F","Flood fill"},
        {"Ctrl+R","Rectangle room tool"},
        {"R (entity mode)","Rotate entity facing"},
        {"0-9","Quick-select tile by ID"},
        {"",""},
        {"TEXTURES",""},
        {"\\","Open texture picker"},
        {"T","Toggle texture paint mode"},
        {"LMB (texture mode)","Paint texture onto tile"},
        {"RMB (texture mode)","Erase texture from tile"},
        {"",""},
        {"NAVIGATION",""},
        {"W/A/S/D / Arrows","Pan camera"},
        {"MMB drag / Scroll","Pan camera"},
        {"Ctrl+Scroll / +/-","Zoom (50/75/100/150/200/300%)"},
        {"F11","Toggle fullscreen"},
        {"",""},
        {"VIEW",""},
        {"Tab","Tile palette (left panel)"},
        {"]","Info panel (right panel)"},
        {"P","Pathability overlay"},
        {"O","Room tags overlay"},
        {"R","Room flood-fill overlay"},
        {"L","Light overlay"},
        {"G","Grid lines"},
        {"V","Entity layer visibility"},
        {"",""},
        {"FILE",""},
        {"M","Place player spawn marker"},
        {"Ctrl+S","Save map"},
        {"Ctrl+Z","Undo"},
        {"Ctrl+Y","Redo"},
        {"F5","Save and play test"},
        {"Escape","Close panel / quit editor"},
    };

    const int col1=ox+12, col2=ox+200, rh=15;
    int y=oy+34;
    for(const auto& row:rows){
        if(!*row.key&&!*row.desc){y+=4;continue;}
        if(!*row.desc){
            draw_text(row.key,col1,y,120,160,200);
            draw_line(col1,y+13,ox+W-12,y+13,45,55,70);
            y+=rh+2;continue;
        }
        draw_text(row.key,col1,y,180,190,160);
        draw_text(row.desc,col2,y,160,165,170);
        y+=rh;
        if(y>oy+H-10)break;
    }
}

// ── Game controls overlay ─────────────────────────────────────────────────────

void Renderer::draw_game_controls_overlay() {
    const int sw=screen_w(), sh=screen_h();
    fill_rect(0,0,sw,sh,0,0,0,180);
    constexpr int W=480, H=380;
    const int ox=(sw-W)/2, oy=(sh-H)/2;
    fill_rect(ox,oy,W,H,20,24,32);
    draw_rect_outline(ox,oy,W,H,70,80,100);
    fill_rect(ox,oy,W,26,30,36,50);
    draw_text("Controls",ox+10,oy+6,210,215,220);
    draw_text("(any key to close)",ox+W-130,oy+6,100,105,115);

    struct Row{const char*key;const char*desc;};
    static const Row rows[]={
        {"MOVEMENT",""},
        {"W / A / S / D",       "Move"},
        {"Arrow Keys",          "Move"},
        {"Numpad 7/8/9/4/6/1/2/3","Move (8 directions)"},
        {"",""},
        {"INTERACTION",""},
        {"E  or  Space",        "Interact / open door"},
        {"Hold E",              "Lock / unlock door"},
        {"E  (facing item)",    "Pick up item"},
        {"",""},
        {"INVENTORY",""},
        {"I",                   "Open / close inventory"},
        {"W / S  or  \xe2\x86\x91\xe2\x86\x93",   "Navigate items"},
        {"\xe2\x86\x90 / \xe2\x86\x92  or  Q",     "Cycle category tabs"},
        {"E",                   "Use selected item"},
        {"R",                   "Drop selected item"},
        {"I",                   "Inspect selected item"},
        {"",""},
        {"TEXT",""},
        {"E  or  Space",        "Advance / dismiss text"},
        {"",""},
        {"GENERAL",""},
        {"+  /  -",             "Zoom in / out"},
        {"Ctrl+Scroll",         "Zoom in / out"},
        {"F11",                 "Toggle fullscreen"},
        {"Esc",                 "Pause menu"},
    };

    const int col1=ox+12, col2=ox+220, rh=15;
    int y=oy+34;
    for(const auto& row:rows){
        if(!*row.key&&!*row.desc){y+=4;continue;}
        if(!*row.desc){
            draw_text(row.key,col1,y,120,160,200);
            draw_line(col1,y+13,ox+W-12,y+13,45,55,70);
            y+=rh+2;continue;
        }
        draw_text(row.key,col1,y,180,190,160);
        draw_text(row.desc,col2,y,160,165,170);
        y+=rh;
        if(y>oy+H-10)break;
    }
}

// ── Tile hover info HUD ───────────────────────────────────────────────────────

void Renderer::draw_tile_hover_hud(const std::vector<std::string>& lines) {
    if (lines.empty()) return;
    constexpr int PAD = 8, ROW = 16, MARGIN = 8;
    int max_w = 0;
    for (const auto& l : lines) {
        const int w = text_width(l);
        if (w > max_w) max_w = w;
    }
    const int pw = max_w + PAD*2;
    const int ph = (int)lines.size() * ROW + PAD;
    const int px = screen_w() - pw - MARGIN;
    const int py = MARGIN;

    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer_, 12, 15, 22, 210);
    SDL_Rect bg{px, py, pw, ph}; SDL_RenderFillRect(renderer_, &bg);
    SDL_SetRenderDrawColor(renderer_, 60, 75, 100, 200);
    SDL_RenderDrawRect(renderer_, &bg);
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_NONE);

    int y = py + PAD/2;
    for (int i = 0; i < (int)lines.size(); ++i) {
        // First line (tile name) is brighter
        const Uint8 r = (i==0)?210:160, g = (i==0)?215:170, b = (i==0)?225:185;
        draw_text(lines[i], px + PAD, y, r, g, b);
        y += ROW;
    }
}
