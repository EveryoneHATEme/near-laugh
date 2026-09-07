#include "editor/editor_audio_audition.hpp"

bool EditorAudioAudition::start(const EditorDocument& document,
                                EditorObjectId source,
                                const std::filesystem::path& root,
                                const CaptionFont& font, double now,
                                AudioOutput output) {
  stop();
  error_.clear();
  try {
    if (!document.valid())
      throw std::runtime_error(
          "Repair level validation errors before audition.");
    const auto value = document.object(source);
    if (!value || !std::holds_alternative<AudioSourceDefinition>(*value))
      throw std::runtime_error("Select an audio source to audition.");
    const auto snapshot = *document.document();
    auto content = prepareAudioContent(root, snapshot.audio);
    validateAudioCaptions(content, snapshot.audio, font);
    auto cues = std::make_unique<CueCoordinator>(
        snapshot.audio, snapshot.doors, std::move(content), output, now);
    source_ = std::get<AudioSourceDefinition>(*value).id;
    if (cues->start(source_) != CueStart::Started)
      throw std::runtime_error("Audition source could not start.");
    cues_ = std::move(cues);
    generation_ = document.generation();
    revision_ = document.revision();
    return true;
  } catch (const std::exception& error) {
    error_ = error.what();
    stop();
    return false;
  }
}
void EditorAudioAudition::update(const EditorDocument& document,
                                 WorldPosition listener,
                                 WorldPosition direction, double now) {
  if (!cues_) return;
  if (document.generation() != generation_ ||
      document.revision() != revision_) {
    stop();
    return;
  }
  cues_->update(now);
  cues_->listener(listener, direction);
  if (cues_->status(source_) != CueStatus::Playing) stop();
}
void EditorAudioAudition::stop() {
  cues_.reset();
  source_.clear();
}
void EditorAudioAudition::mute() {
  if (cues_) cues_->mute(!cues_->muted());
}
void EditorAudioAudition::pause(double now) {
  if (cues_) cues_->suspend(!cues_->suspended(), now);
}
