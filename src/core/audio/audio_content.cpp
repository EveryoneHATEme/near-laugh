#include "core/audio/audio_content.hpp"

#include <algorithm>
#include <bit>
#include <charconv>
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>

#include "core/text/caption_font.hpp"
#include "core/world/audio.hpp"
#include "core/world/characters.hpp"

namespace {
constexpr std::size_t decoded_budget = 128 * 1024 * 1024;
std::string pathText(const std::filesystem::path& path) {
  const auto utf8 = path.u8string();
  return {utf8.begin(), utf8.end()};
}
std::vector<unsigned char> readAsset(const std::filesystem::path& path,
                                     std::size_t limit) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file) throw std::runtime_error("cannot open file");
  const auto length = file.tellg();
  if (length <= 0 || static_cast<std::uint64_t>(length) > limit)
    throw std::runtime_error("empty or oversized file");
  std::vector<unsigned char> bytes(static_cast<std::size_t>(length));
  file.seekg(0);
  if (!file.read(reinterpret_cast<char*>(bytes.data()), length))
    throw std::runtime_error("cannot read complete file");
  return bytes;
}
AudioClip readClip(const std::filesystem::path& path, const std::string& id,
                   std::size_t& remaining) {
  const auto bytes = readAsset(path, 32 * 1024 * 1024);
  const auto u16 = [&](std::size_t at) -> unsigned {
    if (at > bytes.size() || bytes.size() - at < 2)
      throw std::runtime_error("truncated WAVE field");
    return unsigned(bytes[at]) | (unsigned(bytes[at + 1]) << 8);
  };
  const auto u32 = [&](std::size_t at) -> std::uint32_t {
    return u16(at) | (std::uint32_t(u16(at + 2)) << 16);
  };
  const auto tag = [&](std::size_t at) -> std::string_view {
    if (at > bytes.size() || bytes.size() - at < 4)
      throw std::runtime_error("truncated WAVE chunk");
    return {reinterpret_cast<const char*>(bytes.data() + at), 4};
  };
  if (bytes.size() < 12 || tag(0) != "RIFF" || tag(8) != "WAVE" ||
      u32(4) != bytes.size() - 8)
    throw std::runtime_error("expected complete RIFF/WAVE file");
  unsigned channels = 0;
  std::size_t data = 0, size = 0;
  for (std::size_t at = 12; at < bytes.size();) {
    if (bytes.size() - at < 8)
      throw std::runtime_error("truncated WAVE chunk header");
    const auto kind = tag(at);
    const std::size_t length = u32(at + 4);
    at += 8;
    if (length > bytes.size() - at)
      throw std::runtime_error("truncated WAVE chunk payload");
    if (kind == "fmt ") {
      if (channels || (length != 16 && length != 18) || u16(at) != 1 ||
          (u16(at + 2) != 1 && u16(at + 2) != 2) || u32(at + 4) != 48000 ||
          u16(at + 14) != 16 || (length == 18 && u16(at + 16) != 0))
        throw std::runtime_error("requires PCM16 at 48000 Hz, mono or stereo");
      channels = u16(at + 2);
      if (u16(at + 12) != channels * 2 || u32(at + 8) != channels * 2 * 48000)
        throw std::runtime_error("invalid PCM frame/byte rate");
    } else if (kind == "data") {
      if (data) throw std::runtime_error("duplicate WAVE data chunk");
      data = at;
      size = length;
    }
    at += length;
    if (length % 2) {
      if (at == bytes.size()) throw std::runtime_error("missing WAVE padding");
      ++at;
    }
  }
  if (!channels || !data || !size || size % (channels * 2))
    throw std::runtime_error("missing or incomplete PCM data");
  const auto frames = size / (channels * 2);
  if (frames > 120 * 48000)
    throw std::runtime_error("clip duration exceeds 120 seconds");
  const auto sample_count = size / 2;
  if (sample_count > remaining / sizeof(float))
    throw std::runtime_error("selected audio exceeds 128 MiB decoded budget");
  remaining -= sample_count * sizeof(float);
  AudioClip clip{id, channels, std::vector<float>(sample_count)};
  for (std::size_t i = 0; i < sample_count; ++i) {
    const unsigned value = u16(data + 2 * i);
    clip.samples[i] =
        static_cast<float>(value >= 32768 ? int(value) - 65536 : int(value)) /
        32768.0F;
  }
  return clip;
}
CaptionTrack readCaptions(const std::filesystem::path& path,
                          const std::string& id, double duration) {
  const auto bytes = readAsset(path, 64 * 1024);
  std::string_view text(reinterpret_cast<const char*>(bytes.data()),
                        bytes.size());
  CaptionTrack track{id, {}};
  const auto number = [](std::string_view value) {
    double parsed{};
    const auto result =
        std::from_chars(value.data(), value.data() + value.size(), parsed);
    if (result.ec != std::errc{} || result.ptr != value.data() + value.size() ||
        !std::isfinite(parsed))
      throw std::runtime_error(
          "caption offsets must be finite decimal seconds");
    return parsed;
  };
  while (!text.empty()) {
    const auto newline = text.find('\n');
    auto line = text.substr(0, newline);
    text = newline == text.npos ? std::string_view{} : text.substr(newline + 1);
    if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
    try {
      if (track.segments.size() >= 64)
        throw std::runtime_error("exceeds 64 segments");
      const auto field = [&]() {
        const auto tab = line.find('\t');
        if (tab == line.npos)
          throw std::runtime_error(
              "expected start, end, label and text separated by tabs");
        const auto first = line.substr(0, tab);
        line.remove_prefix(tab + 1);
        return first;
      };
      const auto start = number(field());
      const auto end = number(field());
      const auto label = field();
      if (label.empty() || line.empty())
        throw std::runtime_error("label and text must be nonempty");
      if (captionScalars(label).size() + captionScalars(line).size() > 160)
        throw std::runtime_error(
            "label and text exceed 160 Unicode scalar values");
      if (start < 0 || end - start < 1 || end > duration ||
          (!track.segments.empty() && start < track.segments.back().end))
        throw std::runtime_error(
            "offsets overlap or exceed clip duration, or segment lasts less "
            "than one second");
      track.segments.push_back(
          {start, end, std::string(label), std::string(line)});
    } catch (const std::exception& e) {
      throw std::runtime_error("segment " +
                               std::to_string(track.segments.size() + 1) +
                               ": " + e.what());
    }
  }
  if (track.segments.empty())
    throw std::runtime_error("caption track is empty");
  return track;
}
}  // namespace

const AudioClip& AudioContent::clip(std::string_view id) const {
  for (const auto& c : clips)
    if (c.id == id) return c;
  throw std::invalid_argument("clip was not prepared: " + std::string(id));
}
const CaptionTrack* AudioContent::caption(std::string_view id) const noexcept {
  for (const auto& c : captions)
    if (c.id == id) return &c;
  return nullptr;
}
AudioContent prepareAudioContent(const std::filesystem::path& root,
                                 const LevelAudio& audio) {
  AudioContent content;
  std::size_t remaining = decoded_budget;
  for (const auto& cue : audio.cues) {
    auto path = root / "audio" / (cue.clip + ".wav");
    try {
      if (!audioCatalogContains(cue.clip))
        throw std::runtime_error("unknown catalog clip");
      if (std::none_of(content.clips.begin(), content.clips.end(),
                       [&](const auto& c) { return c.id == cue.clip; }))
        content.clips.push_back(readClip(path, cue.clip, remaining));
      const auto& clip = content.clip(cue.clip);
      if (clip.channels != 1 &&
          (cue.spatial || cue.kind != AudioCueKind::Ambience))
        throw std::runtime_error("spatial/foreground clips must be mono");
      if (cue.caption) {
        path = root / "captions" / (*cue.caption + ".captions");
        if (!audioCatalogContains(*cue.caption) || *cue.caption != cue.clip)
          throw std::runtime_error(
              "caption catalog identity does not match clip '" + cue.clip +
              "'");
        if (!content.caption(*cue.caption))
          content.captions.push_back(
              readCaptions(path, *cue.caption, clip.duration()));
      } else if (cue.kind != AudioCueKind::Ambience)
        throw std::runtime_error("foreground cue requires captions");
    } catch (const std::exception& e) {
      throw std::runtime_error("cue '" + cue.id + "', clip '" + cue.clip +
                               "', '" + pathText(path) + "': " + e.what());
    }
  }
  return content;
}

void validateAudioCaptions(const AudioContent& content, const LevelAudio& audio,
                           const CaptionFont& font) {
  for (const auto& cue : audio.cues) {
    if (!cue.caption) continue;
    const auto* track = content.caption(*cue.caption);
    if (!track)
      throw std::runtime_error("cue '" + cue.id +
                               "': captions were not prepared");
    for (std::size_t i = 0; i < track->segments.size(); ++i) {
      const auto& segment = track->segments[i];
      try {
        font.validate({segment.label, segment.text},
                      cue.kind == AudioCueKind::Ambience);
      } catch (const std::exception& error) {
        throw std::runtime_error("cue '" + cue.id + "', caption '" + track->id +
                                 "', segment " + std::to_string(i) + ": " +
                                 error.what());
      }
    }
  }
}

void validateCharacterAudio(const AudioContent& content,
                            const LevelAudio& audio,
                            const LevelCharacters& characters) {
  const auto diagnostics = validateCharacterDefinitions(characters, audio);
  if (!diagnostics.empty())
    throw std::runtime_error(diagnostics.front().document_path + ": " +
                             diagnostics.front().message);
  for (const auto& actor : characters.actors) {
    for (const bool interaction : {false, true}) {
      const auto& source_id =
          interaction ? actor.interaction_source : actor.footstep_source;
      if (!source_id) continue;
      const auto source =
          std::find_if(audio.sources.begin(), audio.sources.end(),
                       [&](const auto& s) { return s.id == *source_id; });
      const auto& cue = *findAudioCue(audio, source->cue);
      try {
        const auto& clip = content.clip(cue.clip);
        if (!interaction) {
          if (clip.duration() > .15)
            throw std::runtime_error("footstep exceeds 0.15 seconds");
        } else {
          if (clip.duration() != 1.)
            throw std::runtime_error(
                "interaction must contain one second of PCM");
          if (std::any_of(clip.samples.begin() + 12000 * clip.channels,
                          clip.samples.end(),
                          [](float sample) { return sample != 0; }))
            throw std::runtime_error(
                "interaction must be silent after 0.25 seconds");
          const auto* caption =
              cue.caption ? content.caption(*cue.caption) : nullptr;
          if (!caption || caption->segments.size() != 1 ||
              caption->segments[0].start != 0 || caption->segments[0].end != 1)
            throw std::runtime_error(
                "interaction requires a one-second caption");
        }
      } catch (const std::exception& error) {
        throw std::runtime_error("actor '" + actor.id + "', source '" +
                                 *source_id + "': " + error.what());
      }
    }
  }
}
