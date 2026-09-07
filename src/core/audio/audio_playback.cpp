#include "core/audio/audio_playback.hpp"

#include <miniaudio.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <stdexcept>
#include <string>

#include "core/audio/transmission.hpp"

namespace {
bool audioFailureAt(std::string_view stage) {
#if defined(_WIN32)
  char* value = nullptr;
  std::size_t size = 0;
  if (_dupenv_s(&value, &size, "NEAR_LAUGH_FORCE_AUDIO_FAILURE_STAGE") != 0)
    return false;
  const bool result = value && stage == value;
  std::free(value);
  return result;
#else
  const auto* value = std::getenv("NEAR_LAUGH_FORCE_AUDIO_FAILURE_STAGE");
  return value && stage == value;
#endif
}
}  // namespace

struct AudioPlayback::Impl {
  struct Voice {
    ma_audio_buffer buffer{};
    ma_sound sound{};
    bool buffer_ready{}, sound_ready{};
    WorldPosition position{};
    float gain{}, near_distance{}, far_distance{}, target_gain{-1};
    bool spatial{};
    ~Voice() {
      if (sound_ready) ma_sound_uninit(&sound);
      if (buffer_ready) ma_audio_buffer_uninit(&buffer);
    }
  };
  AudioContent content;
  ma_engine engine{};
  bool initialized{};
  AudioOutput output{};
  std::string warning{};
  std::array<std::unique_ptr<Voice>, level_maximum_audio_source_count> voices{};
  bool suspended{};
  std::atomic<bool> device_lost{};
  WorldPosition listener_position{};

  explicit Impl(AudioContent prepared, AudioOutput selected)
      : content(std::move(prepared)), output(selected) {
    if (selected == AudioOutput::Silent) return;
    if (audioFailureAt("device-init")) {
      output = AudioOutput::Silent;
      warning =
          "Audio device initialization failed; captions continue. Restart to "
          "retry.";
      return;
    }
    auto config = ma_engine_config_init();
    config.channels = 2;
    config.sampleRate = 48000;
    config.noDevice = selected == AudioOutput::Offline;
    config.noAutoStart = MA_TRUE;
    config.gainSmoothTimeInMilliseconds = 50;
    config.defaultVolumeSmoothTimeInPCMFrames = 2400;
    config.pProcessUserData = this;
    config.notificationCallback =
        [](const ma_device_notification* notification) {
          if (notification->type == ma_device_notification_type_rerouted ||
              notification->type ==
                  ma_device_notification_type_interruption_began) {
            auto* e = static_cast<ma_engine*>(notification->pDevice->pUserData);
            auto* self = static_cast<Impl*>(e->pProcessUserData);
            self->device_lost.store(true, std::memory_order_relaxed);
          }
        };
    const auto result = ma_engine_init(&config, &engine);
    if (result != MA_SUCCESS) {
      if (selected == AudioOutput::Offline)
        throw std::runtime_error("Cannot initialize offline audio engine: " +
                                 std::to_string(result));
      output = AudioOutput::Silent;
      warning = "Audio output unavailable (" + std::to_string(result) +
                "). Check the output device and restart to retry; captions "
                "remain available.";
      return;
    }
    initialized = true;
    if (audioFailureAt("after-engine") ||
        (selected == AudioOutput::Device &&
         ma_engine_get_device(&engine)->pContext->backend == ma_backend_null)) {
      shutdown();
      output = AudioOutput::Silent;
      warning =
          "Audio device unavailable after initialization; captions continue. "
          "Restart to retry.";
      return;
    }
    if (selected == AudioOutput::Device &&
        ma_engine_start(&engine) != MA_SUCCESS) {
      ma_engine_uninit(&engine);
      initialized = false;
      output = AudioOutput::Silent;
      warning =
          "Cannot start audio output. Check the output device and restart to "
          "retry; captions remain available.";
    }
  }

  ~Impl() { shutdown(); }
  void shutdown() noexcept {
    if (!initialized) return;
    // ma_engine_stop joins device processing before any buffers are freed.
    if (output == AudioOutput::Device) ma_engine_stop(&engine);
    for (auto& voice : voices) voice.reset();
    ma_engine_uninit(&engine);
    initialized = false;
  }
  Voice* voice(std::size_t slot) const {
    if (slot >= voices.size())
      throw std::out_of_range("audio voice slot exceeds 64-source bound");
    return voices[slot].get();
  }
  void updateGain(Voice& voice) {
    const float gain =
        voice.gain *
        (voice.spatial
             ? audioDistanceGain(voice.position, listener_position,
                                 voice.near_distance, voice.far_distance)
             : 1.0F);
    if (gain != voice.target_gain) {
      ma_sound_set_fade_in_milliseconds(&voice.sound, -1, gain, 50);
      voice.target_gain = gain;
    }
  }
};

AudioPlayback::AudioPlayback(AudioOutput output)
    : AudioPlayback(AudioContent{}, output) {}
AudioPlayback::AudioPlayback(AudioContent content, AudioOutput output)
    : impl_(std::make_unique<Impl>(std::move(content), output)) {}
AudioPlayback::~AudioPlayback() = default;
bool AudioPlayback::silent() const noexcept {
  return impl_->output == AudioOutput::Silent;
}
std::string_view AudioPlayback::warning() const noexcept {
  return impl_->warning;
}
void AudioPlayback::render(std::span<float> samples) {
  if (samples.size() % 2 != 0)
    throw std::invalid_argument(
        "Audio output requires interleaved stereo frames");
  if (impl_->output == AudioOutput::Device)
    throw std::logic_error(
        "Cannot render offline while an audio device is active");
  std::fill(samples.begin(), samples.end(), 0.0F);
  if (impl_->initialized && !impl_->suspended && !samples.empty() &&
      ma_engine_read_pcm_frames(&impl_->engine, samples.data(),
                                samples.size() / 2, nullptr) != MA_SUCCESS)
    throw std::runtime_error("Offline audio rendering failed");
}

const AudioContent& AudioPlayback::content() const noexcept {
  return impl_->content;
}
void AudioPlayback::start(std::size_t slot, const AudioCueDefinition& cue,
                          const AudioSourceDefinition& source) {
  stop(slot);
  const auto& clip = impl_->content.clip(cue.clip);
  if (!impl_->initialized) return;
  auto voice = std::make_unique<Impl::Voice>();
  const auto buffer_config = ma_audio_buffer_config_init(
      ma_format_f32, clip.channels, clip.samples.size() / clip.channels,
      clip.samples.data(), nullptr);
  if (ma_audio_buffer_init(&buffer_config, &voice->buffer) != MA_SUCCESS)
    throw std::runtime_error("cannot create audio buffer for source '" +
                             source.id + "'");
  voice->buffer_ready = true;
  if (audioFailureAt("voice"))
    throw std::runtime_error("injected audio voice initialization failure");
  if (ma_sound_init_from_data_source(&impl_->engine, &voice->buffer,
                                     MA_SOUND_FLAG_NO_PITCH, nullptr,
                                     &voice->sound) != MA_SUCCESS)
    throw std::runtime_error("cannot create audio voice for source '" +
                             source.id + "'");
  voice->sound_ready = true;
  voice->position = source.position;
  voice->gain = source.gain;
  voice->near_distance = source.near_distance;
  voice->far_distance = source.far_distance;
  voice->spatial = cue.spatial;
  ma_sound_set_spatialization_enabled(&voice->sound, cue.spatial);
  ma_sound_set_looping(&voice->sound, cue.loop);
  ma_sound_set_doppler_factor(&voice->sound, 0);
  // Apply the game's finite linear attenuation with a 50 ms fade. The backend
  // spatial gain smoother otherwise retains a small tail beyond the far bound.
  ma_sound_set_attenuation_model(&voice->sound, ma_attenuation_model_linear);
  ma_sound_set_rolloff(&voice->sound, 0);
  ma_sound_set_min_distance(&voice->sound, source.near_distance);
  ma_sound_set_max_distance(&voice->sound, source.far_distance);
  ma_sound_set_min_gain(&voice->sound, 0);
  ma_sound_set_max_gain(&voice->sound, 1);
  ma_sound_set_position(&voice->sound, source.position.x, source.position.y,
                        source.position.z);
  ma_sound_set_fade_in_milliseconds(&voice->sound, 0, 0, 0);
  impl_->updateGain(*voice);
  if (ma_sound_start(&voice->sound) != MA_SUCCESS)
    throw std::runtime_error("cannot start audio source '" + source.id + "'");
  impl_->voices[slot] = std::move(voice);
}
void AudioPlayback::stop(std::size_t slot) {
  static_cast<void>(impl_->voice(slot));
  impl_->voices[slot].reset();
}
void AudioPlayback::source(std::size_t slot, WorldPosition position,
                           float gain) {
  if (!std::isfinite(position.x) || !std::isfinite(position.y) ||
      !std::isfinite(position.z) || !std::isfinite(gain) || gain < 0 ||
      gain > 1)
    throw std::invalid_argument(
        "audio source requires finite position and gain in [0,1]");
  if (auto* voice = impl_->voice(slot)) {
    voice->position = position;
    voice->gain = gain;
    ma_sound_set_position(&voice->sound, position.x, position.y, position.z);
    impl_->updateGain(*voice);
  }
}
void AudioPlayback::listener(WorldPosition position, WorldPosition direction) {
  const double length = std::sqrt(double(direction.x) * direction.x +
                                  double(direction.y) * direction.y +
                                  double(direction.z) * direction.z);
  if (!std::isfinite(position.x) || !std::isfinite(position.y) ||
      !std::isfinite(position.z) || !std::isfinite(length) || length < 0.0001)
    throw std::invalid_argument(
        "audio listener requires finite position and nonzero direction");
  if (!impl_->initialized) return;
  impl_->listener_position = position;
  ma_engine_listener_set_position(&impl_->engine, 0, position.x, position.y,
                                  position.z);
  ma_engine_listener_set_direction(
      &impl_->engine, 0, float(direction.x / length),
      float(direction.y / length), float(direction.z / length));
  ma_engine_listener_set_world_up(&impl_->engine, 0, 0, 1, 0);
  for (auto& voice : impl_->voices)
    if (voice) impl_->updateGain(*voice);
}
void AudioPlayback::mute(bool muted) {
  if (impl_->initialized)
    ma_engine_set_volume(&impl_->engine, muted ? 0.0F : 1.0F);
}
void AudioPlayback::suspend(bool suspended) {
  if (impl_->suspended == suspended) return;
  impl_->suspended = suspended;
  if (impl_->output != AudioOutput::Device || !impl_->initialized) return;
  if (suspended)
    ma_engine_stop(&impl_->engine);
  else if (ma_engine_start(&impl_->engine) != MA_SUCCESS)
    silentFallback(
        "Audio output failed on resume; captions continue. Restart to retry "
        "the device.");
}
bool AudioPlayback::seek(std::size_t slot, double seconds) {
  if (audioFailureAt("seek")) return false;
  if (!std::isfinite(seconds) || seconds < 0 || seconds > 120) return false;
  auto* voice = impl_->voice(slot);
  return !voice || ma_sound_seek_to_pcm_frame(
                       &voice->sound,
                       static_cast<ma_uint64>(seconds * 48000)) == MA_SUCCESS;
}
double AudioPlayback::cursor(std::size_t slot) const {
  auto* voice = impl_->voice(slot);
  ma_uint64 frames{};
  if (!voice ||
      ma_sound_get_cursor_in_pcm_frames(&voice->sound, &frames) != MA_SUCCESS)
    return -1;
  return static_cast<double>(frames) / 48000;
}
std::size_t AudioPlayback::voiceCount() const noexcept {
  return std::count_if(impl_->voices.begin(), impl_->voices.end(),
                       [](const auto& voice) { return bool(voice); });
}
void AudioPlayback::pollDevice() {
  if (audioFailureAt("device-loss")) {
    silentFallback("Audio device lost; captions continue. Restart to retry.");
    return;
  }
  if (impl_->output != AudioOutput::Device || !impl_->initialized ||
      impl_->suspended)
    return;
  if (impl_->device_lost.load(std::memory_order_relaxed) ||
      !ma_device_is_started(ma_engine_get_device(&impl_->engine)))
    silentFallback(
        "Audio device lost or changed; captions continue. Restart to retry the "
        "output device.");
}
void AudioPlayback::silentFallback(std::string reason) {
  impl_->shutdown();
  impl_->output = AudioOutput::Silent;
  impl_->warning = std::move(reason);
}
