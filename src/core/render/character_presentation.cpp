#include "core/render/character_presentation.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
[[noreturn]] void invalid(std::uint32_t instance, const std::string& field) {
  throw std::invalid_argument("Character instance " + std::to_string(instance) +
                              ": " + field);
}

const CharacterPoseFrame& poseFor(std::span<const CharacterPoseFrame> poses,
                                  std::uint32_t instance) {
  const auto found =
      std::find_if(poses.begin(), poses.end(),
                   [=](const auto& p) { return p.instance == instance; });
  if (found == poses.end()) invalid(instance, "missing supplied pose");
  return *found;
}
}  // namespace

CharacterPresentation::CharacterPresentation(
    std::span<const CharacterRenderInstance> instances) {
  if (instances.size() > frame_maximum_character_count)
    throw std::invalid_argument("Character selection exceeds four instances");
  // Check aggregate counts/ranges before allocating derived presentation data.
  std::size_t drawn_index_count = 0;
  for (std::size_t i = 0; i < instances.size(); ++i) {
    const auto& selected = instances[i];
    if (!selected.asset) invalid(selected.instance, "missing immutable asset");
    for (std::size_t j = 0; j < i; ++j)
      if (instances[j].instance == selected.instance)
        invalid(selected.instance, "duplicate selected handle");
    const auto& asset = *selected.asset;
    if (asset.vertices.empty() || asset.vertices.size() > 10000 ||
        asset.indices.empty() || asset.indices.size() > 50000 ||
        asset.primitives.empty() || asset.primitives.size() > 2 ||
        asset.joint_nodes.empty() || asset.joint_nodes.size() > 65 ||
        asset.inverse_bind_matrices.size() != asset.joint_nodes.size() ||
        asset.skeleton_identity == 0)
      invalid(selected.instance,
              "asset exceeds the prepared character profile");
    if (asset.vertices.size() > 40000 - vertices_ ||
        asset.indices.size() > 200000 - drawn_index_count)
      invalid(selected.instance,
              "aggregate geometry exceeds the frame profile");
    vertices_ += asset.vertices.size();
    drawn_index_count += asset.indices.size();
    scratch_ = std::max(scratch_, asset.vertices.size());
    std::size_t next_index = 0;
    for (const auto& primitive : asset.primitives) {
      if (primitive.first_index != next_index || primitive.index_count == 0 ||
          primitive.index_count % 3 != 0 ||
          primitive.index_count > asset.indices.size() - next_index)
        invalid(selected.instance, "primitive index range is invalid");
      next_index += primitive.index_count;
      for (const float value : primitive.base_color)
        if (!std::isfinite(value) || value < 0 || value > 1)
          invalid(selected.instance, "primitive base color is invalid");
    }
    if (next_index != asset.indices.size())
      invalid(selected.instance, "primitive ranges do not cover the indices");
    for (const auto index : asset.indices)
      if (index >= asset.vertices.size())
        invalid(selected.instance, "index exceeds source vertices");
    // The immutable handoff normally comes from the bounded loader. Check the
    // palette accesses here as well, since native development fixtures can
    // construct project-owned assets directly.
    for (const auto& vertex : asset.vertices) {
      float weight_sum = 0;
      for (std::size_t influence = 0; influence < 4; ++influence) {
        if (vertex.joints[influence] >= asset.joint_nodes.size() ||
            !std::isfinite(vertex.weights[influence]) ||
            vertex.weights[influence] < 0)
          invalid(selected.instance, "vertex joint/weight is invalid");
        weight_sum += vertex.weights[influence];
      }
      if (!std::isfinite(weight_sum) || std::abs(weight_sum - 1) > 0.00001F)
        invalid(selected.instance, "vertex weights must sum to one");
    }
  }
  // The profile caps imply these bounds, but keep the native draw conversions
  // and derived byte products checked if those caps change.
  if (vertices_ > std::numeric_limits<std::int32_t>::max() ||
      vertices_ > std::numeric_limits<std::size_t>::max() /
                      sizeof(PositionColorVertex) ||
      drawn_index_count > std::numeric_limits<std::uint32_t>::max() ||
      drawn_index_count > std::numeric_limits<std::size_t>::max() /
                              sizeof(std::uint32_t))
    throw std::invalid_argument("Character geometry exceeds draw/storage limits");
  instances_.assign(instances.begin(), instances.end());
  struct SharedAsset {
    const CharacterAsset* asset;
    std::size_t first_index;
    std::size_t first_material;
  };
  std::vector<SharedAsset> assets;
  std::size_t first_vertex = 0;
  for (const auto& selected : instances_) {
    const auto& asset = *selected.asset;
    const auto existing =
        std::find_if(assets.begin(), assets.end(),
                     [&](const auto& shared) { return shared.asset == &asset; });
    SharedAsset shared;
    if (existing == assets.end()) {
      shared = {&asset, indices_.size(), materials_.size()};
      assets.push_back(shared);
      indices_.insert(indices_.end(), asset.indices.begin(), asset.indices.end());
      for (const auto& primitive : asset.primitives)
        materials_.push_back(primitive.base_color);
    } else {
      shared = *existing;
    }
    for (std::size_t p = 0; p < asset.primitives.size(); ++p) {
      const auto& primitive = asset.primitives[p];
      draws_.push_back(
          {static_cast<std::uint32_t>(shared.first_index + primitive.first_index),
           primitive.index_count, static_cast<std::int32_t>(first_vertex),
           shared.first_material + p});
    }
    first_vertex += asset.vertices.size();
  }
}

void CharacterPresentation::validate(
    std::span<const CharacterPoseFrame> poses) const {
  if (poses.size() > frame_maximum_character_count)
    throw std::invalid_argument("Character poses exceed four instances");
  for (std::size_t i = 0; i < poses.size(); ++i) {
    for (std::size_t j = 0; j < i; ++j)
      if (poses[j].instance == poses[i].instance)
        invalid(poses[i].instance, "duplicate supplied pose");
    if (std::none_of(instances_.begin(), instances_.end(), [&](const auto& s) {
          return s.instance == poses[i].instance;
        }))
      invalid(poses[i].instance, "unknown selected handle");
  }
  for (const auto& selected : instances_) {
    const auto& pose = poseFor(poses, selected.instance);
    if (pose.skeleton_identity != selected.asset->skeleton_identity)
      invalid(selected.instance, "pose skeleton does not match selected asset");
    try {
      validateCharacterPose(*selected.asset, pose.joint_globals,
                            {pose.position, pose.yaw_degrees});
    } catch (const std::exception& error) {
      invalid(selected.instance, error.what());
    }
  }
}

void CharacterPresentation::deform(
    std::span<const CharacterPoseFrame> poses,
    std::span<CharacterDeformedVertex> scratch,
    std::span<PositionColorVertex> output) const {
  validate(poses);
  if (scratch.size() < scratch_ || output.size() != vertices_)
    throw std::invalid_argument("Character deformation storage size mismatch");
  std::size_t offset = 0;
  for (const auto& selected : instances_) {
    const auto& asset = *selected.asset;
    const auto& pose = poseFor(poses, selected.instance);
    try {
      deformCharacterInto(asset, pose.joint_globals,
                          {pose.position, pose.yaw_degrees},
                          scratch.first(asset.vertices.size()));
    } catch (const std::exception& error) {
      invalid(selected.instance, error.what());
    }
    for (const auto& source : scratch.first(asset.vertices.size())) {
      auto& vertex = output[offset++];
      std::copy(source.position.begin(), source.position.end(),
                vertex.position);
      std::copy(source.normal.begin(), source.normal.end(), vertex.normal);
      std::copy(source.uv.begin(), source.uv.end(), vertex.texture_coordinates);
      std::fill(std::begin(vertex.color), std::end(vertex.color), 255);
      vertex.texture_layer = 0;
    }
  }
}
