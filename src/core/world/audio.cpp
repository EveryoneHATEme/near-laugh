#include "core/world/audio.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <set>

namespace {
constexpr std::array<AudioCatalogEntry, 5> catalog{
    {{"radio", "Radio"},
     {"phone-ring", "Telephone ring"},
     {"footsteps", "Footsteps"},
     {"phone-conversation", "Telephone conversation"},
     {"invitation", "Voice beyond the door"}}};

bool finite(WorldPosition p) noexcept {
  return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}
bool gainValid(float gain) noexcept {
  return std::isfinite(gain) && gain >= 0.0F && gain <= 1.0F;
}
}  // namespace

std::span<const AudioCatalogEntry> audioCatalog() noexcept { return catalog; }
bool audioCatalogContains(std::string_view id) noexcept {
  return std::any_of(catalog.begin(), catalog.end(),
                     [id](const auto& entry) { return entry.id == id; });
}
const AudioCueDefinition* findAudioCue(const LevelAudio& audio,
                                       std::string_view id) noexcept {
  const auto it = std::find_if(audio.cues.begin(), audio.cues.end(),
                               [id](const auto& cue) { return cue.id == id; });
  return it == audio.cues.end() ? nullptr : &*it;
}
bool audioRoomBoundsAreSafe(const AudioRoomDefinition& room) noexcept {
  const auto c = room.center;
  const auto e = room.half_extent;
  return finite(c) && finite({e.x, e.y, e.z}) && e.x > 0 && e.y > 0 &&
         e.z > 0 && finite({2 * e.x, 2 * e.y, 2 * e.z}) &&
         finite({c.x - e.x, c.y - e.y, c.z - e.z}) &&
         finite({c.x + e.x, c.y + e.y, c.z + e.z}) && c.x - e.x < c.x + e.x &&
         c.y - e.y < c.y + e.y && c.z - e.z < c.z + e.z;
}

std::vector<LevelDiagnostic> validateLevelAudio(
    const LevelAudio& audio, std::span<const DoorDefinition> doors,
    const std::filesystem::path& source_path) {
  std::vector<LevelDiagnostic> diagnostics;
  const auto error = [&](std::string path, std::string message) {
    diagnostics.push_back({LevelDiagnosticCategory::Validation,
                           source_path,
                           std::move(path),
                           std::move(message),
                           {}});
  };
  const auto checkIds = [&](const auto& records, std::size_t maximum,
                            std::string_view collection) {
    const auto path = "audio." + std::string(collection);
    if (records.size() > maximum)
      error(path, "exceeds the " + std::to_string(maximum) + "-record limit");
    std::set<std::string_view> ids;
    for (std::size_t i = 0; i < records.size(); ++i) {
      const auto& id = records[i].id;
      const auto field = path + "[" + std::to_string(i) + "].id";
      if (!levelEntryIdIsValid(id))
        error(field, "'" + id + "' must match [a-z][a-z0-9-]{0,63}");
      if (!ids.insert(id).second)
        error(field, "duplicate identifier '" + id + "'");
    }
  };
  checkIds(audio.cues, level_maximum_audio_cue_count, "cues");
  checkIds(audio.sources, level_maximum_audio_source_count, "sources");
  checkIds(audio.rooms, level_maximum_audio_room_count, "rooms");
  checkIds(audio.connections, level_maximum_audio_connection_count,
           "connections");
  for (std::size_t i = 0; i < audio.cues.size(); ++i) {
    const auto& cue = audio.cues[i];
    const auto p = "audio.cues[" + std::to_string(i) + "]";
    const auto label = "cue '" + cue.id + "': ";
    if (!audioCatalogContains(cue.clip))
      error(p + ".clip", label + "unknown clip '" + cue.clip + "'");
    if (cue.caption && !audioCatalogContains(*cue.caption))
      error(p + ".caption", label + "unknown caption '" + *cue.caption + "'");
    if (cue.kind != AudioCueKind::Dialogue &&
        cue.kind != AudioCueKind::Essential &&
        cue.kind != AudioCueKind::Ambience)
      error(p + ".kind", label + "unsupported kind");
    if (cue.kind != AudioCueKind::Ambience) {
      if (cue.loop)
        error(p + ".loop", label + "foreground cues must be finite one-shots");
      if (!cue.caption)
        error(p + ".caption", label + "foreground cues require captions");
    }
  }
  for (std::size_t i = 0; i < audio.sources.size(); ++i) {
    const auto& s = audio.sources[i];
    const auto p = "audio.sources[" + std::to_string(i) + "]";
    const auto label = "source '" + s.id + "': ";
    const auto* cue = findAudioCue(audio, s.cue);
    if (!cue) error(p + ".cue", label + "unknown cue '" + s.cue + "'");
    if (!finite(s.position)) error(p + ".position", label + "must be finite");
    if (!gainValid(s.gain))
      error(p + ".gain", label + "must be finite in [0,1]");
    if (!std::isfinite(s.near_distance) || !std::isfinite(s.far_distance) ||
        s.near_distance <= 0 || s.near_distance >= s.far_distance ||
        s.far_distance > 100)
      error(p + ".near_distance",
            label + "requires 0 < near_distance < far_distance <= 100");
    if (s.autoplay &&
        (!cue || cue->kind != AudioCueKind::Ambience || !cue->loop))
      error(p + ".autoplay", label + "only looping ambience may autoplay");
  }
  for (std::size_t i = 0; i < audio.rooms.size(); ++i) {
    const auto& room = audio.rooms[i];
    const auto p = "audio.rooms[" + std::to_string(i) + "]";
    if (!audioRoomBoundsAreSafe(room)) {
      error(p + ".bounds",
            "room '" + room.id +
                "': requires finite nonempty bounds and positive half extents");
      continue;
    }
    for (std::size_t j = 0; j < i; ++j) {
      const auto& other = audio.rooms[j];
      if (!audioRoomBoundsAreSafe(other)) continue;
      const auto a = room.center;
      const auto b = other.center;
      const auto e = room.half_extent;
      const auto f = other.half_extent;
      if (a.x - e.x < b.x + f.x && b.x - f.x < a.x + e.x &&
          a.y - e.y < b.y + f.y && b.y - f.y < a.y + e.y &&
          a.z - e.z < b.z + f.z && b.z - f.z < a.z + e.z)
        error(p + ".bounds",
              "room '" + room.id + "' overlaps room '" + other.id + "'");
    }
  }
  std::set<std::string_view> linked_doors;
  for (std::size_t i = 0; i < audio.connections.size(); ++i) {
    const auto& c = audio.connections[i];
    const auto p = "audio.connections[" + std::to_string(i) + "]";
    const auto label = "connection '" + c.id + "': ";
    const auto checkRoom = [&](const auto& id, std::string_view field) {
      if (id && std::none_of(audio.rooms.begin(), audio.rooms.end(),
                             [&](const auto& r) { return r.id == *id; }))
        error(p + "." + std::string(field),
              label + "unknown room '" + *id + "'");
    };
    checkRoom(c.room_a, "room_a");
    checkRoom(c.room_b, "room_b");
    if (c.room_a == c.room_b)
      error(p + ".room_b", label + "endpoints must differ");
    if (!gainValid(c.closed_gain) || !gainValid(c.open_gain) ||
        c.closed_gain > c.open_gain)
      error(p + ".closed_gain",
            label + "requires finite 0 <= closed_gain <= open_gain <= 1");
    if (c.door) {
      if (std::none_of(doors.begin(), doors.end(),
                       [&](const auto& d) { return d.id == *c.door; }))
        error(p + ".door", label + "unknown door '" + *c.door + "'");
      if (!linked_doors.insert(*c.door).second)
        error(p + ".door",
              label + "door '" + *c.door + "' already has a connection");
    }
  }
  return diagnostics;
}
