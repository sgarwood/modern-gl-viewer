#pragma once

#include "mgv/hardware/shot_data.hpp"
#include <string>
#include <optional>

namespace mgv::hardware {

class IShotDataParser {
public:
    virtual ~IShotDataParser() = default;

    // Parses a raw payload string into a ShotData struct, if valid
    virtual std::optional<ShotData> parse(const std::string& payload) const = 0;
};

} // namespace mgv::hardware
