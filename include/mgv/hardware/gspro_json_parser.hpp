#pragma once

#include "mgv/hardware/shot_data_parser.hpp"

namespace mgv::hardware {

class GSProJsonParser final : public IShotDataParser {
public:
    std::optional<ShotData> parse(const std::string& payload) const override;
};

} // namespace mgv::hardware
