#ifndef CORE_RENDER_SCENE_RESOURCES_HPP
#define CORE_RENDER_SCENE_RESOURCES_HPP

#include <memory>
#include <vector>

#include "core/render/immutable_mesh_buffer.hpp"
#include "core/render/sampled_texture.hpp"
#include "core/render/scene_assets.hpp"

class GraphicsPipeline;

// The static resources shared by game rendering and transactional editor
// preview.
class SceneResources {
 public:
  SceneResources(VkDevice device, VkPhysicalDevice physical_device,
                 VkQueue queue, std::uint32_t queue_family,
                 const PreparedSceneAssets& assets);
  void draw(VkCommandBuffer commands, const GraphicsPipeline& pipeline) const;
  void replaceWorld(const LevelDocument& document);
  [[nodiscard]] VkDescriptorSetLayout materialLayout() const noexcept;
  [[nodiscard]] VkDescriptorSet firstMaterial() const noexcept;
  [[nodiscard]] VkDescriptorSet obstacleMaterial() const;

 private:
  struct Draw {
    std::size_t material;
    std::unique_ptr<ImmutableMeshBuffer> mesh;
  };
  VkDevice device_;
  VkPhysicalDevice physical_device_;
  std::vector<std::string> material_ids_;
  std::vector<std::unique_ptr<SampledTexture>> materials_;
  std::vector<Draw> world_;
  std::vector<Draw> props_;
  std::optional<std::size_t> obstacle_;
};

#endif
