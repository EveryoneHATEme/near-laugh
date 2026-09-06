#include "core/render/static_model_loader.hpp"

#include <cgltf.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <numbers>
#include <stdexcept>
#include <string>

#include "core/resources/shader_provider.hpp"

namespace {
using Matrix4 = std::array<float, 16>;

[[noreturn]] void fail(const std::filesystem::path& path,
                       const std::string& reason) {
  throw std::runtime_error("Static GLB " + path.string() + ": " + reason);
}

const char* resultName(cgltf_result result) noexcept {
  switch (result) {
    case cgltf_result_success:
      return "success";
    case cgltf_result_data_too_short:
      return "data too short";
    case cgltf_result_unknown_format:
      return "unknown format";
    case cgltf_result_invalid_json:
      return "invalid JSON";
    case cgltf_result_invalid_gltf:
      return "invalid glTF";
    case cgltf_result_invalid_options:
      return "invalid parser options";
    case cgltf_result_file_not_found:
      return "file not found";
    case cgltf_result_io_error:
      return "I/O error";
    case cgltf_result_out_of_memory:
      return "out of memory";
    case cgltf_result_legacy_gltf:
      return "legacy glTF";
    case cgltf_result_max_enum:
      break;
  }
  return "unknown parser error";
}

struct CgltfDeleter {
  void operator()(cgltf_data* data) const noexcept { cgltf_free(data); }
};
using CgltfData = std::unique_ptr<cgltf_data, CgltfDeleter>;

void requireBoundedStructure(const std::filesystem::path& path,
                             const cgltf_data& data) {
  if (data.file_type != cgltf_file_type_glb) {
    fail(path, "the asset is not a binary glTF 2.0 file");
  }
  if (data.extensions_required_count != 0 || data.extensions_used_count != 0) {
    fail(path, "required extensions are unsupported");
  }
  if (data.buffers_count != 1 || data.buffers[0].uri != nullptr) {
    fail(path,
         "geometry must use one embedded GLB buffer without an external URI");
  }
  for (cgltf_size index = 0; index < data.buffer_views_count; ++index) {
    if (data.buffer_views[index].has_meshopt_compression) {
      fail(path, "compressed buffer views are unsupported");
    }
    const auto& view = data.buffer_views[index];
    if (view.buffer != &data.buffers[0] || view.offset > view.buffer->size ||
        view.size > view.buffer->size - view.offset)
      fail(path, "buffer-view range exceeds the embedded buffer");
  }
  if (data.buffers[0].size > data.bin_size)
    fail(path, "embedded buffer length exceeds the GLB binary chunk");
  if (data.scenes_count != 1 || data.scene == nullptr ||
      data.scene != &data.scenes[0] || data.scene->nodes_count != 1) {
    fail(path, "exactly one default scene with one root node is required");
  }
  const cgltf_node& node = *data.scene->nodes[0];
  if (node.parent != nullptr || node.children_count != 0) {
    fail(path, "child nodes and parented model nodes are unsupported");
  }
  if (data.nodes_count != 1 || data.scene->nodes[0] != &data.nodes[0]) {
    fail(path, "exactly one scene-owned node is required");
  }
  if (data.meshes_count != 1 || node.mesh != &data.meshes[0]) {
    fail(path, "exactly one mesh on the root node is required");
  }
  if (node.skin != nullptr || data.skins_count != 0) {
    fail(path, "skins are unsupported");
  }
  if (data.animations_count != 0) {
    fail(path, "animations are unsupported");
  }
  if (node.camera != nullptr || data.cameras_count != 0 ||
      node.light != nullptr || data.lights_count != 0) {
    fail(path, "cameras and lights are unsupported");
  }
  if (node.weights_count != 0 || data.meshes[0].weights_count != 0 ||
      data.meshes[0].target_names_count != 0) {
    fail(path, "morph weights and targets are unsupported");
  }
  const cgltf_mesh& mesh = data.meshes[0];
  if (mesh.primitives_count != 1) {
    fail(path, "exactly one mesh primitive is required");
  }
  const cgltf_primitive& primitive = mesh.primitives[0];
  if (primitive.type != cgltf_primitive_type_triangles) {
    fail(path, "the primitive must use triangle-list mode");
  }
  if (primitive.targets_count != 0) {
    fail(path, "morph targets are unsupported");
  }
  if (primitive.has_draco_mesh_compression) {
    fail(path, "compressed primitives are unsupported");
  }
}

SceneMaterialData readMaterial(const std::filesystem::path& path,
                               const cgltf_data& data,
                               const cgltf_primitive& primitive) {
  SceneMaterialData result;
  if (data.materials_count > 1 || data.images_count > 1 ||
      data.textures_count > 1 || data.samplers_count > 1)
    fail(path,
         "material profile allows one material and one embedded base-color "
         "image");
  const auto* material = primitive.material;
  if (material == nullptr) {
    if (data.materials_count || data.images_count || data.textures_count ||
        data.samplers_count)
      fail(path,
           "unused material or texture resources are outside the profile");
    return result;
  }
  const std::string context =
      std::string{"material "} + (material->name ? material->name : "0");
  const auto& pbr = material->pbr_metallic_roughness;
  if (!material->has_pbr_metallic_roughness || pbr.metallic_factor != 0.0F ||
      pbr.roughness_factor != 1.0F || pbr.metallic_roughness_texture.texture ||
      material->normal_texture.texture || material->occlusion_texture.texture ||
      material->emissive_texture.texture || material->emissive_factor[0] != 0 ||
      material->emissive_factor[1] != 0 || material->emissive_factor[2] != 0 ||
      material->double_sided || material->unlit || material->extensions_count ||
      material->has_pbr_specular_glossiness || material->has_clearcoat ||
      material->has_transmission || material->has_volume || material->has_ior ||
      material->has_specular || material->has_sheen ||
      material->has_emissive_strength || material->has_iridescence ||
      material->has_diffuse_transmission || material->has_anisotropy ||
      material->has_dispersion)
    fail(path, context +
                   ": unsupported shading inputs; use the prepared base-color "
                   "profile");
  for (std::size_t i = 0; i < 4; ++i) {
    const float value = pbr.base_color_factor[i];
    if (!std::isfinite(value) || value < 0 || value > 1)
      fail(path, context + ": base-color factor must be finite in [0,1]");
    result.base_color_factor[i] = value;
  }
  if (material->alpha_mode != cgltf_alpha_mode_opaque &&
      material->alpha_mode != cgltf_alpha_mode_mask)
    fail(path,
         context + ": BLEND and unsupported alpha modes are not supported");
  result.alpha_mask = material->alpha_mode == cgltf_alpha_mode_mask;
  if (!std::isfinite(material->alpha_cutoff) || material->alpha_cutoff < 0 ||
      material->alpha_cutoff > 1)
    fail(path, context + ": alpha cutoff must be finite in [0,1]");
  result.alpha_cutoff = material->alpha_cutoff;
  const auto& texture_view = pbr.base_color_texture;
  if (texture_view.has_transform || texture_view.texcoord != 0)
    fail(path, context + ": only untransformed TEXCOORD_0 is supported");
  const auto* texture = texture_view.texture;
  if (texture == nullptr) {
    if (data.images_count || data.textures_count || data.samplers_count)
      fail(path, context + ": unused texture resources are unsupported");
    return result;
  }
  if (texture->has_basisu || texture->has_webp || texture->extensions_count)
    fail(path,
         context +
             ": compressed or extended base-color textures are unsupported");
  if (const auto* sampler = texture->sampler) {
    const auto mag = sampler->mag_filter == cgltf_filter_type_undefined
                         ? cgltf_filter_type_linear
                         : sampler->mag_filter;
    const auto min = sampler->min_filter == cgltf_filter_type_undefined
                         ? cgltf_filter_type_linear_mipmap_linear
                         : sampler->min_filter;
    const bool nearest = mag == cgltf_filter_type_nearest &&
                         min == cgltf_filter_type_nearest_mipmap_nearest;
    const bool linear = mag == cgltf_filter_type_linear &&
                        min == cgltf_filter_type_linear_mipmap_linear;
    if ((!nearest && !linear) || sampler->wrap_s != cgltf_wrap_mode_repeat ||
        sampler->wrap_t != cgltf_wrap_mode_repeat || sampler->extensions_count)
      fail(path, context +
                     ": unsupported sampler; use repeat nearest/nearest-mip or "
                     "linear/trilinear");
    result.nearest = nearest;
  }
  const auto* image = texture->image;
  if (image == nullptr || image->uri || !image->mime_type ||
      std::strcmp(image->mime_type, "image/png") != 0 || !image->buffer_view ||
      image->extensions_count)
    fail(path, context + ": base color must use an embedded PNG image");
  const auto& view = *image->buffer_view;
  const auto* bytes = static_cast<const std::uint8_t*>(view.buffer->data);
  result.image =
      decodePngRgba({bytes + view.offset, view.size},
                    path.string() + " " + context + " base-color image");
  return result;
}

const cgltf_accessor* findAttribute(const cgltf_primitive& primitive,
                                    cgltf_attribute_type type, int index) {
  const cgltf_accessor* found = nullptr;
  for (cgltf_size attribute_index = 0;
       attribute_index < primitive.attributes_count; ++attribute_index) {
    const cgltf_attribute& attribute = primitive.attributes[attribute_index];
    if (attribute.type == type && attribute.index == index) {
      if (found != nullptr) {
        return nullptr;
      }
      found = attribute.data;
    }
  }
  return found;
}

void requireAccessor(const std::filesystem::path& path,
                     const cgltf_accessor* accessor, cgltf_type type,
                     const char* name) {
  if (accessor == nullptr) {
    fail(path, std::string{"missing or duplicate "} + name + " accessor");
  }
  if (accessor->type != type) {
    fail(path, std::string{name} + " accessor has the wrong element type");
  }
  if (accessor->is_sparse) {
    fail(path, std::string{name} + " sparse accessors are unsupported");
  }
  if (accessor->buffer_view == nullptr || accessor->count == 0) {
    fail(path, std::string{name} + " accessor must contain embedded data");
  }
}

Matrix4 placementMatrix(const PrototypeStaticProp& placement) noexcept {
  const float yaw = placement.yaw_degrees * std::numbers::pi_v<float> / 180.0F;
  const float cosine = std::cos(yaw) * placement.uniform_scale;
  const float sine = std::sin(yaw) * placement.uniform_scale;
  return {cosine,
          0.0F,
          -sine,
          0.0F,
          0.0F,
          placement.uniform_scale,
          0.0F,
          0.0F,
          sine,
          0.0F,
          cosine,
          0.0F,
          placement.translation.x,
          placement.translation.y,
          placement.translation.z,
          1.0F};
}

std::array<float, 9> normalMatrix(const std::filesystem::path& path,
                                  const Matrix4& matrix) {
  const float a = matrix[0];
  const float b = matrix[4];
  const float c = matrix[8];
  const float d = matrix[1];
  const float e = matrix[5];
  const float f = matrix[9];
  const float g = matrix[2];
  const float h = matrix[6];
  const float i = matrix[10];
  const std::array<float, 9> cofactors = {
      e * i - f * h, f * g - d * i, d * h - e * g, c * h - b * i, a * i - c * g,
      b * g - a * h, b * f - c * e, c * d - a * f, a * e - b * d};
  const float determinant =
      a * cofactors[0] + b * cofactors[1] + c * cofactors[2];
  if (!std::isfinite(determinant) || std::abs(determinant) < 1.0e-8F) {
    fail(path, "the combined node and placement transform is singular");
  }
  std::array<float, 9> result = cofactors;
  for (float& component : result) {
    component /= determinant;
    if (!std::isfinite(component)) {
      fail(path, "the combined normal transform is non-finite");
    }
  }
  return result;
}

std::array<float, 3> transformPosition(const std::filesystem::path& path,
                                       const Matrix4& matrix,
                                       const std::array<float, 3>& value) {
  std::array<float, 3> result{};
  for (std::size_t row = 0; row < 3; ++row) {
    result[row] = matrix[row] * value[0] + matrix[4 + row] * value[1] +
                  matrix[8 + row] * value[2] + matrix[12 + row];
  }
  if (!std::isfinite(result[0]) || !std::isfinite(result[1]) ||
      !std::isfinite(result[2])) {
    fail(path, "a transformed position is non-finite");
  }
  return result;
}

std::array<float, 3> transformNormal(const std::filesystem::path& path,
                                     const std::array<float, 9>& matrix,
                                     const std::array<float, 3>& value) {
  std::array<float, 3> result{};
  for (std::size_t row = 0; row < 3; ++row) {
    result[row] = matrix[row * 3] * value[0] + matrix[row * 3 + 1] * value[1] +
                  matrix[row * 3 + 2] * value[2];
  }
  const float length = std::sqrt(result[0] * result[0] + result[1] * result[1] +
                                 result[2] * result[2]);
  if (!std::isfinite(length) || length < 1.0e-8F) {
    fail(path, "a transformed normal is non-finite or has zero length");
  }
  for (float& component : result) {
    component /= length;
  }
  return result;
}
}  // namespace

StaticModelData loadStaticModel(const std::filesystem::path& model_path) {
  const std::filesystem::path path =
      std::filesystem::absolute(model_path).lexically_normal();
  std::error_code file_error;
  const auto file_size = std::filesystem::file_size(path, file_error);
  if (file_error)
    fail(path, "model is missing or unreadable: " + file_error.message());
  if (file_size > 16U * 1024U * 1024U) fail(path, "model exceeds 16 MiB");
  const auto encoded = readBinaryFile(path);

  const cgltf_options options{};
  cgltf_data* parsed = nullptr;
  const cgltf_result parse_result =
      cgltf_parse(&options, encoded.data(), encoded.size(), &parsed);
  if (parse_result != cgltf_result_success) {
    fail(path, std::string{"parse failed: "} + resultName(parse_result));
  }
  CgltfData data{parsed};
  requireBoundedStructure(path, *data);

  const cgltf_result load_result =
      cgltf_load_buffers(&options, data.get(), nullptr);
  if (load_result != cgltf_result_success) {
    fail(path, std::string{"embedded buffer load failed: "} +
                   resultName(load_result));
  }
  const cgltf_primitive& primitive = data->meshes[0].primitives[0];
  const cgltf_accessor* positions =
      findAttribute(primitive, cgltf_attribute_type_position, 0);
  const cgltf_accessor* normals =
      findAttribute(primitive, cgltf_attribute_type_normal, 0);
  const cgltf_accessor* texture_coordinates =
      findAttribute(primitive, cgltf_attribute_type_texcoord, 0);
  requireAccessor(path, positions, cgltf_type_vec3, "POSITION");
  requireAccessor(path, normals, cgltf_type_vec3, "NORMAL");
  requireAccessor(path, texture_coordinates, cgltf_type_vec2, "TEXCOORD_0");
  if (primitive.attributes_count != 3)
    fail(path, "only POSITION, NORMAL and TEXCOORD_0 attributes are supported");
  if (positions->count != normals->count ||
      positions->count != texture_coordinates->count) {
    fail(path, "POSITION, NORMAL, and TEXCOORD_0 counts do not match");
  }

  const cgltf_accessor* indices = primitive.indices;
  const std::size_t output_count =
      indices == nullptr ? positions->count : indices->count;
  if (indices != nullptr) {
    requireAccessor(path, indices, cgltf_type_scalar, "index");
    if (indices->normalized ||
        (indices->component_type != cgltf_component_type_r_8u &&
         indices->component_type != cgltf_component_type_r_16u &&
         indices->component_type != cgltf_component_type_r_32u)) {
      fail(path, "indices must use unsigned 8-bit, 16-bit, or 32-bit values");
    }
  }
  if (output_count == 0 || output_count % 3 != 0 || output_count > 300000) {
    fail(path, "triangle vertex count is empty, incomplete, or too large");
  }
  // cgltf's general validator performs offset + stride * count arithmetic.
  // Bound each range by subtraction/division first, including unused accessors,
  // so crafted JSON integers cannot wrap before the library inspects them.
  for (cgltf_size index = 0; index < data->accessors_count; ++index) {
    const auto& accessor = data->accessors[index];
    const auto element_size =
        cgltf_calc_size(accessor.type, accessor.component_type);
    if (accessor.is_sparse || !accessor.buffer_view || !accessor.count ||
        !element_size || accessor.stride < element_size)
      fail(path, "unsupported or empty accessor storage");
    const auto available = accessor.buffer_view->size;
    if (accessor.offset > available ||
        element_size > available - accessor.offset ||
        accessor.count - 1 >
            (available - accessor.offset - element_size) / accessor.stride)
      fail(path, "accessor byte range is invalid or too large");
  }
  const cgltf_result validation_result = cgltf_validate(data.get());
  if (validation_result != cgltf_result_success) {
    fail(path, std::string{"glTF validation failed: "} +
                   resultName(validation_result));
  }

  Matrix4 node_matrix{};
  cgltf_node_transform_local(&data->nodes[0], node_matrix.data());
  for (float component : node_matrix) {
    if (!std::isfinite(component)) {
      fail(path, "the root-node transform contains non-finite data");
    }
  }
  const Matrix4 combined = node_matrix;
  const std::array<float, 9> normals_matrix = normalMatrix(path, combined);

  StaticModelData result;
  result.material = readMaterial(path, *data, primitive);
  auto& vertices = result.vertices;
  vertices.reserve(output_count);
  for (std::size_t output_index = 0; output_index < output_count;
       ++output_index) {
    const std::size_t source_index =
        indices == nullptr ? output_index
                           : cgltf_accessor_read_index(indices, output_index);
    if (source_index >= positions->count) {
      fail(path, "an index references a vertex outside the attribute arrays");
    }
    std::array<float, 3> position{};
    std::array<float, 3> normal{};
    std::array<float, 2> uv{};
    if (!cgltf_accessor_read_float(positions, source_index, position.data(),
                                   position.size()) ||
        !cgltf_accessor_read_float(normals, source_index, normal.data(),
                                   normal.size()) ||
        !cgltf_accessor_read_float(texture_coordinates, source_index, uv.data(),
                                   uv.size())) {
      fail(path, "an accessor element could not be decoded");
    }
    if (!std::isfinite(position[0]) || !std::isfinite(position[1]) ||
        !std::isfinite(position[2]) || !std::isfinite(normal[0]) ||
        !std::isfinite(normal[1]) || !std::isfinite(normal[2]) ||
        !std::isfinite(uv[0]) || !std::isfinite(uv[1])) {
      fail(path, "geometry contains non-finite position, normal, or UV data");
    }
    position = transformPosition(path, combined, position);
    normal = transformNormal(path, normals_matrix, normal);
    vertices.push_back({{position[0], position[1], position[2]},
                        {255, 255, 255, 255},
                        {normal[0], normal[1], normal[2]},
                        {uv[0], uv[1]},
                        0});
    if (vertices.size() == 1) {
      result.minimum = result.maximum = {position[0], position[1], position[2]};
    } else {
      result.minimum.x = std::min(result.minimum.x, position[0]);
      result.minimum.y = std::min(result.minimum.y, position[1]);
      result.minimum.z = std::min(result.minimum.z, position[2]);
      result.maximum.x = std::max(result.maximum.x, position[0]);
      result.maximum.y = std::max(result.maximum.y, position[1]);
      result.maximum.z = std::max(result.maximum.z, position[2]);
    }
  }
  return result;
}

std::vector<PositionColorVertex> placeStaticModelVertices(
    const StaticModelData& model, const PrototypeStaticProp& placement) {
  if (!std::isfinite(placement.translation.x) ||
      !std::isfinite(placement.translation.y) ||
      !std::isfinite(placement.translation.z) ||
      !std::isfinite(placement.yaw_degrees) ||
      !std::isfinite(placement.uniform_scale) || placement.uniform_scale <= 0)
    throw std::runtime_error("Static model placement transform is invalid");
  const auto matrix = placementMatrix(placement);
  const auto normals = normalMatrix({}, matrix);
  auto vertices = model.vertices;
  for (auto& vertex : vertices) {
    const auto p = transformPosition(
        {}, matrix,
        {vertex.position[0], vertex.position[1], vertex.position[2]});
    const auto n = transformNormal(
        {}, normals, {vertex.normal[0], vertex.normal[1], vertex.normal[2]});
    std::copy(p.begin(), p.end(), vertex.position);
    std::copy(n.begin(), n.end(), vertex.normal);
  }
  return vertices;
}

std::vector<PositionColorVertex> loadStaticModelVertices(
    const std::filesystem::path& model_path,
    const PrototypeStaticProp& placement) {
  return placeStaticModelVertices(loadStaticModel(model_path), placement);
}
