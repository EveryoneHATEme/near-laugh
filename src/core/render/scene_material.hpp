#ifndef CORE_RENDER_SCENE_MATERIAL_HPP
#define CORE_RENDER_SCENE_MATERIAL_HPP

#include <array>

#include "core/resources/image_decoder.hpp"

// The selected base-color game profile, shared by CPU import and GPU upload.
struct SceneMaterialData {
  DecodedRgbaImage image{1, 1, {255, 255, 255, 255}};
  std::array<float, 4> base_color_factor{1, 1, 1, 1};
  bool nearest{};
  bool alpha_mask{};
  float alpha_cutoff{0.5F};
};

[[nodiscard]] inline bool materialCoversSample(
    const SceneMaterialData& material, float sampled_alpha) noexcept {
  return !material.alpha_mask ||
         sampled_alpha * material.base_color_factor[3] >= material.alpha_cutoff;
}

#endif
