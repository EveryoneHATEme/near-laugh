#ifndef CORE_RENDER_STATIC_MODEL_LOADER_HPP
#define CORE_RENDER_STATIC_MODEL_LOADER_HPP

#include <filesystem>
#include <vector>

#include "core/render/prototype_scene.hpp"
#include "core/world/prototype_level.hpp"

[[nodiscard]] std::vector<PositionColorVertex> loadStaticModelVertices(
    const std::filesystem::path& model_path,
    const PrototypeStaticProp& placement);

#endif
