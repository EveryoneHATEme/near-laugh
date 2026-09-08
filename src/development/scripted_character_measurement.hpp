#ifndef DEVELOPMENT_SCRIPTED_CHARACTER_MEASUREMENT_HPP
#define DEVELOPMENT_SCRIPTED_CHARACTER_MEASUREMENT_HPP

#include "core/world/prototype_level.hpp"

// P07a geometry, lights, initial doors and camera remain identical. The P07a
// visual-only placements are replaced by short, collision-valid lanes clear
// of the chair, bed and every player entry. No packaged level is rewritten.
inline LevelDocument scriptedCharacterMeasurement(
    const std::filesystem::path& root, unsigned count) {
  if (count != 0 && count != 1 && count != 4)
    throw std::invalid_argument(
        "Measurement requires zero, one or four actors");
  auto loaded =
      loadLevelDocument(root / "levels/interior-lighting-capacity.level.json");
  if (!loaded)
    throw std::runtime_error(formatLevelDiagnostics(loaded.diagnostics));
  auto d = std::move(*loaded.document);
  d.characters = {};
  if (count) {
    d.audio.cues.push_back({"measurement-step",
                            "character-footstep",
                            {},
                            AudioCueKind::Ambience,
                            false,
                            true});
    d.audio.cues.push_back({"measurement-touch", "character-interaction",
                            "character-interaction", AudioCueKind::Essential,
                            false, true});
  }
  for (unsigned i = 0; i < count; ++i) {
    const auto id = "measurement-" + std::to_string(i);
    const float x = i % 2 == 0 ? -4.5F : -2.2F;
    const float z = i < 2 ? 2.95F : 1.6F;
    d.characters.marks.push_back({id + "-a", {x, 3, z}, 0});
    d.characters.marks.push_back({id + "-b", {x, 3, z + .75F}, 0});
    d.characters.actors.push_back({id, "test-mannequin", id + "-a", id + "-out",
                                   1, id + "-step", id + "-touch"});
    d.characters.routes.push_back({id + "-out", id, {id + "-b"}, "interact"});
    d.characters.routes.push_back({id + "-back", id, {id + "-a"}, {}});
    d.audio.sources.push_back({id + "-step", "measurement-step"});
    d.audio.sources.push_back({id + "-touch", "measurement-touch"});
  }
  return d;
}

#endif
