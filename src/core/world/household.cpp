#include "core/world/household.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <set>
#include <stdexcept>

#include "core/world/audio.hpp"
#include "core/world/characters.hpp"
#include "core/world/prototype_level.hpp"
#include "core/world/scene_assets.hpp"

OpaqueBoxFrame householdRadioBounds(const PrototypeStaticProp& prop) {
  const auto* model = findSceneModel(prop.model);
  if (!model) throw std::invalid_argument("Radio prop has no catalog model");
  const auto bounds = sceneModelBounds(*model);
  const auto center = propBoxWorldCenter(prop, bounds);
  const auto extent = propBoxWorldHalfExtent(prop, bounds);
  return {{center.x, center.y, center.z},
          {extent.x, extent.y, extent.z},
          yawQuaternion(prop.yaw_degrees)};
}

bool householdPointInside(const OpaqueBoxFrame& box,
                          WorldPosition point) noexcept {
  if (!quaternionIsValid(box.orientation)) return false;
  const auto& q = box.orientation;
  const auto local =
      rotateVector({-q[0], -q[1], -q[2], q[3]},
                   {point.x - box.center[0], point.y - box.center[1],
                    point.z - box.center[2]});
  return std::abs(local[0]) <= box.half_extent[0] &&
         std::abs(local[1]) <= box.half_extent[1] &&
         std::abs(local[2]) <= box.half_extent[2];
}

std::optional<float> householdRayDistance(const OpaqueBoxFrame& box,
                                          WorldPosition origin,
                                          WorldPosition direction) noexcept {
  if (!quaternionIsValid(box.orientation) || householdPointInside(box, origin))
    return std::nullopt;
  const double length =
      std::hypot(double(direction.x), double(direction.y), double(direction.z));
  if (!(length > 0) || !std::isfinite(length)) return std::nullopt;
  const auto& q = box.orientation;
  const std::array<float, 4> inverse{-q[0], -q[1], -q[2], q[3]};
  const auto p =
      rotateVector(inverse, {origin.x - box.center[0], origin.y - box.center[1],
                             origin.z - box.center[2]});
  const auto d = rotateVector(
      inverse, {float(direction.x / length), float(direction.y / length),
                float(direction.z / length)});
  double enter = 0, leave = std::numeric_limits<double>::infinity();
  for (std::size_t axis = 0; axis < 3; ++axis) {
    const double extent = box.half_extent[axis];
    if (!(extent > 0) || !std::isfinite(extent) || !std::isfinite(p[axis]) ||
        !std::isfinite(d[axis]))
      return std::nullopt;
    if (std::abs(d[axis]) < 1.e-12) {
      if (std::abs(p[axis]) > extent) return std::nullopt;
      continue;
    }
    double a = (-extent - p[axis]) / d[axis];
    double b = (extent - p[axis]) / d[axis];
    if (a > b) std::swap(a, b);
    enter = std::max(enter, a);
    leave = std::min(leave, b);
    if (enter > leave) return std::nullopt;
  }
  if (!std::isfinite(enter)) return std::nullopt;
  return float(enter);
}

namespace {
bool finite(WorldPosition p) {
  return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}
bool safePose(const DoorLeafPose& box) {
  if (!finite(box.center) || !std::isfinite(box.yaw_degrees)) return false;
  const double angle =
      std::remainder(double(box.yaw_degrees), 360.) * std::numbers::pi / 180.;
  const double c = std::abs(std::cos(angle)), s = std::abs(std::sin(angle));
  const WorldExtent extent{
      float(c * box.half_extent.x + s * box.half_extent.z), box.half_extent.y,
      float(s * box.half_extent.x + c * box.half_extent.z)};
  const WorldPosition lo{box.center.x - extent.x, box.center.y - extent.y,
                         box.center.z - extent.z};
  const WorldPosition hi{box.center.x + extent.x, box.center.y + extent.y,
                         box.center.z + extent.z};
  return finite(lo) && finite(hi) && lo.x < hi.x && lo.y < hi.y && lo.z < hi.z;
}

// Count Unicode scalars without a font/resource dependency. Glyphs and actual
// page layout are checked separately by selected-resource text preflight.
bool boundedText(std::string_view text, std::size_t maximum) {
  if (text.empty() || text.size() > 4 * maximum) return false;
  std::size_t count = 0;
  for (std::size_t i = 0; i < text.size(); ++count) {
    const auto lead = static_cast<unsigned char>(text[i++]);
    unsigned trailing = 0;
    char32_t scalar = lead, minimum = 0;
    if (lead >= 0xc2 && lead <= 0xdf) {
      trailing = 1;
      scalar = lead & 31;
      minimum = 0x80;
    } else if (lead >= 0xe0 && lead <= 0xef) {
      trailing = 2;
      scalar = lead & 15;
      minimum = 0x800;
    } else if (lead >= 0xf0 && lead <= 0xf4) {
      trailing = 3;
      scalar = lead & 7;
      minimum = 0x10000;
    } else if (lead >= 0x80)
      return false;
    for (unsigned j = 0; j < trailing; ++j) {
      if (i == text.size()) return false;
      const auto byte = static_cast<unsigned char>(text[i++]);
      if ((byte & 0xc0) != 0x80) return false;
      scalar = (scalar << 6) | (byte & 63);
    }
    if (scalar < minimum || scalar > 0x10ffff ||
        (scalar >= 0xd800 && scalar <= 0xdfff))
      return false;
  }
  return count <= maximum;
}
}  // namespace

DoorLeafPose householdBoxPose(const HouseholdBoxDefinition& box) noexcept {
  return {box.center,
          {household_box_half_extent, household_box_half_extent,
           household_box_half_extent},
          box.yaw_degrees};
}
DoorLeafPose householdDocumentPose(
    const HouseholdDocumentDefinition& document) noexcept {
  return {document.position, household_document_half_extent,
          document.yaw_degrees};
}

OpaqueBoxFrame householdBoxPresentation(
    WorldPosition center, std::array<float, 4> orientation) noexcept {
  return {{center.x, center.y, center.z},
          {household_box_half_extent, household_box_half_extent,
           household_box_half_extent},
          orientation,
          {156, 115, 68, 255},
          2};
}

OpaqueBoxFrame householdDocumentPresentation(
    const HouseholdDocumentDefinition& document) noexcept {
  const auto p = document.position;
  const auto h = household_document_half_extent;
  return {{p.x, p.y, p.z},
          {h.x, h.y, h.z},
          yawQuaternion(document.yaw_degrees),
          {235, 224, 188, 255},
          2};
}

OpaqueBoxFrame householdRadioPresentation(const PrototypeStaticProp& prop,
                                          bool on) {
  const auto* model = findSceneModel(prop.model);
  if (!model || prop.model != "apartment-radio" ||
      !prototypeStaticPropIsValid(prop))
    throw std::invalid_argument("Radio indicator requires a valid radio prop");
  const PropCollisionBox indicator{
      {(model->bounds_min.x + model->bounds_max.x) / 2,
       model->bounds_max.y + .015F,
       (model->bounds_min.z + model->bounds_max.z) / 2},
      {.015F, .015F, .015F}};
  const auto p = propBoxWorldCenter(prop, indicator);
  const auto h = propBoxWorldHalfExtent(prop, indicator);
  return {{p.x, p.y, p.z},
          {h.x, h.y, h.z},
          yawQuaternion(prop.yaw_degrees),
          on ? WorldColor{64, 210, 90, 255} : WorldColor{130, 55, 45, 255},
          2};
}

std::vector<OpaqueBoxFrame> householdInitialPresentation(
    const LevelDocument& document) {
  const auto& h = document.household;
  if (h.boxes.size() > level_maximum_household_box_count ||
      h.documents.size() > level_maximum_household_document_count ||
      h.radios.size() > level_maximum_household_radio_count)
    throw std::invalid_argument("Household preview count exceeds its profile");
  std::vector<OpaqueBoxFrame> result;
  result.reserve(h.boxes.size() + h.documents.size() + h.radios.size());
  for (const auto& box : h.boxes)
    if (safePose(householdBoxPose(box)))
      result.push_back(
          householdBoxPresentation(box.center, yawQuaternion(box.yaw_degrees)));
  for (const auto& document : h.documents)
    if (safePose(householdDocumentPose(document)))
      result.push_back(householdDocumentPresentation(document));
  for (const auto& radio : h.radios) {
    const auto prop =
        std::find_if(document.props.begin(), document.props.end(),
                     [&](const auto& p) { return p.id == radio.prop; });
    if (prop != document.props.end() && prop->model == "apartment-radio" &&
        prototypeStaticPropIsValid(*prop))
      result.push_back(householdRadioPresentation(*prop, radio.initially_on));
  }
  return result;
}

std::vector<LevelDiagnostic> validateHouseholdDefinitions(
    const LevelDocument& document, const std::filesystem::path& source_path) {
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
    const auto path = "household." + std::string(collection);
    if (records.size() > maximum)
      error(path, "exceeds the " + std::to_string(maximum) + "-record limit");
    std::set<std::string_view> ids;
    for (std::size_t i = 0; i < records.size(); ++i) {
      const auto& id = records[i].id;
      const auto p = path + "[" + std::to_string(i) + "].id";
      if (!levelEntryIdIsValid(id))
        error(p, "'" + id + "' must match [a-z][a-z0-9-]{0,63}");
      if (!ids.insert(id).second) error(p, "duplicate identifier '" + id + "'");
    }
  };
  const auto& household = document.household;
  checkIds(household.boxes, level_maximum_household_box_count, "boxes");
  checkIds(household.documents, level_maximum_household_document_count,
           "documents");
  checkIds(household.radios, level_maximum_household_radio_count, "radios");
  const auto* terrain =
      document.terrain && prototypeTerrainIsValid(*document.terrain)
          ? &*document.terrain
          : nullptr;
  for (std::size_t i = 0; i < household.boxes.size(); ++i) {
    const auto& box = household.boxes[i];
    const auto p = "household.boxes[" + std::to_string(i) + "]";
    const auto label = "box '" + box.id + "': ";
    const auto bounds = householdBoxPose(box);
    if (!std::isfinite(box.yaw_degrees))
      error(p + ".yaw_degrees", label + "must be finite");
    if (!safePose(bounds)) {
      error(p + ".center",
            label + "derived bounds must be finite and nondegenerate");
      continue;
    }
    const auto overlap = [&](const DoorLeafPose& other,
                             const std::string& name) {
      if (safePose(other) && yawedBoxesOverlap(bounds, other))
        error(p + ".center", label + "initial bounds overlap " + name);
    };
    if (terrain && yawedBoxOverlapsTerrain(bounds, *terrain))
      error(p + ".center", label + "initial bounds penetrate terrain");
    for (std::size_t j = 0; j < document.solids.size(); ++j) {
      const auto& solid = document.solids[j];
      if (prototypeSolidIsValid(solid))
        overlap({solid.center, solid.half_extent, 0},
                "solid[" + std::to_string(j) + "]");
    }
    for (const auto& prop : document.props)
      if (prototypeStaticPropIsValid(prop))
        for (const auto& proxy : prop.collision_boxes)
          overlap({propBoxWorldCenter(prop, proxy),
                   propBoxWorldHalfExtent(prop, proxy), prop.yaw_degrees},
                  "prop '" + prop.id + "'");
    for (std::size_t j = 0; j < i; ++j)
      overlap(householdBoxPose(household.boxes[j]),
              "box '" + household.boxes[j].id + "'");
    for (const auto& door : document.doors)
      if (doorGeometryIsValid(door))
        overlap(doorLeafPose(door, doorInitialAngle(door)),
                "door '" + door.id + "'");
    for (const auto& entry : document.entries) {
      const auto feet = entry.pose.foot_position;
      const float radius =
          prototype_spawn_validation_radius + prototype_player_contact_padding;
      const float height = prototype_spawn_validation_height +
                           2 * prototype_player_contact_padding;
      overlap({{feet.x, feet.y + height / 2, feet.z},
               {radius, height / 2, radius},
               0},
              "entry '" + entry.id + "'");
    }
    for (const auto& actor : document.characters.actors) {
      const auto* model = findCharacterModel(actor.model);
      const auto* mark =
          findCharacterMark(document.characters, actor.initial_mark);
      if (!model || !mark || !finite(mark->feet_position)) continue;
      auto feet = mark->feet_position;
      feet.y +=
          prototypeActorCapsuleOffset(terrain, feet, model->capsule_radius_m);
      overlap({{feet.x, feet.y + model->capsule_height_m / 2, feet.z},
               {model->capsule_radius_m, model->capsule_height_m / 2,
                model->capsule_radius_m},
               0},
              "actor '" + actor.id + "'");
    }
  }
  for (std::size_t i = 0; i < household.documents.size(); ++i) {
    const auto& readable = household.documents[i];
    const auto p = "household.documents[" + std::to_string(i) + "]";
    const auto label = "document '" + readable.id + "': ";
    if (!std::isfinite(readable.yaw_degrees))
      error(p + ".yaw_degrees", label + "must be finite");
    if (!safePose(householdDocumentPose(readable)))
      error(p + ".position",
            label + "derived panel bounds must be finite and nondegenerate");
    if (!boundedText(readable.title, level_maximum_document_title_scalars))
      error(p + ".title",
            label + "requires 1 through 80 valid Unicode scalar values");
    if (readable.pages.empty() ||
        readable.pages.size() > level_maximum_document_page_count)
      error(p + ".pages", label + "requires 1 through 16 ordered pages");
    for (std::size_t j = 0; j < readable.pages.size(); ++j)
      if (!boundedText(readable.pages[j], level_maximum_document_page_scalars))
        error(p + ".pages[" + std::to_string(j) + "]",
              label + "page " + std::to_string(j + 1) +
                  " requires 1 through 480 valid Unicode scalar values");
  }
  std::set<std::string_view> props, sources;
  for (std::size_t i = 0; i < household.radios.size(); ++i) {
    const auto& radio = household.radios[i];
    const auto p = "household.radios[" + std::to_string(i) + "]";
    const auto label = "radio '" + radio.id + "': ";
    const auto prop =
        std::find_if(document.props.begin(), document.props.end(),
                     [&](const auto& v) { return v.id == radio.prop; });
    if (!levelEntryIdIsValid(radio.prop) || prop == document.props.end())
      error(p + ".prop", label + "unknown prop '" + radio.prop + "'");
    else if (prop->model != "apartment-radio" || !prop->collision_boxes.empty())
      error(p + ".prop",
            label + "requires apartment-radio with no collision boxes");
    if (!props.insert(radio.prop).second)
      error(p + ".prop",
            label + "prop '" + radio.prop + "' is owned by another radio");
    if (!sources.insert(radio.source).second)
      error(p + ".source",
            label + "source '" + radio.source + "' is owned by another radio");
    for (const auto& actor : document.characters.actors)
      if (actor.footstep_source == radio.source ||
          actor.interaction_source == radio.source)
        error(p + ".source", label + "source '" + radio.source +
                                 "' is owned by actor '" + actor.id + "'");
    const auto source = std::find_if(
        document.audio.sources.begin(), document.audio.sources.end(),
        [&](const auto& v) { return v.id == radio.source; });
    if (!levelEntryIdIsValid(radio.source) ||
        source == document.audio.sources.end()) {
      error(p + ".source", label + "unknown source '" + radio.source + "'");
      continue;
    }
    const auto* cue = findAudioCue(document.audio, source->cue);
    if (source->autoplay || !cue || !cue->spatial || !cue->loop ||
        cue->kind != AudioCueKind::Ambience || !cue->caption ||
        !audioCatalogContains(*cue->caption))
      error(p + ".source", label + "source '" + radio.source +
                               "' requires spatial non-autoplay looping "
                               "ambience with a valid caption");
  }
  return diagnostics;
}
