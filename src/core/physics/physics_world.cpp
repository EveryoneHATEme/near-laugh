#include "core/physics/physics_world.hpp"

// Jolt's configuration header must precede its other headers.
#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/IssueReporting.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/HeightFieldShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#include <array>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

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
  explicit Impl(const PrototypeLevel& level)
      : job_system_(JPH::cMaxPhysicsJobs),
        standing_shape_(makePlayerCapsule(player_standing_height)),
        crouched_shape_(makePlayerCapsule(player_crouched_height)) {
    if (!prototypeLevelIsValid(level)) {
      throw std::invalid_argument(
          "Physics world requires a valid immutable prototype level");
    }

    physics_system_.Init(256, 0, 1024, 1024, broad_phase_interface_,
                         object_vs_broad_phase_filter_,
                         object_layer_pair_filter_);
    physics_system_.SetGravity({0.0F, -18.0F, 0.0F});
    if (forcedFailureAt("world")) {
      throw std::runtime_error(
          "Physics initialization forced to fail after world creation");
    }

    static_solids_.reserve(level.solids().size() + 1);
    static_body_ids_.reserve(level.solids().size() + 2);
    try {
      const JPH::RefConst<JPH::Shape> terrain_shape =
          makeTerrainShape(level.terrain());
      JPH::BodyCreationSettings terrain_settings(
          terrain_shape, {0.0F, 0.0F, 0.0F}, JPH::Quat::sIdentity(),
          JPH::EMotionType::Static, layers::non_moving);
      const JPH::BodyID terrain_id =
          physics_system_.GetBodyInterface().CreateAndAddBody(
              terrain_settings, JPH::EActivation::DontActivate);
      if (terrain_id.IsInvalid()) {
        throw std::runtime_error("Create static terrain collision body failed");
      }
      static_body_ids_.push_back(terrain_id);
      terrain_collision_installed_ = true;
      if (forcedFailureAt("static-bodies")) {
        throw std::runtime_error(
            "Physics initialization forced to fail during static collision "
            "creation");
      }

      for (std::size_t solid_index = 0; solid_index < level.solids().size();
           ++solid_index) {
        const PrototypeSolid& solid = level.solids()[solid_index];
        JPH::BodyCreationSettings settings(
            new JPH::BoxShape({solid.half_extent.x, solid.half_extent.y,
                               solid.half_extent.z}),
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
      }
      const PrototypeStaticProp& prop = level.staticProp();
      const WorldPosition prop_center =
          prototypeStaticPropProxyWorldCenter(prop);
      const WorldExtent prop_half_extent =
          prototypeStaticPropProxyWorldHalfExtent(prop);
      JPH::BodyCreationSettings prop_settings(
          new JPH::BoxShape(
              {prop_half_extent.x, prop_half_extent.y, prop_half_extent.z}),
          {prop_center.x, prop_center.y, prop_center.z},
          JPH::Quat::sRotation(JPH::Vec3::sAxisY(),
                               JPH::DegreesToRadians(prop.yaw_degrees)),
          JPH::EMotionType::Static, layers::non_moving);
      prop_settings.mUserData = static_cast<JPH::uint64>(level.solids().size());
      const JPH::BodyID prop_id =
          physics_system_.GetBodyInterface().CreateAndAddBody(
              prop_settings, JPH::EActivation::DontActivate);
      if (prop_id.IsInvalid()) {
        throw std::runtime_error(
            "Create static prototype chair proxy body failed");
      }
      static_body_ids_.push_back(prop_id);
      static_solids_.push_back({prop_center, prop_half_extent,
                                PrototypeSolidKind::Obstacle,
                                prop.yaw_degrees});
      if (forcedFailureAt("model-proxy")) {
        throw std::runtime_error(
            "Physics initialization forced to fail after model proxy creation");
      }
      physics_system_.OptimizeBroadPhase();

      JPH::CharacterVirtualSettings character_settings;
      character_settings.mShape = standing_shape_;
      character_settings.mMaxSlopeAngle =
          JPH::DegreesToRadians(player_maximum_slope_degrees);
      character_settings.mSupportingVolume =
          JPH::Plane(JPH::Vec3::sAxisY(),
                     -(player_standing_height - player_capsule_radius));
      character_ = new JPH::CharacterVirtual(
          &character_settings,
          toJoltFootPosition(level.playerSpawn().foot_position),
          JPH::Quat::sIdentity(), 0, &physics_system_);
      if (forcedFailureAt("character")) {
        throw std::runtime_error(
            "Physics initialization forced to fail after character creation");
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

  PhysicsCharacterState stepCharacter(const PhysicsCharacterMotion& motion,
                                      float delta_seconds) {
    if (!(delta_seconds > 0.0F) || !std::isfinite(delta_seconds)) {
      throw std::invalid_argument(
          "Physics character step requires a finite positive delta");
    }

    applyRequestedStance(motion.crouch_requested);
    physics_system_.Update(delta_seconds, 1, &temp_allocator_, &job_system_);
    const JPH::Vec3 gravity{motion.gravity.x, motion.gravity.y,
                            motion.gravity.z};
    character_->SetLinearVelocity({motion.linear_velocity.x,
                                   motion.linear_velocity.y,
                                   motion.linear_velocity.z});
    JPH::CharacterVirtual::ExtendedUpdateSettings settings;
    settings.mWalkStairsStepUp = {0.0F, player_maximum_step_height, 0.0F};
    settings.mStickToFloorStepDown = {0.0F, -player_maximum_step_height, 0.0F};
    character_->ExtendedUpdate(
        delta_seconds, gravity, settings,
        physics_system_.GetDefaultBroadPhaseLayerFilter(layers::moving),
        physics_system_.GetDefaultLayerFilter(layers::moving), {}, {},
        temp_allocator_);
    return characterState();
  }

  PhysicsCharacterState characterState() const noexcept {
    return {fromJoltPosition(character_->GetPosition()),
            fromJoltVector(character_->GetLinearVelocity()),
            fromJoltGroundState(character_->GetGroundState()),
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

  void destroyStaticBodies() noexcept {
    JPH::BodyInterface& bodies = physics_system_.GetBodyInterface();
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
  std::vector<PhysicsStaticSolid> static_solids_{};
  JPH::Ref<JPH::CharacterVirtual> character_{};
  bool terrain_collision_installed_{};
  bool crouched_{};
};

PhysicsWorld::PhysicsWorld(const PrototypeLevel& level)
    : impl_(std::make_unique<Impl>(level)) {}

PhysicsWorld::~PhysicsWorld() = default;

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
