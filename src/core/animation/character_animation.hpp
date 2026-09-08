#ifndef CORE_ANIMATION_CHARACTER_ANIMATION_HPP
#define CORE_ANIMATION_CHARACTER_ANIMATION_HPP

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

using AnimationMatrix = std::array<float, 16>;  // Column-major.

struct CharacterLocalTransform {
  std::array<float, 3> translation{};
  std::array<float, 4> rotation{0, 0, 0, 1};  // xyzw.
};
using CharacterLocalPose = std::vector<CharacterLocalTransform>;
using CharacterPose = std::vector<AnimationMatrix>;

struct CharacterNode {
  std::int16_t parent{-1};
  CharacterLocalTransform rest{};
};
struct CharacterVertex {
  std::array<float, 3> position{};
  std::array<float, 3> normal{};
  std::array<float, 2> uv{};
  std::array<std::uint16_t, 4> joints{};
  std::array<float, 4> weights{};
};
struct CharacterDeformedVertex {
  std::array<float, 3> position{};
  std::array<float, 3> normal{};
  std::array<float, 2> uv{};
};
struct CharacterPrimitive {
  std::uint32_t first_index{};
  std::uint32_t index_count{};
  std::array<float, 4> base_color{1, 1, 1, 1};
};
struct CharacterChannel {
  std::uint16_t node{};
  bool rotation{};
  std::vector<float> times{};
  std::vector<std::array<float, 4>> values{};
};
struct CharacterClip {
  std::string id{};
  double duration{};
  bool looping{};
  std::vector<CharacterChannel> channels{};
};

// Loader handoff is shared immutable ownership. Joint palettes use skin order;
// local poses use node order, including constant skeleton ancestors.
struct CharacterAsset {
  std::string source{};
  std::uint64_t skeleton_identity{};
  std::vector<CharacterNode> nodes{};
  std::vector<std::uint16_t> evaluation_order{};
  std::vector<std::uint16_t> joint_nodes{};
  std::vector<AnimationMatrix> inverse_bind_matrices{};
  std::vector<CharacterVertex> vertices{};
  std::vector<std::uint32_t> indices{};
  std::vector<CharacterPrimitive> primitives{};
  std::vector<CharacterClip> clips{};
};
struct CharacterPlacement {
  std::array<float, 3> position{};
  float yaw_degrees{};
};

[[nodiscard]] std::shared_ptr<const CharacterAsset> loadCharacterAsset(
    const std::filesystem::path& path);
[[nodiscard]] const CharacterClip& characterClip(const CharacterAsset& asset,
                                                std::string_view id);
[[nodiscard]] CharacterLocalPose characterRestPose(const CharacterAsset& asset);
[[nodiscard]] CharacterLocalPose sampleCharacterPose(const CharacterAsset& asset,
                                                    std::string_view clip,
                                                    double time);
[[nodiscard]] CharacterPose evaluateCharacterPose(
    const CharacterAsset& asset, std::span<const CharacterLocalTransform> local);
void validateCharacterPose(const CharacterAsset& asset,
                           std::span<const AnimationMatrix> joint_globals,
                           const CharacterPlacement& placement);
void deformCharacterInto(const CharacterAsset& asset,
                         std::span<const AnimationMatrix> joint_globals,
                         const CharacterPlacement& placement,
                         std::span<CharacterDeformedVertex> output);
[[nodiscard]] std::vector<CharacterDeformedVertex> deformCharacter(
    const CharacterAsset& asset, std::span<const AnimationMatrix> joint_globals,
    const CharacterPlacement& placement = {});

class CharacterPlayback {
 public:
  explicit CharacterPlayback(std::shared_ptr<const CharacterAsset> asset,
                             std::string_view clip = "idle");
  void selectClip(std::string_view clip);
  // Returns true once on crossing a one-shot endpoint; inspection emits nothing.
  bool advance(double elapsed_seconds);
  void seek(double seconds);
  void restart();
  void setPaused(bool paused) noexcept { paused_ = paused; }
  [[nodiscard]] bool paused() const noexcept { return paused_; }
  [[nodiscard]] double time() const noexcept { return time_; }
  [[nodiscard]] std::string_view clip() const noexcept;
  [[nodiscard]] const CharacterLocalPose& localPose() const noexcept {
    return displayed_;
  }
  [[nodiscard]] CharacterPose pose() const;
  static constexpr double transition_seconds = 0.15;

 private:
  void refresh();
  std::shared_ptr<const CharacterAsset> asset_;
  std::size_t clip_index_{};
  double time_{};
  double time_compensation_{};
  double transition_time_{transition_seconds};
  bool paused_{};
  bool completion_reported_{};
  CharacterLocalPose transition_source_{};
  CharacterLocalPose displayed_{};
};

#endif
