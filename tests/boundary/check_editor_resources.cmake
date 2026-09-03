foreach(REQUIRED_RESOURCE IN ITEMS
        "shaders/prototype_scene_vertex.spv"
        "shaders/prototype_scene_fragment.spv"
        "textures/prototype_floor.png"
        "textures/prototype_boundary.png"
        "textures/prototype_obstacle.png"
        "models/prototype_chair.glb"
        "levels/prototype.level.json")
    if(NOT EXISTS "${ROOT}/${REQUIRED_RESOURCE}")
        message(FATAL_ERROR
            "level_editor is missing executable-relative resource: "
            "${ROOT}/${REQUIRED_RESOURCE}")
    endif()
endforeach()
