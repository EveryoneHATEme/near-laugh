#include "core/animation/character_animation.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace {
constexpr std::size_t max_nodes = 80;
constexpr std::size_t max_joints = 65;

[[noreturn]] void fail(const CharacterAsset& asset, const std::string& field) {
  throw std::runtime_error("Character " + asset.source + ": " + field);
}
template <typename Range>
bool finite(const Range& values) {
  return std::all_of(std::begin(values), std::end(values),
                     [](float v) { return std::isfinite(v); });
}
glm::quat quaternion(const std::array<float, 4>& q) {
  return {q[3], q[0], q[1], q[2]};
}
std::array<float, 4> components(const glm::quat& q) {
  return {q.x, q.y, q.z, q.w};
}
AnimationMatrix matrixArray(const glm::mat4& m) {
  AnimationMatrix result;
  std::copy_n(glm::value_ptr(m), 16, result.begin());
  return result;
}
glm::mat4 localMatrix(const CharacterLocalTransform& t) {
  return glm::translate(glm::mat4(1), glm::make_vec3(t.translation.data())) *
         glm::mat4_cast(quaternion(t.rotation));
}
double normalizedTime(const CharacterClip& clip, double time) {
  if (!std::isfinite(time) || time < 0)
    throw std::invalid_argument("Character clip time must be finite and nonnegative");
  return clip.looping ? std::fmod(time, clip.duration) : std::min(time, clip.duration);
}
CharacterLocalTransform blend(const CharacterLocalTransform& from,
                              const CharacterLocalTransform& to, float amount) {
  if (amount <= 0) return from;
  if (amount >= 1) return to;
  CharacterLocalTransform result;
  for (std::size_t i = 0; i < 3; ++i)
    result.translation[i] = std::lerp(from.translation[i], to.translation[i], amount);
  auto target = quaternion(to.rotation);
  const auto origin = quaternion(from.rotation);
  if (glm::dot(origin, target) < 0) target = -target;
  result.rotation = components(glm::normalize(glm::slerp(origin, target, amount)));
  return result;
}
}  // namespace

const CharacterClip& characterClip(const CharacterAsset& asset, std::string_view id) {
  const auto found = std::find_if(asset.clips.begin(), asset.clips.end(),
                                  [&](const auto& c) { return c.id == id; });
  if (found == asset.clips.end()) fail(asset, "clip: unknown identity " + std::string(id));
  return *found;
}
CharacterLocalPose characterRestPose(const CharacterAsset& asset) {
  CharacterLocalPose result;
  result.reserve(asset.nodes.size());
  for (const auto& node : asset.nodes) result.push_back(node.rest);
  return result;
}
CharacterLocalPose sampleCharacterPose(const CharacterAsset& asset, std::string_view id,
                                       double time) {
  const auto& clip = characterClip(asset, id);
  time = normalizedTime(clip, time);
  auto result = characterRestPose(asset);
  for (const auto& channel : clip.channels) {
    const auto upper = std::upper_bound(channel.times.begin(), channel.times.end(), time);
    const auto right = upper == channel.times.end() ? channel.times.size() - 1
                       : static_cast<std::size_t>(upper - channel.times.begin());
    const auto left = right == 0 || upper == channel.times.end() ? right : right - 1;
    const float amount = left == right ? 0 : static_cast<float>(
        (time - channel.times[left]) / (channel.times[right] - channel.times[left]));
    auto& target = result[channel.node];
    if (channel.rotation) {
      CharacterLocalTransform a, b;
      a.rotation = channel.values[left];
      b.rotation = channel.values[right];
      target.rotation = blend(a, b, amount).rotation;
    } else {
      for (std::size_t axis = 0; axis < 3; ++axis)
        target.translation[axis] = std::lerp(channel.values[left][axis], channel.values[right][axis], amount);
    }
  }
  return result;
}
CharacterPose evaluateCharacterPose(const CharacterAsset& asset,
                                     std::span<const CharacterLocalTransform> local) {
  if (local.size() != asset.nodes.size() || local.size() > max_nodes)
    fail(asset, "local pose: skeleton node count mismatch");
  std::array<glm::mat4, max_nodes> global;
  for (const auto index : asset.evaluation_order) {
    const auto& t = local[index];
    if (!finite(t.translation) || !finite(t.rotation) ||
        std::abs(glm::length(quaternion(t.rotation)) - 1.0F) > 0.00001F)
      fail(asset, "local pose node " + std::to_string(index) + ": finite unit TR required");
    const auto parent = asset.nodes[index].parent;
    global[index] = parent < 0 ? localMatrix(t) : global[parent] * localMatrix(t);
    if (!finite(matrixArray(global[index])))
      fail(asset, "local pose node " + std::to_string(index) + ": global transform overflow");
  }
  CharacterPose result;
  result.reserve(asset.joint_nodes.size());
  for (const auto joint : asset.joint_nodes) result.push_back(matrixArray(global[joint]));
  return result;
}
void validateCharacterPose(const CharacterAsset& asset,
                           std::span<const AnimationMatrix> joint_globals,
                           const CharacterPlacement& placement) {
  if (joint_globals.size() != asset.joint_nodes.size() || joint_globals.empty() ||
      joint_globals.size() > max_joints)
    fail(asset, "joint pose: skeleton palette count mismatch");
  if (!finite(placement.position) || !std::isfinite(placement.yaw_degrees))
    fail(asset, "placement: non-finite position or yaw");
  for (std::size_t i = 0; i < joint_globals.size(); ++i) {
    const auto& value = joint_globals[i];
    const auto matrix = glm::make_mat4(value.data());
    const auto rotation = glm::mat3(matrix);
    bool orthogonal = true;
    const auto identity = glm::transpose(rotation) * rotation;
    for (int c = 0; c < 3; ++c)
      for (int r = 0; r < 3; ++r)
        if (std::abs(identity[c][r] - (c == r ? 1.0F : 0.0F)) > 0.001F)
          orthogonal = false;
    if (!finite(value) || std::abs(value[3]) > 0.0001F ||
        std::abs(value[7]) > 0.0001F || std::abs(value[11]) > 0.0001F ||
        std::abs(value[15] - 1) > 0.0001F || !orthogonal ||
        std::abs(glm::determinant(rotation) - 1) > 0.001F)
      fail(asset, "joint pose " + std::to_string(i) + ": finite rigid transform required");
  }
}
void deformCharacterInto(const CharacterAsset& asset,
                         std::span<const AnimationMatrix> joint_globals,
                         const CharacterPlacement& placement,
                         std::span<CharacterDeformedVertex> output) {
  validateCharacterPose(asset, joint_globals, placement);
  if (output.size() != asset.vertices.size())
    fail(asset, "deformation: output vertex count mismatch");
  std::array<glm::mat4, max_joints> skin;
  const float angle = std::remainder(placement.yaw_degrees, 360.0F) *
                      std::numbers::pi_v<float> / 180;
  const auto world = glm::translate(glm::mat4(1), glm::make_vec3(placement.position.data())) *
                     glm::rotate(glm::mat4(1), angle, glm::vec3(0, 1, 0));
  for (std::size_t j = 0; j < joint_globals.size(); ++j)
    skin[j] = glm::make_mat4(joint_globals[j].data()) *
              glm::make_mat4(asset.inverse_bind_matrices[j].data());
  for (std::size_t i = 0; i < asset.vertices.size(); ++i) {
    const auto& vertex = asset.vertices[i];
    const auto position = glm::vec4(glm::make_vec3(vertex.position.data()), 1);
    const auto normal = glm::make_vec3(vertex.normal.data());
    glm::vec4 transformed(0);
    glm::vec3 rotated(0);
    for (std::size_t j = 0; j < 4; ++j) {
      if (vertex.weights[j] == 0) continue;
      transformed += vertex.weights[j] * (skin[vertex.joints[j]] * position);
      rotated += vertex.weights[j] * (glm::mat3(skin[vertex.joints[j]]) * normal);
    }
    transformed = world * transformed;
    rotated = glm::mat3(world) * rotated;
    const float normal_length = glm::length(rotated);
    if (!std::isfinite(normal_length) || normal_length < 1e-8F)
      fail(asset, "deformation vertex " + std::to_string(i) + ": degenerate normal");
    rotated /= normal_length;
    auto& result = output[i];
    result.position = {transformed.x, transformed.y, transformed.z};
    result.normal = {rotated.x, rotated.y, rotated.z};
    result.uv = vertex.uv;
    if (!finite(result.position) || !finite(result.normal))
      fail(asset, "deformation vertex " + std::to_string(i) + ": non-finite geometry");
  }
}
std::vector<CharacterDeformedVertex> deformCharacter(
    const CharacterAsset& asset, std::span<const AnimationMatrix> globals,
    const CharacterPlacement& placement) {
  std::vector<CharacterDeformedVertex> result(asset.vertices.size());
  deformCharacterInto(asset, globals, placement, result);
  return result;
}

CharacterPlayback::CharacterPlayback(std::shared_ptr<const CharacterAsset> asset,
                                     std::string_view id) : asset_(std::move(asset)) {
  if (!asset_) throw std::invalid_argument("CharacterPlayback requires an asset");
  clip_index_ = static_cast<std::size_t>(&characterClip(*asset_, id) - asset_->clips.data());
  refresh();
}
std::string_view CharacterPlayback::clip() const noexcept { return asset_->clips[clip_index_].id; }
void CharacterPlayback::selectClip(std::string_view id) {
  const auto index = static_cast<std::size_t>(&characterClip(*asset_, id) - asset_->clips.data());
  if (index == clip_index_) return;
  transition_source_ = displayed_;
  transition_time_ = 0;
  clip_index_ = index;
  time_ = 0;
  time_compensation_ = 0;
  completion_reported_ = false;
  refresh();
}
bool CharacterPlayback::advance(double elapsed) {
  if (!std::isfinite(elapsed) || elapsed < 0)
    throw std::invalid_argument("Character elapsed time must be finite and nonnegative");
  if (paused_ || elapsed == 0) return false;
  const auto& selected = asset_->clips[clip_index_];
  bool completed = false;
  const double increment = (selected.looping ? std::fmod(elapsed, selected.duration)
                                             : std::min(elapsed, selected.duration)) -
                           time_compensation_;
  const double sum = time_ + increment;
  time_compensation_ = (sum - time_) - increment;
  if (selected.looping) {
    // Reduce first and compensate addition: finite large deltas cannot overflow,
    // and small batches do not accumulate an extra frame at a loop endpoint.
    time_ = std::fmod(sum, selected.duration);
    if (time_ == 0) time_compensation_ = 0;
  } else {
    if (sum >= selected.duration) {
      time_ = selected.duration;
      time_compensation_ = 0;
      completed = !completion_reported_;
      completion_reported_ = true;
    } else time_ = sum;
  }
  transition_time_ += std::min(elapsed, transition_seconds - transition_time_);
  refresh();
  return completed;
}
void CharacterPlayback::seek(double seconds) {
  const auto& selected = asset_->clips[clip_index_];
  time_ = normalizedTime(selected, seconds);
  time_compensation_ = 0;
  transition_time_ = transition_seconds;
  transition_source_.clear();
  // Seeking to the endpoint is inspection, without a deferred completion.
  completion_reported_ = !selected.looping && time_ >= selected.duration;
  refresh();
}
void CharacterPlayback::restart() { seek(0); }
void CharacterPlayback::refresh() {
  displayed_ = sampleCharacterPose(*asset_, clip(), time_);
  if (transition_time_ < transition_seconds) {
    const auto amount = static_cast<float>(transition_time_ / transition_seconds);
    for (std::size_t i = 0; i < displayed_.size(); ++i)
      displayed_[i] = blend(transition_source_[i], displayed_[i], amount);
  } else transition_source_.clear();
}
CharacterPose CharacterPlayback::pose() const { return evaluateCharacterPose(*asset_, displayed_); }
