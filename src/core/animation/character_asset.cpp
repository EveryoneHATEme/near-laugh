#include "core/animation/character_animation.hpp"

#include <cgltf.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace {
constexpr std::size_t max_file_bytes = 16U * 1024U * 1024U;
constexpr std::size_t max_decoded_bytes = 32U * 1024U * 1024U;
constexpr std::size_t max_nodes = 80;
constexpr std::size_t max_joints = 65;
constexpr float unit_tolerance = 0.00001F;

[[noreturn]] void fail(std::string_view source, const std::string& field) {
  throw std::runtime_error("Animated GLB " + std::string(source) + ": " + field);
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
glm::mat4 localMatrix(const CharacterLocalTransform& t) {
  return glm::translate(glm::mat4(1), glm::make_vec3(t.translation.data())) *
         glm::mat4_cast(quaternion(t.rotation));
}
bool nearIdentity(const glm::mat4& m) {
  for (int column = 0; column < 4; ++column)
    for (int row = 0; row < 4; ++row)
      if (!std::isfinite(m[column][row]) ||
          std::abs(m[column][row] - (column == row ? 1.0F : 0.0F)) > 0.0001F)
        return false;
  return true;
}
void addBudget(std::string_view source, std::size_t& bytes, std::size_t count,
               std::size_t stride, const std::string& field) {
  if (count > (max_decoded_bytes - bytes) / stride)
    fail(source, field + ": decoded data exceeds 32 MiB");
  bytes += count * stride;
}
// cgltf builds a parse tree before collection counts can be inspected. Bound
// those requests too, conservatively accounting cumulative allocations.
struct ParseBudget { std::size_t allocated{}; };
void* parseAllocate(void* user, cgltf_size size) {
  auto& budget = *static_cast<ParseBudget*>(user);
  if (size > max_decoded_bytes - budget.allocated) return nullptr;
  budget.allocated += size;
  return std::malloc(size);
}
void parseFree(void*, void* allocation) { std::free(allocation); }
struct ParsedDeleter {
  void operator()(cgltf_data* data) const noexcept { cgltf_free(data); }
};

void preflightJsonDepth(std::string_view source, std::span<const std::uint8_t> bytes) {
  // cgltf recursively skips JSON extras and unknown members. The prepared
  // profile has shallow JSON, so cap nesting before invoking that parser.
  if (bytes.size() < 20) fail(source, "GLB header: truncated file");
  std::uint32_t json_size = 0;
  std::memcpy(&json_size, bytes.data() + 12, sizeof(json_size));
  if (json_size > bytes.size() - 20) fail(source, "GLB JSON chunk: invalid byte range");
  unsigned depth = 0;
  bool quoted = false;
  bool escaped = false;
  for (const auto byte : bytes.subspan(20, json_size)) {
    if (quoted) {
      if (escaped) escaped = false;
      else if (byte == '\\') escaped = true;
      else if (byte == '"') quoted = false;
    } else if (byte == '"') quoted = true;
    else if (byte == '{' || byte == '[') {
      if (++depth > 32) fail(source, "GLB JSON: nesting exceeds bounded profile");
    } else if (byte == '}' || byte == ']') {
      if (depth == 0) fail(source, "GLB JSON: unbalanced nesting");
      --depth;
    }
  }
}

void preflight(std::string_view source, const cgltf_data& data) {
  if (data.file_type != cgltf_file_type_glb || !data.asset.version ||
      std::strcmp(data.asset.version, "2.0") != 0)
    fail(source, "asset: embedded binary glTF 2.0 required");
  if (data.extensions_required_count || data.extensions_used_count ||
      data.data_extensions_count || data.asset.extensions_count)
    fail(source, "asset.extensions: extensions are unsupported");
  if (data.scenes_count != 1 || data.scene != data.scenes ||
      data.scene->nodes_count == 0 || data.scene->nodes_count > max_nodes ||
      data.scene->extensions_count)
    fail(source, "scene: exactly one nonempty default scene required");
  if (!data.nodes_count || data.nodes_count > max_nodes ||
      data.meshes_count != 1 || data.skins_count != 1 ||
      !data.skins[0].joints_count || data.skins[0].joints_count > max_joints)
    fail(source, "nodes/mesh/skin: requires at most 80 nodes, 65 joints, one mesh and skin");
  if (data.animations_count != 3)
    fail(source, "animations: exactly idle, walk and interact required");
  if (data.cameras_count || data.lights_count || data.images_count ||
      data.textures_count || data.samplers_count || data.variants_count)
    fail(source, "asset: cameras, lights, textures and variants unsupported");
  if (data.buffers_count != 1 || data.buffers[0].uri ||
      data.buffers[0].extensions_count || !data.bin ||
      data.buffers[0].size > data.bin_size)
    fail(source, "buffer: requires one bounded embedded binary buffer");
  // These checks precede cgltf_validate's arithmetic and every accessor read,
  // including unused data and integer-overflow attempts.
  for (std::size_t i = 0; i < data.buffer_views_count; ++i) {
    const auto& view = data.buffer_views[i];
    if (view.extensions_count || view.has_meshopt_compression || view.data ||
        view.buffer != data.buffers || view.offset > data.buffers[0].size ||
        view.size > data.buffers[0].size - view.offset)
      fail(source, "bufferView " + std::to_string(i) + ": unsupported or overflowing byte range");
  }
  for (std::size_t i = 0; i < data.accessors_count; ++i) {
    const auto& a = data.accessors[i];
    const std::string field = "accessor " + std::to_string(i);
    const std::size_t element = cgltf_calc_size(a.type, a.component_type);
    const std::size_t component = cgltf_component_size(a.component_type);
    if (a.extensions_count || a.is_sparse || !a.buffer_view || !a.count ||
        !element || !component || a.stride < element || a.offset % component ||
        a.stride % component || a.buffer_view->offset % component)
      fail(source, field + ": unsupported, sparse, empty or misaligned storage");
    const std::size_t available = a.buffer_view->size;
    if (a.offset > available || element > available - a.offset ||
        a.count - 1 > (available - a.offset - element) / a.stride)
      fail(source, field + ": byte range exceeds buffer or overflows");
  }
}

const cgltf_accessor& accessor(std::string_view source, const cgltf_accessor* a,
                              cgltf_type type, const std::string& field,
                              bool floating = true,
                              bool normalized_unsigned = false) {
  const bool normalized_storage = a && normalized_unsigned && a->normalized &&
      (a->component_type == cgltf_component_type_r_8u ||
       a->component_type == cgltf_component_type_r_16u);
  if (!a || a->type != type ||
      (!normalized_storage && (a->normalized ||
        (floating && a->component_type != cgltf_component_type_r_32f))))
    fail(source, field + ": missing or unsupported accessor type");
  return *a;
}
template <std::size_t N>
std::array<float, N> readFloat(std::string_view source, const cgltf_accessor& a,
                              std::size_t i, const std::string& field) {
  std::array<float, N> value{};
  if (!cgltf_accessor_read_float(&a, i, value.data(), N) || !finite(value))
    fail(source, field + ": non-finite or unreadable accessor element " + std::to_string(i));
  return value;
}
const cgltf_accessor* attribute(std::string_view source,
                               const cgltf_primitive& primitive,
                               cgltf_attribute_type type,
                               const std::string& field) {
  const cgltf_accessor* result = nullptr;
  for (std::size_t i = 0; i < primitive.attributes_count; ++i) {
    const auto& a = primitive.attributes[i];
    if (a.type == type && a.index == 0) {
      if (result) fail(source, field + ": duplicate attribute");
      result = a.data;
    }
  }
  return result;
}

void readSkeleton(CharacterAsset& asset, const cgltf_data& data) {
  const auto source = std::string_view(asset.source);
  asset.nodes.resize(data.nodes_count);
  std::array<unsigned, max_nodes> incoming{};
  std::size_t mesh_nodes = 0;
  for (std::size_t i = 0; i < data.nodes_count; ++i) {
    const auto& n = data.nodes[i];
    const std::string field = "node " + std::to_string(i);
    if (n.has_matrix || n.extensions_count || n.has_mesh_gpu_instancing ||
        n.camera || n.light || n.weights_count || n.children_count > max_nodes)
      fail(source, field + ": unsupported node features");
    auto& output = asset.nodes[i];
    if (n.has_translation) std::copy_n(n.translation, 3, output.rest.translation.begin());
    if (n.has_rotation) std::copy_n(n.rotation, 4, output.rest.rotation.begin());
    if (!finite(output.rest.translation) || !finite(output.rest.rotation) ||
        std::abs(glm::length(quaternion(output.rest.rotation)) - 1) > unit_tolerance)
      fail(source, field + ": finite translation and unit rotation required");
    if (n.has_scale && (!finite(n.scale) || n.scale[0] != 1 ||
                       n.scale[1] != 1 || n.scale[2] != 1))
      fail(source, field + ".scale: unit scale required");
    output.rest.rotation = components(glm::normalize(quaternion(output.rest.rotation)));
    if (n.parent) output.parent = static_cast<std::int16_t>(n.parent - data.nodes);
    if (n.mesh || n.skin) {
      if (n.mesh != data.meshes || n.skin != data.skins || n.children_count)
        fail(source, field + ": only one skinned mesh leaf is supported");
      ++mesh_nodes;
    }
    for (std::size_t child = 0; child < n.children_count; ++child) {
      const auto index = static_cast<std::size_t>(n.children[child] - data.nodes);
      if (index >= data.nodes_count || n.children[child]->parent != &n ||
          ++incoming[index] != 1)
        fail(source, field + ".children: duplicate or inconsistent hierarchy");
    }
  }
  if (mesh_nodes != 1) fail(source, "mesh: exactly one skinned mesh node required");
  std::array<bool, max_nodes> visited{};
  const auto visit = [&](const auto& self, std::size_t index) -> void {
    if (index >= data.nodes_count || visited[index])
      fail(source, "node hierarchy: cycle or duplicate scene root");
    visited[index] = true;
    asset.evaluation_order.push_back(static_cast<std::uint16_t>(index));
    const auto& n = data.nodes[index];
    for (std::size_t i = 0; i < n.children_count; ++i)
      self(self, static_cast<std::size_t>(n.children[i] - data.nodes));
  };
  for (std::size_t i = 0; i < data.scene->nodes_count; ++i) {
    const auto* root = data.scene->nodes[i];
    if (root->parent) fail(source, "scene root: must not have a parent (cyclic hierarchy)");
    visit(visit, static_cast<std::size_t>(root - data.nodes));
  }
  if (asset.evaluation_order.size() != data.nodes_count)
    fail(source, "node hierarchy: unreachable or cyclic nodes");

  const auto& skin = data.skins[0];
  if (skin.extensions_count) fail(source, "skin.extensions: unsupported");
  const auto& binds = accessor(source, skin.inverse_bind_matrices, cgltf_type_mat4,
                                "skin.inverseBindMatrices");
  if (binds.count != skin.joints_count)
    fail(source, "skin.inverseBindMatrices: count differs from joints");
  std::array<bool, max_nodes> joint_seen{};
  std::int16_t skeleton_root = -1;
  for (std::size_t i = 0; i < skin.joints_count; ++i) {
    const auto index = static_cast<std::uint16_t>(skin.joints[i] - data.nodes);
    if (index >= data.nodes_count || joint_seen[index] || skin.joints[i]->mesh)
      fail(source, "skin.joints: duplicate, invalid or mesh joint");
    joint_seen[index] = true;
    asset.joint_nodes.push_back(index);
    auto ancestor = static_cast<std::int16_t>(index);
    while (asset.nodes[ancestor].parent >= 0) ancestor = asset.nodes[ancestor].parent;
    if (skeleton_root >= 0 && skeleton_root != ancestor)
      fail(source, "skin.joints: joints must share one skeleton root");
    skeleton_root = ancestor;
    asset.inverse_bind_matrices.push_back(readFloat<16>(source, binds, i,
                         "skin.inverseBindMatrices joint " + std::to_string(i)));
  }
  if (skin.skeleton) {
    const auto declared = static_cast<std::int16_t>(skin.skeleton - data.nodes);
    for (const auto joint : asset.joint_nodes) {
      auto n = static_cast<std::int16_t>(joint);
      while (n >= 0 && n != declared) n = asset.nodes[n].parent;
      if (n < 0) fail(source, "skin.skeleton: must be ancestor of every joint");
    }
  }
  std::array<glm::mat4, max_nodes> globals;
  for (const auto index : asset.evaluation_order) {
    const auto& n = asset.nodes[index];
    globals[index] = n.parent < 0 ? localMatrix(n.rest)
                                 : globals[n.parent] * localMatrix(n.rest);
    if (data.nodes[index].mesh && !nearIdentity(globals[index]))
      fail(source, "mesh node: identity global transform required");
  }
  for (std::size_t i = 0; i < asset.joint_nodes.size(); ++i) {
    const auto inverse = glm::make_mat4(asset.inverse_bind_matrices[i].data());
    const float determinant = glm::determinant(inverse);
    if (!std::isfinite(determinant) || std::abs(determinant) < 1e-8F ||
        !nearIdentity(globals[asset.joint_nodes[i]] * inverse))
      fail(source, "skin.inverseBindMatrices joint " + std::to_string(i) +
                       ": singular or inconsistent with bind pose");
  }
}

std::array<float, 4> materialColor(std::string_view source,
                                  const cgltf_material* material,
                                  const std::string& field) {
  if (!material) fail(source, field + ": explicit constant material required");
  const auto& m = *material;
  const auto& p = m.pbr_metallic_roughness;
  if (!m.has_pbr_metallic_roughness || p.metallic_factor != 0 ||
      p.roughness_factor != 1 || p.base_color_texture.texture ||
      p.metallic_roughness_texture.texture || m.normal_texture.texture ||
      m.occlusion_texture.texture || m.emissive_texture.texture ||
      m.emissive_factor[0] != 0 || m.emissive_factor[1] != 0 || m.emissive_factor[2] != 0 ||
      m.alpha_mode != cgltf_alpha_mode_opaque || !m.double_sided || m.unlit ||
      m.extensions_count || m.has_pbr_specular_glossiness || m.has_clearcoat ||
      m.has_transmission || m.has_volume || m.has_ior || m.has_specular ||
      m.has_sheen || m.has_emissive_strength || m.has_iridescence ||
      m.has_diffuse_transmission || m.has_anisotropy || m.has_dispersion)
    fail(source, field + ": requires two-sided constant OPAQUE diffuse material");
  std::array<float, 4> color;
  std::copy_n(p.base_color_factor, 4, color.begin());
  if (!finite(color) || std::any_of(color.begin(), color.end(),
                                    [](float c) { return c < 0 || c > 1; }))
    fail(source, field + ".baseColorFactor: finite [0,1] required");
  return color;
}

void readGeometry(CharacterAsset& asset, const cgltf_data& data, std::size_t& bytes) {
  const auto source = std::string_view(asset.source);
  const auto& mesh = data.meshes[0];
  if (!mesh.primitives_count || mesh.primitives_count > 2 || mesh.weights_count ||
      mesh.target_names_count || mesh.extensions_count || !data.materials_count ||
      data.materials_count > 2)
    fail(source, "mesh: requires one or two indexed primitives and constant materials");
  for (std::size_t i = 0; i < data.materials_count; ++i)
    (void)materialColor(source, &data.materials[i], "material " + std::to_string(i));
  for (std::size_t p = 0; p < mesh.primitives_count; ++p) {
    const auto& primitive = mesh.primitives[p];
    const std::string field = "primitive " + std::to_string(p);
    if (primitive.type != cgltf_primitive_type_triangles || primitive.targets_count ||
        primitive.has_draco_mesh_compression || primitive.extensions_count ||
        primitive.mappings_count || primitive.attributes_count != 5)
      fail(source, field + ": only indexed POSITION/NORMAL/TEXCOORD_0/JOINTS_0/WEIGHTS_0 triangles supported");
    const auto attr = [&](cgltf_attribute_type type, cgltf_type shape,
                          const char* name, bool floating = true,
                          bool normalized_unsigned = false) -> const cgltf_accessor& {
      return accessor(source, attribute(source, primitive, type, field + "." + name),
                      shape, field + "." + name, floating, normalized_unsigned);
    };
    const auto& position = attr(cgltf_attribute_type_position, cgltf_type_vec3, "POSITION");
    const auto& normal = attr(cgltf_attribute_type_normal, cgltf_type_vec3, "NORMAL");
    const auto& uv = attr(cgltf_attribute_type_texcoord, cgltf_type_vec2, "TEXCOORD_0", true, true);
    const auto& joints = attr(cgltf_attribute_type_joints, cgltf_type_vec4, "JOINTS_0", false);
    const auto& weights = attr(cgltf_attribute_type_weights, cgltf_type_vec4, "WEIGHTS_0", true, true);
    if (position.count != normal.count || position.count != uv.count ||
        position.count != joints.count || position.count != weights.count ||
        position.count > 10000 - asset.vertices.size())
      fail(source, field + ": attribute counts mismatch or exceed 10000 source vertices");
    if (joints.component_type != cgltf_component_type_r_8u &&
        joints.component_type != cgltf_component_type_r_16u)
      fail(source, field + ".JOINTS_0: unsigned byte or short required");
    const auto& indices = accessor(source, primitive.indices, cgltf_type_scalar,
                                    field + ".indices", false);
    if ((indices.component_type != cgltf_component_type_r_8u &&
         indices.component_type != cgltf_component_type_r_16u &&
         indices.component_type != cgltf_component_type_r_32u) ||
        indices.count % 3 || indices.count > 50000 - asset.indices.size())
      fail(source, field + ".indices: unsigned triangles bounded to 50000 required");
    addBudget(source, bytes, position.count, sizeof(CharacterVertex), field);
    addBudget(source, bytes, indices.count, sizeof(std::uint32_t), field);
    const auto first_vertex = static_cast<std::uint32_t>(asset.vertices.size());
    asset.vertices.reserve(asset.vertices.size() + position.count);
    asset.indices.reserve(asset.indices.size() + indices.count);
    const auto first_index = static_cast<std::uint32_t>(asset.indices.size());
    asset.primitives.push_back({first_index, static_cast<std::uint32_t>(indices.count),
                               materialColor(source, primitive.material, field + ".material")});
    for (std::size_t i = 0; i < position.count; ++i) {
      CharacterVertex vertex;
      vertex.position = readFloat<3>(source, position, i, field + ".POSITION");
      vertex.normal = readFloat<3>(source, normal, i, field + ".NORMAL");
      const double normal_length = std::hypot(static_cast<double>(vertex.normal[0]),
                                              vertex.normal[1], vertex.normal[2]);
      if (normal_length < 1e-8)
        fail(source, field + ".NORMAL: zero-length normal");
      for (float& component : vertex.normal)
        component = static_cast<float>(component / normal_length);
      vertex.uv = readFloat<2>(source, uv, i, field + ".TEXCOORD_0");
      vertex.weights = readFloat<4>(source, weights, i, field + ".WEIGHTS_0");
      std::array<cgltf_uint, 4> joint_indices;
      if (!cgltf_accessor_read_uint(&joints, i, joint_indices.data(), 4))
        fail(source, field + ".JOINTS_0: unreadable joints");
      float sum = 0;
      for (std::size_t j = 0; j < 4; ++j) {
        if (joint_indices[j] >= asset.joint_nodes.size() || vertex.weights[j] < 0)
          fail(source, field + ".JOINTS_0/WEIGHTS_0: invalid joint or negative weight at vertex " + std::to_string(i));
        vertex.joints[j] = static_cast<std::uint16_t>(joint_indices[j]);
        sum += vertex.weights[j];
      }
      if (!std::isfinite(sum) || sum <= 0 || std::abs(sum - 1) > unit_tolerance)
        fail(source, field + ".WEIGHTS_0: positive normalized weights required at vertex " + std::to_string(i));
      for (float& weight : vertex.weights) weight /= sum;
      asset.vertices.push_back(vertex);
    }
    for (std::size_t i = 0; i < indices.count; ++i) {
      const auto index = cgltf_accessor_read_index(&indices, i);
      if (index >= position.count)
        fail(source, field + ".indices: vertex reference out of range at index " + std::to_string(i));
      asset.indices.push_back(first_vertex + static_cast<std::uint32_t>(index));
    }
  }
}

void readClips(CharacterAsset& asset, const cgltf_data& data, std::size_t& bytes) {
  const auto source = std::string_view(asset.source);
  std::array<bool, max_nodes> joint{};
  for (const auto index : asset.joint_nodes) joint[index] = true;
  for (std::size_t clip_index = 0; clip_index < data.animations_count; ++clip_index) {
    const auto& animation = data.animations[clip_index];
    const std::string id = animation.name ? animation.name : "";
    const std::string field = "clip " + (id.empty() ? std::to_string(clip_index) : id);
    if ((id != "idle" && id != "walk" && id != "interact") ||
        std::any_of(asset.clips.begin(), asset.clips.end(),
                    [&](const auto& clip) { return clip.id == id; }))
      fail(source, field + ": unique idle/walk/interact names required");
    if (animation.extensions_count || !animation.channels_count ||
        animation.channels_count > max_joints * 2 ||
        animation.samplers_count > max_joints * 2)
      fail(source, field + ": bounded translation/rotation channels required");
    CharacterClip clip;
    clip.id = id;
    clip.looping = id != "interact";
    clip.channels.reserve(animation.channels_count);
    std::array<std::array<bool, 2>, max_nodes> used{};
    std::array<bool, max_joints * 2> used_samplers{};
    for (std::size_t c = 0; c < animation.channels_count; ++c) {
      const auto& channel = animation.channels[c];
      const std::string context = field + " channel " + std::to_string(c);
      if (!channel.target_node || !channel.sampler || channel.extensions_count ||
          (channel.target_path != cgltf_animation_path_type_translation &&
           channel.target_path != cgltf_animation_path_type_rotation))
        fail(source, context + ": target joint and translation/rotation required; scale unsupported");
      const auto node = static_cast<std::uint16_t>(channel.target_node - data.nodes);
      const bool rotation = channel.target_path == cgltf_animation_path_type_rotation;
      if (!joint[node] || used[node][rotation])
        fail(source, context + ": target is not a joint or channel is duplicated");
      used[node][rotation] = true;
      const auto& sampler = *channel.sampler;
      used_samplers[static_cast<std::size_t>(&sampler - animation.samplers)] = true;
      if (sampler.extensions_count || sampler.interpolation != cgltf_interpolation_type_linear)
        fail(source, context + ": LINEAR sampler required");
      const auto& times = accessor(source, sampler.input, cgltf_type_scalar, context + ".input");
      const auto& values = accessor(source, sampler.output,
                                     rotation ? cgltf_type_vec4 : cgltf_type_vec3,
                                     context + ".output");
      if (times.count > 256 || times.count != values.count)
        fail(source, context + ": matching time/value count bounded to 256 required");
      addBudget(source, bytes, times.count, sizeof(float) + sizeof(std::array<float, 4>), context);
      CharacterChannel output;
      output.node = node;
      output.rotation = rotation;
      output.times.reserve(times.count);
      output.values.reserve(times.count);
      bool root_joint = true;
      for (auto parent = asset.nodes[node].parent; parent >= 0;
           parent = asset.nodes[parent].parent)
        if (joint[parent]) root_joint = false;
      for (std::size_t key = 0; key < times.count; ++key) {
        const float time = readFloat<1>(source, times, key, context + ".input")[0];
        if (time < 0 || time > 10 || (!output.times.empty() && time <= output.times.back()))
          fail(source, context + ".input: times must be nonnegative, strictly increasing and <=10 seconds");
        std::array<float, 4> value{};
        if (rotation) {
          value = readFloat<4>(source, values, key, context + ".rotation");
          const auto q = quaternion(value);
          if (std::abs(glm::length(q) - 1) > unit_tolerance)
            fail(source, context + ".rotation: unit quaternion required");
          value = components(glm::normalize(q));
          if (root_joint) {
            const auto& rest = asset.nodes[node].rest.rotation;
            const float sign = glm::dot(quaternion(value), quaternion(rest)) < 0 ? -1.0F : 1.0F;
            for (std::size_t axis = 0; axis < 4; ++axis)
              if (std::abs(value[axis] * sign - rest[axis]) > unit_tolerance)
                fail(source, context + ": skeleton root rotation must remain constant at rest");
          }
        } else {
          const auto translation = readFloat<3>(source, values, key, context + ".translation");
          std::copy(translation.begin(), translation.end(), value.begin());
          if (root_joint)
            for (std::size_t axis = 0; axis < 3; ++axis)
              if (std::abs(value[axis] - asset.nodes[node].rest.translation[axis]) > unit_tolerance)
                fail(source, context + ": skeleton root translation must remain constant at rest");
        }
        output.times.push_back(time);
        output.values.push_back(value);
      }
      clip.duration = std::max(clip.duration, static_cast<double>(output.times.back()));
      clip.channels.push_back(std::move(output));
    }
    for (std::size_t sampler = 0; sampler < animation.samplers_count; ++sampler)
      if (!used_samplers[sampler])
        fail(source, field + " sampler " + std::to_string(sampler) + ": unused sampler unsupported");
    if (clip.duration <= 0 || clip.duration > 10)
      fail(source, field + ": positive duration <=10 seconds required");
    asset.clips.push_back(std::move(clip));
  }
}
}  // namespace

std::shared_ptr<const CharacterAsset> loadCharacterAsset(const std::filesystem::path& path) {
  const auto absolute = std::filesystem::absolute(path).lexically_normal();
  const std::string source = absolute.string();
  std::ifstream file(absolute, std::ios::binary | std::ios::ate);
  if (!file) fail(source, "file: missing or unreadable");
  const auto length = file.tellg();
  if (length <= 0 || length > static_cast<std::streamoff>(max_file_bytes))
    fail(source, "file: empty or exceeds 16 MiB");
  std::vector<std::uint8_t> encoded(static_cast<std::size_t>(length));
  file.seekg(0);
  if (!file.read(reinterpret_cast<char*>(encoded.data()), length))
    fail(source, "file: incomplete read");
  preflightJsonDepth(source, encoded);
  ParseBudget budget;
  cgltf_options options{};
  options.type = cgltf_file_type_glb;
  options.memory = {parseAllocate, parseFree, &budget};
  cgltf_data* parsed = nullptr;
  const auto parse_result = cgltf_parse(&options, encoded.data(), encoded.size(), &parsed);
  if (parse_result == cgltf_result_invalid_gltf)
    fail(source, "parse references/hierarchy/skin: invalid glTF object references or hierarchy");
  if (parse_result != cgltf_result_success)
    fail(source, "parse: invalid GLB or 32 MiB parse allocation budget exceeded (" +
                     std::to_string(static_cast<int>(parse_result)) + ")");
  std::unique_ptr<cgltf_data, ParsedDeleter> data(parsed);
  preflight(source, *data);
  // Borrow the verified embedded bytes directly. No URI loading path exists.
  data->buffers[0].data = const_cast<void*>(data->bin);
  auto asset = std::make_shared<CharacterAsset>();
  asset->source = source;
  std::uint64_t identity = 14695981039346656037ULL;
  for (const auto byte : encoded) identity = (identity ^ byte) * 1099511628211ULL;
  asset->skeleton_identity = identity ? identity : 1;
  std::size_t decoded_bytes = sizeof(CharacterAsset);
  addBudget(source, decoded_bytes, data->nodes_count,
            sizeof(CharacterNode) + sizeof(std::uint16_t), "nodes");
  addBudget(source, decoded_bytes, data->skins[0].joints_count,
            sizeof(AnimationMatrix) + sizeof(std::uint16_t), "skin");
  readSkeleton(*asset, *data);
  readGeometry(*asset, *data, decoded_bytes);
  readClips(*asset, *data, decoded_bytes);
  if (cgltf_validate(data.get()) != cgltf_result_success)
    fail(source, "accessor/scene: glTF validation failed");
  return asset;
}
