#ifndef CORE_AUDIO_TRANSMISSION_HPP
#define CORE_AUDIO_TRANSMISSION_HPP

#include "core/world/audio.hpp"

[[nodiscard]] std::optional<std::string_view> audioRoomAt(
    const LevelAudio& audio, WorldPosition position) noexcept;
[[nodiscard]] float audioDistanceGain(WorldPosition source,
                                      WorldPosition listener,
                                      float near_distance,
                                      float far_distance) noexcept;
// Empty angles select authored initial poses (editor audition). Otherwise the
// span must contain the current accepted angle for each door, in document
// order.
[[nodiscard]] float audioTransmission(const LevelAudio& audio,
                                      std::span<const DoorDefinition> doors,
                                      std::span<const float> accepted_angles,
                                      WorldPosition source,
                                      WorldPosition listener);

#endif
