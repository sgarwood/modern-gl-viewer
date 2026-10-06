#pragma once

namespace mgv {

enum class InputAction {
    orbit_left,
    orbit_right,
    orbit_up,
    orbit_down,
    zoom_in,
    zoom_out,
    reset_view,
    fire_test_shot,
    toggle_range_finder,
    range_finder_ping,
};

class InputSink {
public:
    virtual ~InputSink() = default;
    InputSink(const InputSink&) = delete;
    InputSink& operator=(const InputSink&) = delete;

    virtual void handle(InputAction action) = 0;

protected:
    InputSink() = default;
};

} // namespace mgv
