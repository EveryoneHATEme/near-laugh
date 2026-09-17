#include "core/gameplay/household_controller.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <utility>

#include "core/world/household.hpp"

namespace {
constexpr std::array controls{&PlayerActionSnapshot::move_forward,
                              &PlayerActionSnapshot::move_backward,
                              &PlayerActionSnapshot::move_left,
                              &PlayerActionSnapshot::move_right,
                              &PlayerActionSnapshot::jump,
                              &PlayerActionSnapshot::sprint,
                              &PlayerActionSnapshot::crouch,
                              &PlayerActionSnapshot::menu,
                              &PlayerActionSnapshot::interact,
                              &PlayerActionSnapshot::primary_action,
                              &PlayerActionSnapshot::secondary_action,
                              &PlayerActionSnapshot::lock};

std::array<float, 4> multiply(std::array<float, 4> a, std::array<float, 4> b) {
  std::array<float, 4> q{a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1],
                         a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0],
                         a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3],
                         a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2]};
  const double length = std::sqrt(double(q[0]) * q[0] + double(q[1]) * q[1] +
                                  double(q[2]) * q[2] + double(q[3]) * q[3]);
  for (auto& value : q) value = float(value / length);
  return q;
}

struct HoldTarget {
  WorldPosition center;
  std::array<float, 4> view_orientation;
};
std::optional<HoldTarget> holdTarget(const PlayerViewPose& view) {
  const auto& p = view.position;
  const auto& d = view.direction;
  const double length = std::hypot(double(d.x), double(d.y), double(d.z));
  if (!(length > 0) || !std::isfinite(length) || !std::isfinite(p.x) ||
      !std::isfinite(p.y) || !std::isfinite(p.z))
    return std::nullopt;
  const double yaw = std::atan2(-double(d.x), -double(d.z));
  const double pitch =
      std::atan2(double(d.y), std::hypot(double(d.x), double(d.z)));
  return HoldTarget{
      {float(p.x + .9 * d.x / length), float(p.y + .9 * d.y / length - .15),
       float(p.z + .9 * d.z / length)},
      multiply({0, float(std::sin(yaw / 2)), 0, float(std::cos(yaw / 2))},
               {float(std::sin(pitch / 2)), 0, 0, float(std::cos(pitch / 2))})};
}
}  // namespace

HouseholdController::HouseholdController(const PrototypeLevel& level,
                                         PhysicsWorld& physics,
                                         CueCoordinator& audio)
    : level_(level), physics_(physics), audio_(audio) {
  boxes_.reserve(frame_maximum_opaque_box_count);
  for (const auto& radio : level.household().radios) {
    const auto prop =
        std::find_if(level.props().begin(), level.props().end(),
                     [&](const auto& p) { return p.id == radio.prop; });
    if (prop == level.props().end())
      throw std::invalid_argument("Radio '" + radio.id + "' has no prop");
    radio_props_.push_back(&*prop);
    radio_enabled_.push_back(radio.initially_on);
    audio_.moveSource(radio.source, prop->translation);
    if (radio.initially_on && audio_.start(radio.source) == CueStart::Busy)
      throw std::runtime_error("Radio '" + radio.id +
                               "' cannot start its source");
  }
}

HouseholdController::~HouseholdController() { physics_.dropHeldBox(); }

void HouseholdController::blockHeldControls() {
  for (std::size_t i = 0; i < controls.size(); ++i)
    blocked_[i] = blocked_[i] || last_input_.*controls[i];
}

PlayerActionSnapshot HouseholdController::sampleInput(
    const PlayerActionSnapshot& input, bool controls_active, bool suspended) {
  last_input_ = input;
  active_ = controls_active && !suspended;
  suspended_ = suspended;
  mode_changed_ = false;
  if (!input.menu) reader_escape_held_ = false;
  for (std::size_t i = 0; i < controls.size(); ++i)
    blocked_[i] = blocked_[i] && input.*controls[i];
  const std::array down{input.interact, input.menu, input.move_left,
                        input.move_right};
  std::array<bool, 4> press{};
  for (std::size_t i = 0; i < down.size(); ++i) {
    press[i] = down[i] && reader_armed_[i];
    reader_armed_[i] = !down[i];
  }
  if (!active_) {
    pending_.reset();
    blockHeldControls();
    if (!suspended && physics_.heldBox()) safety_release_ = true;
  }
  if (document_) {
    blockHeldControls();
    if (active_) {
      if (press[0] || press[1]) {
        reader_escape_held_ = input.menu;
        closeDocument();
      } else if (press[2] && page_ > 0)
        --page_;
      else if (press[3] &&
               page_ + 1 <
                   level_.household().documents[*document_].pages.size())
        ++page_;
    }
  }
  PlayerActionSnapshot filtered = input;
  for (std::size_t i = 0; i < controls.size(); ++i)
    if (blocked_[i]) filtered.*controls[i] = false;
  if (!worldActionsAllowed()) filtered = {};
  return filtered;
}

bool HouseholdController::consumesEscape() const noexcept {
  return document_.has_value() || reader_escape_held_;
}
bool HouseholdController::worldActionsAllowed() const noexcept {
  return active_ && !document_ && !mode_changed_;
}
bool HouseholdController::preservesStance() const noexcept {
  return document_.has_value() || mode_changed_;
}
void HouseholdController::suspend() {
  suspended_ = true;
  pending_.reset();
}
void HouseholdController::stop() {
  pending_.reset();
  safety_release_ = false;
  physics_.dropHeldBox();
}

bool HouseholdController::queue(PendingCommand command) {
  if (!worldActionsAllowed() || safety_release_) return false;
  if (pending_) {
    feedback(HouseholdResult::Busy, "Действие уже ожидает выполнения");
    return false;
  }
  pending_ = command;
  result_ = HouseholdResult::Queued;
  return true;
}
bool HouseholdController::requestPickup(std::size_t box,
                                        const PlayerViewPose& selection) {
  if (physics_.heldBox() || box >= physics_.boxCount()) return false;
  return queue({HouseholdCommandKind::Pickup, box, selection, {}});
}
bool HouseholdController::requestDrop() {
  if (!physics_.heldBox()) return false;
  return queue({HouseholdCommandKind::Drop, *physics_.heldBox(), {}, {}});
}
bool HouseholdController::requestThrow(PhysicsVector direction) {
  if (!physics_.heldBox()) return false;
  return queue(
      {HouseholdCommandKind::Throw, *physics_.heldBox(), {}, direction});
}

void HouseholdController::beforeFixedStep(const PlayerViewPose& view) {
  if (suspended_) return;
  if (safety_release_) {
    physics_.dropHeldBox();
    safety_release_ = false;
    pending_.reset();
    feedback(HouseholdResult::Released, "Коробка отпущена");
  }
  const auto target = holdTarget(view);
  if (pending_) {
    const auto command = std::exchange(pending_, std::nullopt).value();
    if (command.kind == HouseholdCommandKind::Drop) {
      physics_.dropHeldBox();
      feedback(HouseholdResult::Dropped, "Коробка опущена");
    } else if (command.kind == HouseholdCommandKind::Throw) {
      if (physics_.throwHeldBox(command.throw_direction))
        feedback(HouseholdResult::Thrown, "Коробка брошена");
      else
        feedback(HouseholdResult::Refused, "Не удалось бросить коробку");
    } else {
      const auto box = physics_.boxState(command.box);
      const auto bounds = householdBoxPresentation(box.center, box.orientation);
      const auto& selection = command.selection;
      const WorldPosition eye{selection.position.x, selection.position.y,
                              selection.position.z};
      const WorldPosition direction{
          selection.direction.x, selection.direction.y, selection.direction.z};
      const auto distance = householdRayDistance(bounds, eye, direction);
      bool eligible = target && distance && *distance <= 2.F;
      if (eligible) {
        const double length = std::hypot(
            double(direction.x), double(direction.y), double(direction.z));
        const WorldPosition hit{
            float(eye.x + direction.x / length * *distance),
            float(eye.y + direction.y / length * *distance),
            float(eye.z + direction.z / length * *distance)};
        const WorldPosition current_eye{view.position.x, view.position.y,
                                        view.position.z};
        eligible =
            std::hypot(double(hit.x) - current_eye.x,
                       double(hit.y) - current_eye.y,
                       double(hit.z) - current_eye.z) <= 2. &&
            !physics_.worldSegmentBlocked(
                eye, hit, {}, level_.household().boxes[command.box].id) &&
            !physics_.worldSegmentBlocked(
                current_eye, hit, {}, level_.household().boxes[command.box].id);
      }
      if (eligible &&
          physics_.beginBoxHold(command.box, target->center, box.orientation)) {
        const auto& q = target->view_orientation;
        hold_relative_orientation_ =
            multiply({-q[0], -q[1], -q[2], q[3]}, box.orientation);
        feedback(HouseholdResult::PickedUp,
                 "Коробка в руках. Положите её перед другим действием");
      } else
        feedback(HouseholdResult::Refused, "Коробка недоступна");
    }
  }
  if (physics_.heldBox()) {
    if (!target || !physics_.updateBoxHold(
                       target->center, multiply(target->view_orientation,
                                                hold_relative_orientation_))) {
      physics_.dropHeldBox();
      feedback(HouseholdResult::Released,
               "Коробка отпущена: слишком далеко от точки удержания");
    }
  }
}

std::optional<std::size_t> HouseholdController::heldBox() const noexcept {
  return physics_.heldBox();
}
void HouseholdController::openDocument(std::size_t document) {
  if (!worldActionsAllowed() || physics_.heldBox()) return;
  if (pending_) {
    feedback(HouseholdResult::Busy, "Действие уже ожидает выполнения");
    return;
  }
  (void)level_.household().documents.at(document);
  document_ = document;
  page_ = 0;
  mode_changed_ = true;
  blockHeldControls();
  feedback(HouseholdResult::Reading, "Чтение: мир продолжает двигаться");
}
void HouseholdController::closeDocument() {
  document_.reset();
  mode_changed_ = true;
  pending_.reset();
  blockHeldControls();
  feedback(HouseholdResult::Closed, "Документ закрыт");
}
std::optional<std::size_t> HouseholdController::readingDocument()
    const noexcept {
  return document_;
}
void HouseholdController::toggleRadio(std::size_t radio) {
  if (!worldActionsAllowed() || physics_.heldBox()) return;
  if (pending_) {
    feedback(HouseholdResult::Busy, "Действие уже ожидает выполнения");
    return;
  }
  const auto& definition = level_.household().radios.at(radio);
  if (radio_enabled_.at(radio)) {
    audio_.cancel(definition.source);
    radio_enabled_[radio] = false;
    feedback(HouseholdResult::RadioOff, "Радио выключено");
  } else if (audio_.start(definition.source) != CueStart::Busy) {
    radio_enabled_[radio] = true;
    feedback(HouseholdResult::RadioOn, "Радио включено");
  } else
    feedback(HouseholdResult::Refused, "Не удалось включить радио");
  hint_ = radio_enabled_[radio] ? "E — выключить радио" : "E — включить радио";
}
bool HouseholdController::radioOn(std::size_t radio) const {
  return radio_enabled_.at(radio);
}
bool HouseholdController::ownsSource(std::string_view source) const noexcept {
  return std::any_of(level_.household().radios.begin(),
                     level_.household().radios.end(),
                     [&](const auto& radio) { return radio.source == source; });
}
void HouseholdController::hint(std::string_view text) { hint_ = text; }
void HouseholdController::feedback(HouseholdResult result,
                                   std::string_view text) {
  result_ = result;
  feedback_ = text;
  feedback_until_ = audio_.activeTime() + 2.5;
}
HouseholdTextPresentation HouseholdController::text() {
  HouseholdTextPresentation presentation;
  if (document_) {
    const auto& document = level_.household().documents[*document_];
    page_controls_ = std::to_string(page_ + 1) + "/" +
                     std::to_string(document.pages.size()) +
                     "  A/D — страницы; E/Escape — закрыть";
    presentation.reader = {document.title, document.pages[page_],
                           page_controls_};
  } else if (physics_.heldBox())
    presentation.hint =
        "E — опустить; правая кнопка — бросить. Другие действия заняты";
  else
    presentation.hint = hint_;
  if (audio_.activeTime() < feedback_until_) presentation.feedback = feedback_;
  return presentation;
}
std::span<const OpaqueBoxFrame> HouseholdController::presentation(
    std::span<const OpaqueBoxFrame> doors) {
  boxes_.assign(doors.begin(), doors.end());
  for (std::size_t i = 0; i < physics_.boxCount(); ++i) {
    const auto state = physics_.boxState(i);
    boxes_.push_back(householdBoxPresentation(state.center, state.orientation));
  }
  for (const auto& document : level_.household().documents)
    boxes_.push_back(householdDocumentPresentation(document));
  for (std::size_t i = 0; i < radio_props_.size(); ++i)
    boxes_.push_back(
        householdRadioPresentation(*radio_props_[i], radio_enabled_[i]));
  if (boxes_.size() > frame_maximum_opaque_box_count)
    throw std::length_error("Combined household presentation exceeds capacity");
  return boxes_;
}
