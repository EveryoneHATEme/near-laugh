#ifndef CORE_RENDER_CHARACTER_PRESENTATION_HPP
#define CORE_RENDER_CHARACTER_PRESENTATION_HPP

#include <memory>
#include <span>
#include <vector>

#include "core/animation/character_animation.hpp"
#include "core/render/prototype_scene.hpp"

struct CharacterRenderInstance {
  std::uint32_t instance{};
  std::shared_ptr<const CharacterAsset> asset;
};

struct CharacterDrawRange {
  std::uint32_t first_index{};
  std::uint32_t index_count{};
  std::int32_t vertex_offset{};
  std::size_t material{};
};

// CPU preparation shared by runtime and editor. Selection is immutable; no
// playback time or caller palette is retained. Indices/materials are shared per
// distinct asset; each instance retains its own deformed source vertices.
class CharacterPresentation {
 public:
  explicit CharacterPresentation(
      std::span<const CharacterRenderInstance> instances);
  void validate(std::span<const CharacterPoseFrame> poses) const;
  void deform(std::span<const CharacterPoseFrame> poses,
              std::span<CharacterDeformedVertex> scratch,
              std::span<PositionColorVertex> output) const;
  [[nodiscard]] std::size_t vertexCount() const noexcept { return vertices_; }
  [[nodiscard]] std::size_t scratchCount() const noexcept { return scratch_; }
  [[nodiscard]] std::span<const CharacterDrawRange> draws() const noexcept {
    return draws_;
  }
  [[nodiscard]] std::span<const std::uint32_t> indices() const noexcept {
    return indices_;
  }
  [[nodiscard]] std::span<const std::array<float, 4>> materials()
      const noexcept {
    return materials_;
  }

 private:
  std::vector<CharacterRenderInstance> instances_;
  std::vector<CharacterDrawRange> draws_;
  std::vector<std::uint32_t> indices_;
  std::vector<std::array<float, 4>> materials_;
  std::size_t vertices_{};
  std::size_t scratch_{};
};

#endif
