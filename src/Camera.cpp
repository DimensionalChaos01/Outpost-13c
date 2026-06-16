#include "Camera.h"
#include <cmath>
#include <algorithm>

void Camera::follow(int tile_x, int tile_y) {
    const float half_w = (float)screen_w_ / (2.0f * k_tile_size * zoom_);
    const float half_h = (float)screen_h_ / (2.0f * k_tile_size * zoom_);
    target_tx_ = (tile_x + 0.5f) - half_w;
    target_ty_ = (tile_y + 0.5f) - half_h;
    if (!smooth_follow_) {
        offset_tx_ = target_tx_;
        offset_ty_ = target_ty_;
    }
}

void Camera::snap_to_tile_f(float tx, float ty) {
    const float half_w = (float)screen_w_ / (2.0f * k_tile_size * zoom_);
    const float half_h = (float)screen_h_ / (2.0f * k_tile_size * zoom_);
    offset_tx_ = target_tx_ = (tx + 0.5f) - half_w;
    offset_ty_ = target_ty_ = (ty + 0.5f) - half_h;
}

void Camera::pan(int dx, int dy) {
    offset_tx_ += (float)dx / (k_tile_size * zoom_);
    offset_ty_ += (float)dy / (k_tile_size * zoom_);
    // Pan detaches from follow target — keep target at current offset.
    target_tx_ = offset_tx_;
    target_ty_ = offset_ty_;
}

void Camera::zoom_to(float new_zoom, int anchor_sx, int anchor_sy) {
    const float atx = offset_tx_ + (float)(anchor_sx - map_left_) / (k_tile_size * zoom_);
    const float aty = offset_ty_ + (float)(anchor_sy - map_top_)  / (k_tile_size * zoom_);
    zoom_ = new_zoom;
    offset_tx_ = atx - (float)(anchor_sx - map_left_) / (k_tile_size * zoom_);
    offset_ty_ = aty - (float)(anchor_sy - map_top_)  / (k_tile_size * zoom_);
    // Keep target in sync after zoom.
    target_tx_ = offset_tx_;
    target_ty_ = offset_ty_;
}

void Camera::zoom_step(int dir) {
    int idx = 2; // default 100%
    for (int i = 0; i < k_num_zoom_steps; ++i)
        if (std::abs(k_zoom_steps[i] - zoom_) < 0.01f) { idx = i; break; }
    idx = std::max(0, std::min(k_num_zoom_steps - 1, idx + dir));
    zoom_to(k_zoom_steps[idx], map_left_ + screen_w_ / 2, map_top_ + screen_h_ / 2);
}

void Camera::update(float dt) {
    if (!smooth_follow_) return;
    const float dx = target_tx_ - offset_tx_;
    const float dy = target_ty_ - offset_ty_;
    const float dist_sq = dx*dx + dy*dy;
    if (dist_sq < 0.0001f) { offset_tx_ = target_tx_; offset_ty_ = target_ty_; return; }
    const float step = follow_speed_ * dt;
    const float dist = std::sqrt(dist_sq);
    if (step >= dist) { offset_tx_ = target_tx_; offset_ty_ = target_ty_; }
    else { offset_tx_ += (dx/dist)*step; offset_ty_ += (dy/dist)*step; }
}

bool Camera::is_detached() const {
    const float dx = target_tx_ - offset_tx_;
    const float dy = target_ty_ - offset_ty_;
    return (dx*dx + dy*dy) > 1.0f;
}
