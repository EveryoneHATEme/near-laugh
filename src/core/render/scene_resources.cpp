#include "core/render/scene_resources.hpp"

#include <algorithm>
#include <stdexcept>

#include "core/render/graphics_pipeline.hpp"
#include "core/world/scene_assets.hpp"

SceneResources::SceneResources(VkDevice device,
                               VkPhysicalDevice physical_device, VkQueue queue,
                               std::uint32_t queue_family,
                               const PreparedSceneAssets& assets)
    : device_(device),
      physical_device_(physical_device),
      obstacle_(assets.obstacle_material) {
  for (const auto& material : assets.materials) {
    material_ids_.push_back(material.id);
    materials_.push_back(std::make_unique<SampledTexture>(
        device, physical_device, queue, queue_family, material.data));
  }
  for (const auto& batch : assets.world)
    if (!batch.vertices.empty())
      world_.push_back({batch.material,
                        std::make_unique<ImmutableMeshBuffer>(
                            device, physical_device, batch.vertices, "world")});
  for (const auto& batch : assets.props)
    if (!batch.vertices.empty())
      props_.push_back({batch.material,
                        std::make_unique<ImmutableMeshBuffer>(
                            device, physical_device, batch.vertices, "prop")});
}

void SceneResources::draw(VkCommandBuffer commands,
                          const GraphicsPipeline& pipeline) const {
  for (const auto* draws : {&world_, &props_})
    for (const auto& draw : *draws) {
      pipeline.bindMaterial(commands,
                            materials_.at(draw.material)->descriptorSet());
      draw.mesh->bindAndDraw(commands);
    }
}

VkDescriptorSetLayout SceneResources::materialLayout() const noexcept {
  return materials_.front()->descriptorSetLayout();
}
VkDescriptorSet SceneResources::firstMaterial() const noexcept {
  return materials_.front()->descriptorSet();
}
VkDescriptorSet SceneResources::obstacleMaterial() const {
  if (!obstacle_)
    throw std::runtime_error("Scene has no changing opaque material");
  return materials_.at(*obstacle_)->descriptorSet();
}

void SceneResources::replaceWorld(const LevelDocument& document) {
  const auto vertices = buildPrototypeSceneVertices(
      document.terrain, document.solids, document.light_switch);
  const auto catalog = structuralMaterials();
  std::vector<Draw> replacement;
  for (std::size_t role = 0; role < catalog.size(); ++role) {
    std::vector<PositionColorVertex> group;
    for (const auto& vertex : vertices)
      if (vertex.texture_layer == role) group.push_back(vertex);
    if (group.empty()) continue;
    auto material =
        std::find(material_ids_.begin(), material_ids_.end(), catalog[role].id);
    if (material == material_ids_.end())
      throw std::runtime_error(
          "Terrain replacement changes material resources; replace the "
          "document");
    replacement.push_back(
        {static_cast<std::size_t>(material - material_ids_.begin()),
         std::make_unique<ImmutableMeshBuffer>(device_, physical_device_, group,
                                               "world")});
  }
  world_ = std::move(replacement);
}
