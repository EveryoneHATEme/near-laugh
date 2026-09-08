#include "core/physics/physics_world.hpp"

// Jolt's configuration header must precede its other headers.
// clang-format off
#include <Jolt/Jolt.h>
// clang-format on

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/IssueReporting.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/HeightFieldShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "core/world/characters.hpp"
#include "core/world/door.hpp"
#include "core/world/scene_assets.hpp"

namespace {
namespace layers {
constexpr JPH::ObjectLayer non_moving = 0;
constexpr JPH::ObjectLayer moving = 1;
constexpr JPH::ObjectLayer count = 2;
}  // namespace layers

namespace broad_phase_layers {
constexpr JPH::BroadPhaseLayer non_moving{0};
constexpr JPH::BroadPhaseLayer moving{1};
constexpr JPH::uint count = 2;
}  // namespace broad_phase_layers

void traceJolt(const char* format, ...) {
  std::va_list arguments;
  va_start(arguments, format);
  std::fputs("Jolt: ", stderr);
  std::vfprintf(stderr, format, arguments);
  std::fputc('\n', stderr);
  va_end(arguments);
}

#ifdef JPH_ENABLE_ASSERTS
bool reportJoltAssertion(const char* expression, const char* message,
                         const char* file, JPH::uint line) {
  std::fprintf(stderr, "Jolt assertion at %s:%u: %s%s%s\n", file, line,
               expression, message != nullptr ? " - " : "",
               message != nullptr ? message : "");
  return false;
}
#endif

bool forcedFailureAt(const char* stage) noexcept {
#if defined(_WIN32)
  char* requested = nullptr;
  std::size_t length = 0;
  if (_dupenv_s(&requested, &length,
                "NEAR_LAUGH_FORCE_PHYSICS_FAILURE_STAGE") != 0) {
    return false;
  }
  const bool matches = requested != nullptr && std::string{requested} == stage;
  std::free(requested);
  return matches;
#else
  const char* requested = std::getenv("NEAR_LAUGH_FORCE_PHYSICS_FAILURE_STAGE");
  return requested != nullptr && std::string{requested} == stage;
#endif
}

class JoltRuntime {
 public:
  JoltRuntime() {
    if (JPH::Factory::sInstance != nullptr) {
      throw std::runtime_error(
          "Jolt initialization failed: a physics runtime is already active");
    }

    JPH::RegisterDefaultAllocator();
    previous_trace_ = JPH::Trace;
    JPH::Trace = traceJolt;
#ifdef JPH_ENABLE_ASSERTS
    previous_assert_ = JPH::AssertFailed;
    JPH::AssertFailed = reportJoltAssertion;
#endif
    try {
      factory_ = std::make_unique<JPH::Factory>();
      JPH::Factory::sInstance = factory_.get();
      if (forcedFailureAt("runtime-factory")) {
        throw std::runtime_error(
            "Physics initialization forced to fail after Jolt factory setup");
      }
      JPH::RegisterTypes();
      types_registered_ = true;
    } catch (...) {
      cleanup();
      throw;
    }
  }

  ~JoltRuntime() { cleanup(); }

  JoltRuntime(const JoltRuntime&) = delete;
  JoltRuntime& operator=(const JoltRuntime&) = delete;

 private:
  void cleanup() noexcept {
    if (types_registered_) {
      JPH::UnregisterTypes();
      types_registered_ = false;
    }
    if (JPH::Factory::sInstance == factory_.get()) {
      JPH::Factory::sInstance = nullptr;
    }
    factory_.reset();
    JPH::Trace = previous_trace_;
#ifdef JPH_ENABLE_ASSERTS
    JPH::AssertFailed = previous_assert_;
#endif
  }

  JPH::TraceFunction previous_trace_{};
#ifdef JPH_ENABLE_ASSERTS
  JPH::AssertFailedFunction previous_assert_{};
#endif
  std::unique_ptr<JPH::Factory> factory_{};
  bool types_registered_{};
};

class ObjectLayerPairFilter final : public JPH::ObjectLayerPairFilter {
 public:
  bool ShouldCollide(JPH::ObjectLayer first,
                     JPH::ObjectLayer second) const override {
    if (first == layers::non_moving) {
      return second == layers::moving;
    }
    return first == layers::moving;
  }
};

class BroadPhaseLayerInterface final : public JPH::BroadPhaseLayerInterface {
 public:
  BroadPhaseLayerInterface() {
    object_to_broad_phase_[layers::non_moving] = broad_phase_layers::non_moving;
    object_to_broad_phase_[layers::moving] = broad_phase_layers::moving;
  }

  JPH::uint GetNumBroadPhaseLayers() const override {
    return broad_phase_layers::count;
  }

  JPH::BroadPhaseLayer GetBroadPhaseLayer(
      JPH::ObjectLayer layer) const override {
    if (layer >= layers::count) {
      return broad_phase_layers::non_moving;
    }
    return object_to_broad_phase_[layer];
  }

 private:
  std::array<JPH::BroadPhaseLayer, layers::count> object_to_broad_phase_{};
};

class ObjectVsBroadPhaseLayerFilter final
    : public JPH::ObjectVsBroadPhaseLayerFilter {
 public:
  bool ShouldCollide(JPH::ObjectLayer object_layer,
                     JPH::BroadPhaseLayer broad_phase_layer) const override {
    if (object_layer == layers::non_moving) {
      return broad_phase_layer == broad_phase_layers::moving;
    }
    return object_layer == layers::moving;
  }
};

class StaticVisibilityFilter final : public JPH::ObjectLayerFilter {
 public:
  bool ShouldCollide(JPH::ObjectLayer layer) const override {
    return layer == layers::non_moving;
  }
};

JPH::RVec3 toJoltFootPosition(WorldPosition position) {
  return {position.x, position.y, position.z};
}

PhysicsVector fromJoltVector(JPH::Vec3Arg vector) noexcept {
  return {vector.GetX(), vector.GetY(), vector.GetZ()};
}

PhysicsVector fromJoltPosition(JPH::RVec3Arg position) noexcept {
  return {static_cast<float>(position.GetX()),
          static_cast<float>(position.GetY()),
          static_cast<float>(position.GetZ())};
}

PhysicsGroundState fromJoltGroundState(
    JPH::CharacterBase::EGroundState state) noexcept {
  switch (state) {
    case JPH::CharacterBase::EGroundState::OnGround:
      return PhysicsGroundState::OnGround;
    case JPH::CharacterBase::EGroundState::OnSteepGround:
      return PhysicsGroundState::OnSteepGround;
    case JPH::CharacterBase::EGroundState::NotSupported:
      return PhysicsGroundState::Unsupported;
    case JPH::CharacterBase::EGroundState::InAir:
      return PhysicsGroundState::InAir;
  }
  return PhysicsGroundState::InAir;
}

JPH::RefConst<JPH::Shape> makePlayerCapsule(float total_height) {
  const float cylinder_half_height =
      (total_height - 2.0F * player_capsule_radius) * 0.5F;
  JPH::Ref<JPH::CapsuleShape> capsule =
      new JPH::CapsuleShape(cylinder_half_height, player_capsule_radius);
  JPH::RotatedTranslatedShapeSettings settings(
      JPH::Vec3{0.0F, total_height * 0.5F, 0.0F}, JPH::Quat::sIdentity(),
      capsule);
  auto result = settings.Create();
  if (result.HasError()) {
    throw std::runtime_error(std::string{"Create player capsule failed: "} +
                             result.GetError().c_str());
  }
  return result.Get();
}

JPH::RefConst<JPH::Shape> makeTerrainShape(const PrototypeTerrain& terrain) {
  JPH::HeightFieldShapeSettings settings(
      terrain.heights.data(),
      {terrain.origin.x, terrain.origin.y, terrain.origin.z},
      {terrain.sample_spacing, 1.0F, terrain.sample_spacing},
      static_cast<JPH::uint>(prototype_terrain_sample_count));
  settings.mBitsPerSample = 16;
  const JPH::ShapeSettings::ShapeResult result = settings.Create();
  if (result.HasError()) {
    throw std::runtime_error(
        std::string{"Create terrain heightfield failed: "} +
        result.GetError().c_str());
  }
  return result.Get();
}
}  // namespace

class PhysicsWorld::Impl {
 public:
  struct Actor {
    JPH::BodyID body;
    JPH::RefConst<JPH::Shape> shape;
    PhysicsActorState state;
    WorldPosition capsule_feet;
    WorldPosition previous_feet;
    float radius{};
    float height{};
    float support_height{};
  };

  class PlayerActorContacts final : public JPH::CharacterContactListener {
   public:
    explicit PlayerActorContacts(const std::vector<Actor>& actors)
        : actors_(actors) {}
    bool actorBody(JPH::BodyID body) const {
      if (body.IsInvalid()) return false;
      return std::any_of(actors_.begin(), actors_.end(),
                         [&](const auto& actor) { return actor.body == body; });
    }
    void OnContactSolve(const JPH::CharacterVirtual*, const JPH::BodyID& body,
                        const JPH::SubShapeID&, JPH::RVec3Arg position,
                        JPH::Vec3Arg normal, JPH::Vec3Arg,
                        const JPH::PhysicsMaterial*, JPH::Vec3Arg velocity,
                        JPH::Vec3& resolved) override {
      if (normal.GetY() <
          std::cos(JPH::DegreesToRadians(player_maximum_slope_degrees)))
        return;
      for (const auto& actor : actors_) {
        if (actor.body != body) continue;
        JPH::Vec3 away{float(position.GetX()) - actor.capsule_feet.x, 0,
                       float(position.GetZ()) - actor.capsule_feet.z};
        if (away.IsNearZero()) away = {velocity.GetX(), 0, velocity.GetZ()};
        away = away.NormalizedOr(JPH::Vec3::sAxisZ());
        const float speed =
            std::max(1.F, std::hypot(velocity.GetX(), velocity.GetZ()));
        resolved = away * speed;
        resolved.SetY(-(normal.GetX() * resolved.GetX() +
                        normal.GetZ() * resolved.GetZ()) /
                      normal.GetY());
        return;
      }
    }

   private:
    const std::vector<Actor>& actors_;
  };

  Impl(const PrototypeLevel& level, const LevelEntry& entry)
      : job_system_(JPH::cMaxPhysicsJobs),
        standing_shape_(makePlayerCapsule(player_standing_height)),
        crouched_shape_(makePlayerCapsule(player_crouched_height)),
        doors_(level.doors()),
        terrain_(level.terrain() ? &*level.terrain() : nullptr) {
    if (!prototypeLevelIsValid(level) || !level.entry(entry.id) ||
        *level.entry(entry.id) != entry) {
      throw std::invalid_argument(
          "Physics world requires a valid immutable prototype level");
    }

    physics_system_.Init(2048, 0, 4096, 4096, broad_phase_interface_,
                         object_vs_broad_phase_filter_,
                         object_layer_pair_filter_);
    physics_system_.SetGravity({0.0F, -18.0F, 0.0F});
    if (forcedFailureAt("world")) {
      throw std::runtime_error(
          "Physics initialization forced to fail after world creation");
    }

    std::size_t static_count = level.solids().size();
    for (const auto& prop : level.props())
      static_count += prop.collision_boxes.size();
    static_solids_.reserve(static_count);
    static_body_ids_.reserve(static_count + 1);
    door_body_ids_.reserve(doors_.size());
    door_angles_.reserve(doors_.size());
    actors_.resize(level.characters().actors.size());
    for (std::size_t i = 0; i < actors_.size(); ++i) actor_order_.push_back(i);
    std::sort(actor_order_.begin(), actor_order_.end(), [&](auto a, auto b) {
      return level.characters().actors[a].id < level.characters().actors[b].id;
    });
    try {
      if (level.terrain()) {
        const JPH::RefConst<JPH::Shape> terrain_shape =
            makeTerrainShape(*level.terrain());
        JPH::BodyCreationSettings terrain_settings(
            terrain_shape, {0.0F, 0.0F, 0.0F}, JPH::Quat::sIdentity(),
            JPH::EMotionType::Static, layers::non_moving);
        const JPH::BodyID terrain_id =
            physics_system_.GetBodyInterface().CreateAndAddBody(
                terrain_settings, JPH::EActivation::DontActivate);
        if (terrain_id.IsInvalid()) {
          throw std::runtime_error(
              "Create static terrain collision body failed");
        }
        static_body_ids_.push_back(terrain_id);
        terrain_collision_installed_ = true;
      }

      for (std::size_t solid_index = 0; solid_index < level.solids().size();
           ++solid_index) {
        const PrototypeSolid& solid = level.solids()[solid_index];
        JPH::BodyCreationSettings settings(
            new JPH::BoxShape(
                {solid.half_extent.x, solid.half_extent.y, solid.half_extent.z},
                0),
            {solid.center.x, solid.center.y, solid.center.z},
            JPH::Quat::sIdentity(), JPH::EMotionType::Static,
            layers::non_moving);
        settings.mUserData = static_cast<JPH::uint64>(solid_index);
        const JPH::BodyID id =
            physics_system_.GetBodyInterface().CreateAndAddBody(
                settings, JPH::EActivation::DontActivate);
        if (id.IsInvalid()) {
          throw std::runtime_error(
              "Create static prototype collision body failed");
        }
        static_body_ids_.push_back(id);
        static_solids_.push_back(
            {solid.center, solid.half_extent, solid.kind, 0.0F});
        if (forcedFailureAt("static-bodies")) {
          throw std::runtime_error(
              "Physics initialization forced to fail during static collision "
              "creation");
        }
      }
      support_body_count_ = static_body_ids_.size();
      for (const auto& prop : level.props()) {
        for (const auto& box : prop.collision_boxes) {
          const WorldPosition prop_center = propBoxWorldCenter(prop, box);
          const WorldExtent prop_half_extent =
              propBoxWorldHalfExtent(prop, box);
          JPH::BodyCreationSettings prop_settings(
              new JPH::BoxShape(
                  {prop_half_extent.x, prop_half_extent.y, prop_half_extent.z},
                  0),
              {prop_center.x, prop_center.y, prop_center.z},
              JPH::Quat::sRotation(JPH::Vec3::sAxisY(),
                                   JPH::DegreesToRadians(prop.yaw_degrees)),
              JPH::EMotionType::Static, layers::non_moving);
          prop_settings.mUserData =
              static_cast<JPH::uint64>(static_solids_.size());
          const JPH::BodyID prop_id =
              physics_system_.GetBodyInterface().CreateAndAddBody(
                  prop_settings, JPH::EActivation::DontActivate);
          if (prop_id.IsInvalid()) {
            throw std::runtime_error("Create static prop proxy body failed: " +
                                     prop.id);
          }
          static_body_ids_.push_back(prop_id);
          static_solids_.push_back({prop_center, prop_half_extent,
                                    PrototypeSolidKind::Obstacle,
                                    prop.yaw_degrees});
          if (forcedFailureAt("model-proxy")) {
            throw std::runtime_error(
                "Physics initialization forced to fail after model proxy "
                "creation");
          }
        }
      }
      for (const auto& door : doors_) {
        const auto angle = doorInitialAngle(door);
        const auto pose = doorLeafPose(door, angle);
        JPH::BodyCreationSettings settings(
            new JPH::BoxShape(
                {pose.half_extent.x, pose.half_extent.y, pose.half_extent.z},
                0),
            {pose.center.x, pose.center.y, pose.center.z},
            JPH::Quat::sRotation(JPH::Vec3::sAxisY(),
                                 JPH::DegreesToRadians(pose.yaw_degrees)),
            JPH::EMotionType::Kinematic, layers::moving);
        const auto id = physics_system_.GetBodyInterface().CreateAndAddBody(
            settings, JPH::EActivation::DontActivate);
        if (id.IsInvalid())
          throw std::runtime_error("Create door collision failed: " + door.id);
        door_body_ids_.push_back(id);
        door_angles_.push_back(angle);
      }
      if (!door_body_ids_.empty() && forcedFailureAt("door-bodies"))
        throw std::runtime_error(
            "Physics initialization forced to fail after door bodies");
      physics_system_.OptimizeBroadPhase();

      JPH::CharacterVirtualSettings character_settings;
      character_settings.mShape = standing_shape_;
      character_settings.mCharacterPadding = prototype_player_contact_padding;
      character_settings.mMaxSlopeAngle =
          JPH::DegreesToRadians(player_maximum_slope_degrees);
      character_settings.mSupportingVolume =
          JPH::Plane(JPH::Vec3::sAxisY(),
                     -(player_standing_height - player_capsule_radius));
      character_ = new JPH::CharacterVirtual(
          &character_settings, toJoltFootPosition(entry.pose.foot_position),
          JPH::Quat::sIdentity(), 0, &physics_system_);
      character_->SetListener(&player_actor_contacts_);
      previous_character_ = characterState();
      if (forcedFailureAt("character")) {
        throw std::runtime_error(
            "Physics initialization forced to fail after character creation");
      }
      for (std::size_t i = 0; i < actors_.size(); ++i) {
        const auto& definition = level.characters().actors[i];
        const auto& model = *findCharacterModel(definition.model);
        const auto& mark =
            *findCharacterMark(level.characters(), definition.initial_mark);
        auto& actor = actors_[i];
        actor.state = {mark.feet_position, mark.yaw_degrees};
        actor.radius = model.capsule_radius_m;
        actor.height = model.capsule_height_m;
        actor.support_height = mark.feet_position.y;
        actor.capsule_feet = mark.feet_position;
        actor.capsule_feet.y += prototypeActorCapsuleOffset(
            terrain_, mark.feet_position, actor.radius);
        actor.previous_feet = actor.capsule_feet;
        actor.shape = new JPH::CapsuleShape(
            model.capsule_height_m / 2 - model.capsule_radius_m,
            model.capsule_radius_m);
        JPH::BodyCreationSettings settings(
            actor.shape,
            {actor.capsule_feet.x,
             actor.capsule_feet.y + model.capsule_height_m / 2,
             actor.capsule_feet.z},
            JPH::Quat::sIdentity(), JPH::EMotionType::Kinematic,
            layers::moving);
        actor.body = physics_system_.GetBodyInterface().CreateAndAddBody(
            settings, JPH::EActivation::DontActivate);
        if (actor.body.IsInvalid())
          throw std::runtime_error("Create actor collision failed: " +
                                   definition.id);
        const auto stage = "actor-body-" + std::to_string(i + 1);
        if (forcedFailureAt(stage.c_str()))
          throw std::runtime_error(
              "Physics initialization forced to fail after actor: " +
              definition.id);
      }
    } catch (...) {
      character_ = nullptr;
      destroyStaticBodies();
      throw;
    }
  }

  ~Impl() {
    character_ = nullptr;
    destroyStaticBodies();
  }

  void advanceWorld(float delta_seconds) {
    if (!(delta_seconds > 0.0F) || !std::isfinite(delta_seconds))
      throw std::invalid_argument(
          "Physics world step requires a finite positive delta");
    physics_system_.Update(delta_seconds, 1, &temp_allocator_, &job_system_);
    for (auto& actor : actors_) actor.previous_feet = actor.capsule_feet;
  }

  PhysicsCharacterState stepCharacter(const PhysicsCharacterMotion& motion,
                                      float delta_seconds) {
    if (!(delta_seconds > 0.0F) || !std::isfinite(delta_seconds)) {
      throw std::invalid_argument(
          "Physics character step requires a finite positive delta");
    }

    previous_character_ = characterState();
    applyRequestedStance(motion.crouch_requested);
    const JPH::Vec3 gravity{motion.gravity.x, motion.gravity.y,
                            motion.gravity.z};
    character_->SetLinearVelocity({motion.linear_velocity.x,
                                   motion.linear_velocity.y,
                                   motion.linear_velocity.z});
    JPH::CharacterVirtual::ExtendedUpdateSettings settings;
    settings.mWalkStairsStepUp = {0.0F, player_maximum_step_height, 0.0F};
    settings.mStickToFloorStepDown = {0.0F, -player_maximum_step_height, 0.0F};
    if (player_actor_contacts_.actorBody(character_->GetGroundBodyID())) {
      settings.mWalkStairsStepUp = JPH::Vec3::sZero();
      settings.mStickToFloorStepDown = JPH::Vec3::sZero();
    }
    character_->ExtendedUpdate(
        delta_seconds, gravity, settings,
        physics_system_.GetDefaultBroadPhaseLayerFilter(layers::moving),
        physics_system_.GetDefaultLayerFilter(layers::moving), {}, {},
        temp_allocator_);
    // A stair probe must not install an actor's head as a new support surface.
    // Falling contacts retain collision and slide off through the listener.
    if (previous_character_.supported() &&
        player_actor_contacts_.actorBody(character_->GetGroundBodyID()) &&
        character_->GetPosition().GetY() >
            previous_character_.foot_position.y + .0001F) {
      const auto p = previous_character_.foot_position;
      character_->SetPosition({p.x, p.y, p.z});
      character_->SetLinearVelocity(JPH::Vec3::sZero());
      character_->RefreshContacts(
          physics_system_.GetDefaultBroadPhaseLayerFilter(layers::moving),
          physics_system_.GetDefaultLayerFilter(layers::moving), {}, {},
          temp_allocator_);
    }
    return characterState();
  }

  PhysicsCharacterState characterState() const noexcept {
    return {fromJoltPosition(character_->GetPosition()),
            fromJoltVector(character_->GetLinearVelocity()),
            player_actor_contacts_.actorBody(character_->GetGroundBodyID())
                ? PhysicsGroundState::Unsupported
                : fromJoltGroundState(character_->GetGroundState()),
            crouched_ ? PhysicsPlayerStance::Crouched
                      : PhysicsPlayerStance::Standing};
  }

  void applyRequestedStance(bool crouch_requested) {
    if (crouch_requested == crouched_) {
      return;
    }
    const JPH::Shape* requested_shape =
        crouch_requested ? crouched_shape_ : standing_shape_;
    const float allowed_penetration =
        crouch_requested
            ? std::numeric_limits<float>::max()
            : 1.5F * physics_system_.GetPhysicsSettings().mPenetrationSlop;
    if (character_->SetShape(
            requested_shape, allowed_penetration,
            physics_system_.GetDefaultBroadPhaseLayerFilter(layers::moving),
            physics_system_.GetDefaultLayerFilter(layers::moving), {}, {},
            temp_allocator_)) {
      crouched_ = crouch_requested;
    }
  }

  DoorLeafPose playerEnvelope() const {
    const auto now = characterState();
    const auto a = previous_character_.foot_position;
    const auto b = now.foot_position;
    const float height =
        previous_character_.stance == PhysicsPlayerStance::Standing ||
                now.stance == PhysicsPlayerStance::Standing
            ? player_standing_height
            : player_crouched_height;
    // CharacterVirtual raises the shape by padding and keeps an additional
    // padding skin around it. Include both the collider and its contact skin.
    const float padding = character_->GetCharacterPadding();
    const float radius = player_capsule_radius + padding;
    const float lo = std::min(a.y, b.y);
    const float hi = std::max(a.y, b.y) + height + 2 * padding;
    return {{(a.x + b.x) / 2, (lo + hi) / 2, (a.z + b.z) / 2},
            {std::abs(a.x - b.x) / 2 + radius, (hi - lo) / 2,
             std::abs(a.z - b.z) / 2 + radius},
            0};
  }

  DoorLeafPose actorEnvelope(const Actor& actor, WorldPosition from,
                             WorldPosition to) const {
    const float lo = std::min(from.y, to.y);
    const float hi = std::max(from.y, to.y) + actor.height;
    return {{(from.x + to.x) / 2, (lo + hi) / 2, (from.z + to.z) / 2},
            {std::abs(from.x - to.x) / 2 + actor.radius, (hi - lo) / 2,
             std::abs(from.z - to.z) / 2 + actor.radius},
            0};
  }

  struct ActorCast {
    float fraction{1};
    JPH::BodyID body;
    float normal_y{};
    float support_height{};
    PhysicsActorObstruction obstruction{PhysicsActorObstruction::None};
  };

  bool supportBody(JPH::BodyID id) const {
    return std::find(static_body_ids_.begin(),
                     static_body_ids_.begin() + support_body_count_,
                     id) != static_body_ids_.begin() + support_body_count_;
  }

  ActorCast castActor(std::size_t index, WorldPosition from, WorldPosition to,
                      bool support_only = false) const {
    const auto& actor = actors_[index];
    const JPH::Vec3 delta{to.x - from.x, to.y - from.y, to.z - from.z};
    const JPH::RVec3 center{from.x, from.y + actor.height / 2, from.z};
    JPH::ShapeCastSettings settings;
    settings.mReturnDeepestPoint = true;
    settings.mUseShrunkenShapeAndConvexRadius = true;
    settings.mActiveEdgeMode = JPH::EActiveEdgeMode::CollideWithAll;
    settings.SetBackFaceMode(JPH::EBackFaceMode::CollideWithBackFaces);
    JPH::AllHitCollisionCollector<JPH::CastShapeCollector> hits;
    physics_system_.GetNarrowPhaseQuery().CastShape(
        {actor.shape, JPH::Vec3::sReplicate(1),
         JPH::RMat44::sTranslation(center), delta},
        settings, center, hits, {}, {},
        JPH::IgnoreSingleBodyFilter(actor.body));
    ActorCast result;
    for (const auto& hit : hits.mHits) {
      if (support_only && !supportBody(hit.mBodyID2)) continue;
      const auto normal =
          -hit.mPenetrationAxis.NormalizedOr(JPH::Vec3::sAxisY());
      // Resting/tangential contacts must not prevent horizontal travel or
      // moving away. Initial penetration blocks movement farther inward.
      if (hit.mFraction == 0 &&
          (delta.Dot(normal) > 1.0e-6F ||
           (hit.mPenetrationDepth <= .001F && delta.Dot(normal) >= -1.0e-6F)))
        continue;
      if (hit.mFraction > result.fraction) continue;
      result = {std::max(0.F, hit.mFraction), hit.mBodyID2, normal.GetY(), 0,
                PhysicsActorObstruction::Static};
      if (std::find(door_body_ids_.begin(), door_body_ids_.end(),
                    hit.mBodyID2) != door_body_ids_.end())
        result.obstruction = PhysicsActorObstruction::Door;
      for (const auto& other : actors_)
        if (other.body == hit.mBodyID2)
          result.obstruction = PhysicsActorObstruction::Actor;
    }
    if (support_only) {
      if (terrain_ && result.body == static_body_ids_.front()) {
        if (prototypeTerrainContains(*terrain_, from.x, from.z))
          result.support_height =
              prototypeTerrainHeightAt(*terrain_, from.x, from.z);
        else
          result.support_height = from.y + (to.y - from.y) * result.fraction;
      } else {
        const auto body = std::find(
            static_body_ids_.begin(),
            static_body_ids_.begin() + support_body_count_, result.body);
        if (body != static_body_ids_.begin() + support_body_count_) {
          const auto& solid =
              static_solids_[std::size_t(body - static_body_ids_.begin()) -
                             (terrain_ ? 1 : 0)];
          result.support_height = solid.center.y + solid.half_extent.y;
        }
      }
      return result;
    }
    const auto blocked = [&](float fraction) {
      const WorldPosition end{from.x + delta.GetX() * fraction,
                              from.y + delta.GetY() * fraction,
                              from.z + delta.GetZ() * fraction};
      const auto envelope = actorEnvelope(actor, from, end);
      if (yawedBoxesOverlap(envelope, playerEnvelope(), -0.0001F))
        return PhysicsActorObstruction::Player;
      for (std::size_t i = 0; i < actors_.size(); ++i) {
        if (i == index) continue;
        if (yawedBoxesOverlap(
                envelope,
                actorEnvelope(actors_[i], actors_[i].previous_feet,
                              actors_[i].capsule_feet),
                -0.0001F))
          return PhysicsActorObstruction::Actor;
      }
      return PhysicsActorObstruction::None;
    };
    if (const auto obstruction = blocked(result.fraction);
        obstruction != PhysicsActorObstruction::None) {
      float lo = 0, hi = result.fraction;
      for (int i = 0; i < 14; ++i) {
        const float mid = (lo + hi) / 2;
        if (blocked(mid) == PhysicsActorObstruction::None)
          lo = mid;
        else
          hi = mid;
      }
      result.fraction = lo;
      result.obstruction = obstruction;
    }
    return result;
  }

  PhysicsActorAdvance advanceActor(std::size_t index,
                                   WorldPosition displacement, float yaw) {
    auto& actor = actors_.at(index);
    const float distance = std::hypot(displacement.x, displacement.z);
    if (!std::isfinite(distance) || distance > 10 || displacement.y != 0 ||
        !std::isfinite(yaw))
      throw std::invalid_argument(
          "Actor movement requires finite horizontal displacement within 10 m "
          "and yaw");
    PhysicsActorAdvance result{actor.state};
    WorldPosition capsule_feet = actor.capsule_feet;
    float support_height = actor.support_height;
    result.state.yaw_degrees = yaw;
    const int intervals = int(std::ceil(distance / .025F));
    const WorldPosition part{intervals ? displacement.x / intervals : 0, 0,
                             intervals ? displacement.z / intervals : 0};
    const auto interpolate = [](WorldPosition a, WorldPosition b,
                                float fraction) {
      return WorldPosition{a.x + (b.x - a.x) * fraction,
                           a.y + (b.y - a.y) * fraction,
                           a.z + (b.z - a.z) * fraction};
    };
    const auto land = [&](WorldPosition from, float lowest,
                          WorldPosition& accepted, bool allow_edge,
                          float* accepted_height = nullptr) {
      const WorldPosition bottom{from.x, lowest, from.z};
      const auto support = castActor(index, from, bottom, true);
      if (support.obstruction == PhysicsActorObstruction::None) return false;
      if (std::abs(support.support_height - support_height) >
          player_maximum_step_height + .0001F)
        return false;
      if (support.normal_y <
              std::cos(JPH::DegreesToRadians(player_maximum_slope_degrees)) &&
          !allow_edge)
        return false;
      const auto landing = interpolate(from, bottom, support.fraction);
      const auto clearance = castActor(index, from, landing);
      if (clearance.fraction < .999F) return false;
      accepted = landing;
      if (accepted_height) *accepted_height = support.support_height;
      return true;
    };
    for (int step = 0; step < intervals; ++step) {
      const auto start = capsule_feet;
      const WorldPosition target{start.x + part.x, start.y, start.z + part.z};
      const auto forward = castActor(index, start, target);
      const float part_length = std::hypot(part.x, part.z);
      const float safe =
          forward.obstruction == PhysicsActorObstruction::None
              ? 1.F
              // Retain 1 mm clearance, above door-envelope numeric margins.
              : std::max(0.F, forward.fraction - .001F / part_length);
      auto horizontal = interpolate(start, target, safe);
      WorldPosition accepted = start;
      float next_support_height = support_height;
      bool grounded = land({horizontal.x, horizontal.y + .001F, horizontal.z},
                           start.y - player_maximum_step_height, accepted,
                           false, &next_support_height);
      bool stepped = false;
      if (safe < 1 || !grounded) {
        const WorldPosition up{start.x, start.y + player_maximum_step_height,
                               start.z};
        const WorldPosition across{target.x, up.y, target.z};
        if (castActor(index, start, up).obstruction ==
                PhysicsActorObstruction::None &&
            castActor(index, up, across).obstruction ==
                PhysicsActorObstruction::None) {
          WorldPosition landing;
          float stepped_support_height{};
          bool edge_supported = false;
          const WorldPosition test{up.x + part.x / part_length * .15F, up.y,
                                   up.z + part.z / part_length * .15F};
          WorldPosition test_landing;
          if (castActor(index, up, test).obstruction ==
              PhysicsActorObstruction::None)
            edge_supported = land(test, start.y - player_maximum_step_height,
                                  test_landing, false);
          if (land(across, start.y - player_maximum_step_height, landing,
                   edge_supported, &stepped_support_height) &&
              landing.y <= start.y + player_maximum_step_height + .001F) {
            accepted = landing;
            next_support_height = stepped_support_height;
            grounded = true;
            stepped = true;
          }
        }
      }
      if (!grounded) {
        result.obstruction = PhysicsActorObstruction::Support;
        break;
      }
      const float travel =
          std::hypot(accepted.x - start.x, accepted.z - start.z);
      result.horizontal_distance += travel;
      if (travel > 0) {
        capsule_feet = accepted;
        support_height = next_support_height;
      }
      if (safe < 1 && !stepped) {
        result.obstruction = forward.obstruction;
        break;
      }
    }
    result.state.feet_position = capsule_feet;
    if (terrain_ &&
        prototypeTerrainContains(*terrain_, capsule_feet.x, capsule_feet.z)) {
      auto ground = capsule_feet;
      ground.y = prototypeTerrainHeightAt(*terrain_, ground.x, ground.z);
      const float offset =
          prototypeActorCapsuleOffset(terrain_, ground, actor.radius);
      if (std::abs(capsule_feet.y - ground.y - offset) < .003F)
        result.state.feet_position = ground;
    }
    actor.state = result.state;
    actor.capsule_feet = capsule_feet;
    actor.support_height = support_height;
    const auto p = capsule_feet;
    physics_system_.GetBodyInterface().SetPositionAndRotation(
        actor.body, {p.x, p.y + actor.height / 2, p.z}, JPH::Quat::sIdentity(),
        JPH::EActivation::DontActivate);
    return result;
  }

  bool doorIntervalClear(std::size_t index, float from, float to,
                         unsigned refinement = 0) const {
    const auto& door = doors_[index];
    const float midpoint = (from + to) / 2;
    WorldPosition lo{std::numeric_limits<float>::infinity(), 0,
                     std::numeric_limits<float>::infinity()};
    WorldPosition hi{-lo.x, door.height, -lo.z};
    for (float angle : {from, to})
      for (auto corner : doorCorners(door, angle)) {
        const auto p = doorLocalPoint(door, midpoint, corner);
        lo.x = std::min(lo.x, p.x);
        lo.z = std::min(lo.z, p.z);
        hi.x = std::max(hi.x, p.x);
        hi.z = std::max(hi.z, p.z);
      }
    const double half_angle =
        std::abs(double(to) - from) * std::numbers::pi / 360;
    const float margin =
        float(std::hypot(double(door.width), double(door.thickness) / 2) *
              (1 - std::cos(half_angle))) +
        0.0001F;
    const auto center =
        doorWorldPoint(door, midpoint,
                       {(lo.x + hi.x) / 2, door.height / 2, (lo.z + hi.z) / 2});
    const DoorLeafPose envelope{center,
                                {(hi.x - lo.x) / 2 + margin, door.height / 2,
                                 (hi.z - lo.z) / 2 + margin},
                                doorLeafPose(door, midpoint).yaw_degrees};
    if (yawedBoxesOverlap(envelope, playerEnvelope(), -0.0001F)) return false;
    // A resting actor can be closer than the one-degree envelope's excess.
    // Prove both smaller swept intervals clear before allowing that rotation;
    // refining only the stop angle would leave an opening-away door stuck.
    const auto refine_actor_contact = [&] {
      return refinement < 12 && midpoint != from && midpoint != to &&
             doorIntervalClear(index, from, midpoint, refinement + 1) &&
             doorIntervalClear(index, midpoint, to, refinement + 1);
    };
    for (const auto& actor : actors_)
      if (yawedBoxesOverlap(
              envelope,
              actorEnvelope(actor, actor.previous_feet, actor.capsule_feet),
              -0.0001F))
        return refine_actor_contact();
    JPH::BoxShape shape({envelope.half_extent.x, envelope.half_extent.y,
                         envelope.half_extent.z},
                        0);
    const JPH::RVec3 position{center.x, center.y, center.z};
    const auto rotation = JPH::Quat::sRotation(
        JPH::Vec3::sAxisY(), JPH::DegreesToRadians(envelope.yaw_degrees));
    JPH::CollideShapeSettings settings;
    settings.mBackFaceMode = JPH::EBackFaceMode::CollideWithBackFaces;
    settings.mActiveEdgeMode = JPH::EActiveEdgeMode::CollideWithAll;
    JPH::ClosestHitCollisionCollector<JPH::CollideShapeCollector> collector;
    physics_system_.GetNarrowPhaseQuery().CollideShape(
        &shape, JPH::Vec3::sReplicate(1),
        JPH::RMat44::sRotationTranslation(rotation, position), settings,
        position, collector, {}, {},
        JPH::IgnoreSingleBodyFilter(door_body_ids_[index]));
    if (!collector.HadHit() || collector.mHit.mPenetrationDepth < 0)
      return true;
    if (std::any_of(actors_.begin(), actors_.end(), [&](const Actor& actor) {
          return actor.body == collector.mHit.mBodyID2;
        }))
      return refine_actor_contact();
    return false;
  }

  PhysicsDoorAdvance advanceDoor(std::size_t index, float target) {
    if (index >= doors_.size())
      throw std::out_of_range("Unknown door collision index");
    const auto& door = doors_[index];
    if (!std::isfinite(target) ||
        target < std::min(0.0F, door.open_angle_degrees) ||
        target > std::max(0.0F, door.open_angle_degrees))
      throw std::invalid_argument("Door angle exceeds authored limits");
    float angle = door_angles_[index];
    bool blocked = false;
    const int intervals = int(std::ceil(std::abs(target - angle)));
    for (int i = 0; i < intervals; ++i) {
      const float end = i + 1 == intervals
                            ? target
                            : angle + std::copysign(1.0F, target - angle);
      if (doorIntervalClear(index, angle, end)) {
        angle = end;
        continue;
      }
      float clear = angle, unsafe = end;
      const float radius = std::hypot(door.width, door.thickness / 2);
      for (int j = 0;
           j < 12 &&
           std::abs(unsafe - clear) * std::numbers::pi_v<float> / 180 * radius >
               0.001F;
           ++j) {
        const float middle = (clear + unsafe) / 2;
        if (doorIntervalClear(index, clear, middle))
          clear = middle;
        else
          unsafe = middle;
      }
      angle = clear;
      blocked = true;
      break;
    }
    if (angle != door_angles_[index]) {
      const auto pose = doorLeafPose(door, angle);
      physics_system_.GetBodyInterface().SetPositionAndRotation(
          door_body_ids_[index], {pose.center.x, pose.center.y, pose.center.z},
          JPH::Quat::sRotation(JPH::Vec3::sAxisY(),
                               JPH::DegreesToRadians(pose.yaw_degrees)),
          JPH::EActivation::DontActivate);
      door_angles_[index] = angle;
    }
    return {angle, blocked};
  }

  void destroyStaticBodies() noexcept {
    JPH::BodyInterface& bodies = physics_system_.GetBodyInterface();
    for (auto& actor : actors_) {
      if (!actor.body.IsInvalid()) {
        bodies.RemoveBody(actor.body);
        bodies.DestroyBody(actor.body);
        actor.body = JPH::BodyID();
      }
    }
    actors_.clear();
    for (const auto id : door_body_ids_) {
      bodies.RemoveBody(id);
      bodies.DestroyBody(id);
    }
    door_body_ids_.clear();
    door_angles_.clear();
    for (const JPH::BodyID id : static_body_ids_) {
      bodies.RemoveBody(id);
      bodies.DestroyBody(id);
    }
    static_body_ids_.clear();
    static_solids_.clear();
    terrain_collision_installed_ = false;
  }

  JoltRuntime runtime_{};
  BroadPhaseLayerInterface broad_phase_interface_{};
  ObjectVsBroadPhaseLayerFilter object_vs_broad_phase_filter_{};
  ObjectLayerPairFilter object_layer_pair_filter_{};
  JPH::TempAllocatorImpl temp_allocator_{4 * 1024 * 1024};
  JPH::JobSystemSingleThreaded job_system_;
  JPH::PhysicsSystem physics_system_{};
  JPH::RefConst<JPH::Shape> standing_shape_{};
  JPH::RefConst<JPH::Shape> crouched_shape_{};
  std::vector<JPH::BodyID> static_body_ids_{};
  std::size_t support_body_count_{};
  std::vector<PhysicsStaticSolid> static_solids_{};
  const std::vector<DoorDefinition>& doors_;
  const PrototypeTerrain* terrain_;
  std::vector<JPH::BodyID> door_body_ids_{};
  std::vector<float> door_angles_{};
  std::vector<Actor> actors_{};
  std::vector<std::size_t> actor_order_{};
  PlayerActorContacts player_actor_contacts_{actors_};
  PhysicsCharacterState previous_character_{};
  JPH::Ref<JPH::CharacterVirtual> character_{};
  bool terrain_collision_installed_{};
  bool crouched_{};
};

PhysicsWorld::PhysicsWorld(const PrototypeLevel& level)
    : PhysicsWorld(level, *level.entry(level.defaultEntryId())) {}

PhysicsWorld::PhysicsWorld(const PrototypeLevel& level, const LevelEntry& entry)
    : impl_(std::make_unique<Impl>(level, entry)) {}

PhysicsWorld::~PhysicsWorld() = default;

void PhysicsWorld::advanceWorld(float delta_seconds) {
  impl_->advanceWorld(delta_seconds);
}

PhysicsCharacterState PhysicsWorld::stepCharacter(
    const PhysicsCharacterMotion& motion, float delta_seconds) {
  return impl_->stepCharacter(motion, delta_seconds);
}

PhysicsCharacterState PhysicsWorld::characterState() const noexcept {
  return impl_->characterState();
}

std::size_t PhysicsWorld::staticBodyCount() const noexcept {
  return impl_->static_solids_.size();
}

PhysicsStaticSolid PhysicsWorld::staticBody(std::size_t index) const {
  if (index >= impl_->static_solids_.size()) {
    throw std::out_of_range("Physics static body index is out of range");
  }
  return impl_->static_solids_[index];
}

bool PhysicsWorld::hasTerrainCollision() const noexcept {
  return impl_->terrain_collision_installed_;
}

bool PhysicsWorld::usesSingleThreadedJobs() const noexcept {
  return impl_->job_system_.GetMaxConcurrency() == 1;
}

bool PhysicsWorld::staticSegmentBlocked(WorldPosition origin,
                                        WorldPosition endpoint) const {
  const JPH::RVec3 start{origin.x, origin.y, origin.z};
  const JPH::Vec3 delta{endpoint.x - origin.x, endpoint.y - origin.y,
                        endpoint.z - origin.z};
  const float length = delta.Length();
  if (!std::isfinite(origin.x) || !std::isfinite(origin.y) ||
      !std::isfinite(origin.z) || !std::isfinite(length) || !(length > 0.0F))
    return true;
  const JPH::RRayCast ray{start, delta * (1.0F + 0.0001F / length)};
  JPH::RayCastResult hit;
  // The closest-hit overload treats convex shapes as solid at the origin
  // and includes back-facing terrain triangles. CharacterVirtual has no
  // static body; the explicit layer filter also excludes moving bodies.
  return impl_->physics_system_.GetNarrowPhaseQuery().CastRay(
      ray, hit, {}, StaticVisibilityFilter{});
}

bool PhysicsWorld::worldSegmentBlocked(WorldPosition origin,
                                       WorldPosition endpoint,
                                       std::string_view selected_door) const {
  if (staticSegmentBlocked(origin, endpoint)) return true;
  for (std::size_t i = 0; i < impl_->doors_.size(); ++i) {
    const auto& door = impl_->doors_[i];
    if (doorPointInside(door, impl_->door_angles_[i], origin)) return true;
    if (door.id == selected_door) continue;
    const WorldPosition delta{endpoint.x - origin.x, endpoint.y - origin.y,
                              endpoint.z - origin.z};
    const float length = std::hypot(delta.x, delta.y, delta.z);
    const auto distance =
        doorRayDistance(door, impl_->door_angles_[i], origin, delta);
    if (distance && *distance <= length + 0.0001F) return true;
  }
  const JPH::RVec3 start{origin.x, origin.y, origin.z};
  const JPH::Vec3 delta{endpoint.x - origin.x, endpoint.y - origin.y,
                        endpoint.z - origin.z};
  JPH::BodyID selected;
  for (std::size_t i = 0; i < impl_->doors_.size(); ++i)
    if (impl_->doors_[i].id == selected_door)
      selected = impl_->door_body_ids_[i];
  JPH::RayCastResult hit;
  return impl_->physics_system_.GetNarrowPhaseQuery().CastRay(
      {start, delta * (1.F + .0001F / delta.Length())}, hit, {}, {},
      JPH::IgnoreSingleBodyFilter(selected));
}

PhysicsDoorAdvance PhysicsWorld::advanceDoor(std::size_t index,
                                             float requested_angle) {
  return impl_->advanceDoor(index, requested_angle);
}

float PhysicsWorld::doorAngle(std::size_t index) const {
  return impl_->door_angles_.at(index);
}

std::size_t PhysicsWorld::doorCount() const noexcept {
  return impl_->doors_.size();
}

std::size_t PhysicsWorld::actorCount() const noexcept {
  return impl_->actors_.size();
}

PhysicsActorState PhysicsWorld::actorState(std::size_t index) const {
  return impl_->actors_.at(index).state;
}

PhysicsActorAdvance PhysicsWorld::advanceActor(std::size_t index,
                                               WorldPosition displacement,
                                               float yaw_degrees) {
  return impl_->advanceActor(index, displacement, yaw_degrees);
}

std::vector<PhysicsActorAdvance> PhysicsWorld::advanceActors(
    std::span<const PhysicsActorMotion> motions) {
  if (motions.size() != actorCount())
    throw std::invalid_argument(
        "Actor motion count does not match selected actors");
  for (const auto& motion : motions) {
    const auto p = motion.displacement;
    const auto distance = std::hypot(p.x, p.z);
    if (!std::isfinite(distance) || distance > 10 || p.y != 0 ||
        !std::isfinite(motion.yaw_degrees))
      throw std::invalid_argument("Invalid actor motion batch");
  }
  std::vector<PhysicsActorAdvance> results(actorCount());
  for (auto i : impl_->actor_order_)
    results[i] =
        advanceActor(i, motions[i].displacement, motions[i].yaw_degrees);
  return results;
}
