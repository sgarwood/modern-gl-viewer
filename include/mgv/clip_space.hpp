#pragma once

namespace mgv {

enum class ClipDepthRange {
    negative_one_to_one,
    zero_to_one,
};

struct ClipSpaceConvention final {
    ClipDepthRange depth_range{ClipDepthRange::negative_one_to_one};
    bool invert_y{};

    friend bool operator==(const ClipSpaceConvention&, const ClipSpaceConvention&) = default;
};

} // namespace mgv
