#include "core/audio/transmission.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

#include "core/world/door.hpp"

std::optional<std::string_view> audioRoomAt(const LevelAudio& audio,
                                            WorldPosition p) noexcept {
  std::optional<std::string_view> result;
  for (const auto& r : audio.rooms) {
    const auto c = r.center;
    const auto e = r.half_extent;
    if (p.x >= c.x - e.x && p.x < c.x + e.x && p.y >= c.y - e.y &&
        p.y < c.y + e.y && p.z >= c.z - e.z && p.z < c.z + e.z &&
        (!result || r.id < *result))
      result = r.id;
  }
  return result;
}
float audioDistanceGain(WorldPosition s, WorldPosition l, float near_distance,
                        float far_distance) noexcept {
  const double x = double(s.x) - l.x, y = double(s.y) - l.y,
               z = double(s.z) - l.z;
  const double distance = std::sqrt(x * x + y * y + z * z);
  return static_cast<float>(std::clamp(
      (far_distance - distance) / (far_distance - near_distance), 0.0, 1.0));
}
float audioTransmission(const LevelAudio& audio,
                        std::span<const DoorDefinition> doors,
                        std::span<const float> angles, WorldPosition source,
                        WorldPosition listener) {
  if (audio.rooms.size() > 32 || audio.connections.size() > 64 ||
      (!angles.empty() && angles.size() != doors.size()))
    throw std::invalid_argument(
        "audio transmission requires bounded rooms/connections and all "
        "accepted door angles");
  const auto roomIndex =
      [&](std::optional<std::string_view> id) -> std::size_t {
    if (!id) return audio.rooms.size();
    for (std::size_t i = 0; i < audio.rooms.size(); ++i)
      if (audio.rooms[i].id == *id) return i;
    throw std::invalid_argument("audio connection references missing room '" +
                                std::string(*id) + "'");
  };
  const auto from = roomIndex(audioRoomAt(audio, source));
  const auto to = roomIndex(audioRoomAt(audio, listener));
  if (from == to) return 1;
  struct Edge {
    std::size_t a, b;
    float gain;
  };
  std::array<Edge, 64> edges{};
  std::size_t edge_count = 0;
  for (const auto& c : audio.connections) {
    float gain = c.open_gain;
    if (c.door) {
      const auto found =
          std::find_if(doors.begin(), doors.end(),
                       [&](const auto& d) { return d.id == *c.door; });
      if (found == doors.end())
        throw std::invalid_argument(
            "audio connection references missing door '" + *c.door + "'");
      const float angle = angles.empty() ? doorInitialAngle(*found)
                                         : angles[found - doors.begin()];
      if (!std::isfinite(angle) || found->open_angle_degrees == 0)
        throw std::invalid_argument("invalid accepted audio door angle");
      const float openness =
          std::clamp(std::abs(angle / found->open_angle_degrees), 0.0F, 1.0F);
      gain = std::lerp(c.closed_gain, c.open_gain, openness);
    }
    if (!std::isfinite(gain) || gain < 0 || gain > 1)
      throw std::invalid_argument("invalid audio transmission gain");
    edges[edge_count++] = {
        roomIndex(c.room_a ? std::optional<std::string_view>(*c.room_a)
                           : std::nullopt),
        roomIndex(c.room_b ? std::optional<std::string_view>(*c.room_b)
                           : std::nullopt),
        gain};
  }
  std::array<float, 33> strongest;
  strongest.fill(-1);
  std::array<bool, 33> visited{};
  strongest[from] = 1;
  const auto count = audio.rooms.size() + 1;
  for (std::size_t step = 0; step < count; ++step) {
    std::size_t current = count;
    for (std::size_t i = 0; i < count; ++i)
      if (!visited[i] && strongest[i] >= 0 &&
          (current == count || strongest[i] > strongest[current]))
        current = i;
    if (current == count) break;
    if (current == to) return strongest[current];
    visited[current] = true;
    for (std::size_t i = 0; i < edge_count; ++i) {
      const auto& e = edges[i];
      if (e.a != current && e.b != current) continue;
      const auto next = e.a == current ? e.b : e.a;
      strongest[next] = std::max(strongest[next], strongest[current] * e.gain);
    }
  }
  return 0.1F;
}
