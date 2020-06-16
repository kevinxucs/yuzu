// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cmath>
#include <mutex>
#include <iostream>
#include "core/frontend/emu_window.h"
#include "core/frontend/input.h"
#include "core/settings.h"

namespace Core::Frontend {

GraphicsContext::~GraphicsContext() = default;

class EmuWindow::TouchState : public Input::Factory<Input::TouchDevice>,
                              public std::enable_shared_from_this<TouchState> {
public:
    std::unique_ptr<Input::TouchDevice> Create(const Common::ParamPackage&) override {
        return std::make_unique<Device>(shared_from_this());
    }

    std::mutex mutex;

    bool touch_pressed = false; ///< True if touchpad area is currently pressed, otherwise false

    float touch_x = 0.0f; ///< Touchpad X-position
    float touch_y = 0.0f; ///< Touchpad Y-position

private:
    class Device : public Input::TouchDevice {
    public:
        explicit Device(std::weak_ptr<TouchState>&& touch_state) : touch_state(touch_state) {}
        std::tuple<float, float, bool> GetStatus() const override {
            if (auto state = touch_state.lock()) {
                std::lock_guard guard{state->mutex};
                return std::make_tuple(state->touch_x, state->touch_y, state->touch_pressed);
            }
            return std::make_tuple(0.0f, 0.0f, false);
        }

    private:
        std::weak_ptr<TouchState> touch_state;
    };
};

EmuWindow::EmuWindow() {
    // TODO: Find a better place to set this.
    config.min_client_area_size =
        std::make_pair(Layout::MinimumSize::Width, Layout::MinimumSize::Height);
    active_config = config;
    touch_state = std::make_shared<TouchState>();
    Input::RegisterFactory<Input::TouchDevice>("emu_window", touch_state);
}

EmuWindow::~EmuWindow() {
    Input::UnregisterFactory<Input::TouchDevice>("emu_window");
}

/**
 * Check if the given x/y coordinates are within the touchpad specified by the framebuffer layout
 * @param layout FramebufferLayout object describing the framebuffer size and screen positions
 * @param framebuffer_x Framebuffer x-coordinate to check
 * @param framebuffer_y Framebuffer y-coordinate to check
 * @return True if the coordinates are within the touchpad, otherwise false
 */
static bool IsWithinTouchscreen(const Layout::FramebufferLayout& layout, unsigned framebuffer_x,
                                unsigned framebuffer_y) {
    return (framebuffer_y >= layout.screen.top && framebuffer_y < layout.screen.bottom &&
            framebuffer_x >= layout.screen.left && framebuffer_x < layout.screen.right);
}

std::tuple<unsigned, unsigned> EmuWindow::ClipToTouchScreen(unsigned new_x, unsigned new_y) const {
    new_x = std::max(new_x, framebuffer_layout.screen.left);
    new_x = std::min(new_x, framebuffer_layout.screen.right - 1);

    new_y = std::max(new_y, framebuffer_layout.screen.top);
    new_y = std::min(new_y, framebuffer_layout.screen.bottom - 1);

    return std::make_tuple(new_x, new_y);
}

void EmuWindow::TouchPressed(unsigned framebuffer_x, unsigned framebuffer_y) {
    if (!IsWithinTouchscreen(framebuffer_layout, framebuffer_x, framebuffer_y))
        return;

    std::cout << "TouchPressed(" << framebuffer_x << ", " << framebuffer_y << ")" << std::endl;

    std::lock_guard guard{touch_state->mutex};
    touch_state->touch_x = static_cast<float>(framebuffer_x - framebuffer_layout.screen.left) /
                           (framebuffer_layout.screen.right - framebuffer_layout.screen.left);
    touch_state->touch_y = static_cast<float>(framebuffer_y - framebuffer_layout.screen.top) /
                           (framebuffer_layout.screen.bottom - framebuffer_layout.screen.top);

    touch_state->touch_pressed = true;
}

void EmuWindow::TouchReleased() {
    std::cout << "TouchReleased()" << std::endl;

    std::lock_guard guard{touch_state->mutex};
    touch_state->touch_pressed = false;
    touch_state->touch_x = 0;
    touch_state->touch_y = 0;
}

void EmuWindow::TouchMoved(unsigned framebuffer_x, unsigned framebuffer_y) {
    if (!touch_state->touch_pressed)
        return;

    if (!IsWithinTouchscreen(framebuffer_layout, framebuffer_x, framebuffer_y))
        std::tie(framebuffer_x, framebuffer_y) = ClipToTouchScreen(framebuffer_x, framebuffer_y);

    TouchPressed(framebuffer_x, framebuffer_y);
}

void EmuWindow::UpdateCurrentFramebufferLayout(unsigned width, unsigned height) {
    NotifyFramebufferLayoutChanged(Layout::DefaultFrameLayout(width, height));
}

} // namespace Core::Frontend
