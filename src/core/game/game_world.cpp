#include "game_world.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace {

Color shade(Color color, float factor) {
  factor = math::clamp(factor, 0.0f, 1.0f);
  return {static_cast<uint8_t>(color.r * factor),
          static_cast<uint8_t>(color.g * factor),
          static_cast<uint8_t>(color.b * factor),
          color.a};
}

struct ViewVertex {
  math::Vec3 view{};
  Color color{};
};

ViewVertex interpolate(const ViewVertex& from, const ViewVertex& to, float t) {
  return {from.view + (to.view - from.view) * t, from.color};
}

class FrameBuilder {
 private:
  const Camera& camera;
  float aspect_ratio;
  RenderPacket& packet;
  math::Vec3 camera_right;
  math::Vec3 camera_up;
  math::Vec3 camera_forward;
  float scale_x{};
  float scale_y{};

  math::Vec3 toViewSpace(const math::Vec3& world_position) const {
    const math::Vec3 relative = world_position - camera.getPosition();
    return {math::dot(relative, camera_right), math::dot(relative, camera_up),
            math::dot(relative, camera_forward)};
  }

  void emitProjectedTriangle(const ViewVertex& a, const ViewVertex& b,
                             const ViewVertex& c) {
    if (packet.vertices.size() + 3 >
        static_cast<std::size_t>(std::numeric_limits<uint16_t>::max())) {
      return;
    }

    const uint16_t base = static_cast<uint16_t>(packet.vertices.size());
    appendProjectedVertex(a);
    appendProjectedVertex(b);
    appendProjectedVertex(c);
    packet.indices.push_back(base);
    packet.indices.push_back(static_cast<uint16_t>(base + 1));
    packet.indices.push_back(static_cast<uint16_t>(base + 2));
  }

  void appendProjectedVertex(const ViewVertex& vertex) {
    const float near_plane = camera.getNearPlane();
    const float far_plane = camera.getFarPlane();
    const float x = (vertex.view.x * scale_x) / vertex.view.z;
    const float y = (vertex.view.y * scale_y) / vertex.view.z;
    const float z =
        math::clamp((vertex.view.z - near_plane) / (far_plane - near_plane),
                    0.0f, 1.0f);
    packet.vertices.push_back(makePositionColorVertex(x, y, z, vertex.color));
  }

  void addViewTriangle(ViewVertex a, ViewVertex b, ViewVertex c) {
    std::vector<ViewVertex> polygon{a, b, c};
    std::vector<ViewVertex> clipped;
    const float near_plane = camera.getNearPlane();

    for (std::size_t i = 0; i < polygon.size(); ++i) {
      const ViewVertex& current = polygon[i];
      const ViewVertex& previous =
          polygon[(i + polygon.size() - 1) % polygon.size()];
      const bool current_inside = current.view.z >= near_plane;
      const bool previous_inside = previous.view.z >= near_plane;

      if (current_inside != previous_inside) {
        const float t = (near_plane - previous.view.z) /
                        (current.view.z - previous.view.z);
        clipped.push_back(interpolate(previous, current, t));
      }
      if (current_inside) {
        clipped.push_back(current);
      }
    }

    if (clipped.size() < 3) {
      return;
    }
    for (std::size_t i = 1; i + 1 < clipped.size(); ++i) {
      emitProjectedTriangle(clipped[0], clipped[i], clipped[i + 1]);
    }
  }

 public:
  FrameBuilder(const Camera& camera, float aspect_ratio, RenderPacket& packet)
      : camera(camera),
        aspect_ratio(std::max(0.1f, aspect_ratio)),
        packet(packet),
        camera_right(camera.right()),
        camera_up(camera.up()),
        camera_forward(camera.forward()) {
    scale_y = 1.0f / std::tan(camera.getFovY() * 0.5f);
    scale_x = scale_y / this->aspect_ratio;
  }

  void addBox(const math::AABB& box, Color color) {
    const std::array<math::Vec3, 8> corners = {
        {{box.min.x, box.min.y, box.min.z},
         {box.max.x, box.min.y, box.min.z},
         {box.max.x, box.max.y, box.min.z},
         {box.min.x, box.max.y, box.min.z},
         {box.min.x, box.min.y, box.max.z},
         {box.max.x, box.min.y, box.max.z},
         {box.max.x, box.max.y, box.max.z},
         {box.min.x, box.max.y, box.max.z}}};

    struct Face {
      std::array<int, 4> indices;
      math::Vec3 normal;
    };
    const std::array<Face, 6> faces = {
        Face{{0, 1, 2, 3}, {0.0f, 0.0f, -1.0f}},
        Face{{5, 4, 7, 6}, {0.0f, 0.0f, 1.0f}},
        Face{{4, 0, 3, 7}, {-1.0f, 0.0f, 0.0f}},
        Face{{1, 5, 6, 2}, {1.0f, 0.0f, 0.0f}},
        Face{{3, 2, 6, 7}, {0.0f, 1.0f, 0.0f}},
        Face{{4, 5, 1, 0}, {0.0f, -1.0f, 0.0f}}};

    const math::Vec3 light_direction =
        math::normalize({-0.35f, 0.85f, -0.45f});
    for (const Face& face : faces) {
      const float light =
          0.55f + 0.35f *
                      std::max(0.0f, math::dot(face.normal, light_direction));
      const Color face_color = shade(color, light);
      std::array<ViewVertex, 4> vertices = {
          ViewVertex{toViewSpace(corners[face.indices[0]]), face_color},
          ViewVertex{toViewSpace(corners[face.indices[1]]), face_color},
          ViewVertex{toViewSpace(corners[face.indices[2]]), face_color},
          ViewVertex{toViewSpace(corners[face.indices[3]]), face_color}};

      addViewTriangle(vertices[0], vertices[1], vertices[2]);
      addViewTriangle(vertices[0], vertices[2], vertices[3]);
    }
  }

  void addScreenQuad(float min_x, float min_y, float max_x, float max_y,
                     Color color) {
    if (packet.vertices.size() + 4 >
        static_cast<std::size_t>(std::numeric_limits<uint16_t>::max())) {
      return;
    }

    const uint16_t base = static_cast<uint16_t>(packet.vertices.size());
    packet.vertices.push_back(
        makePositionColorVertex(min_x, min_y, 0.0f, color));
    packet.vertices.push_back(
        makePositionColorVertex(max_x, min_y, 0.0f, color));
    packet.vertices.push_back(
        makePositionColorVertex(max_x, max_y, 0.0f, color));
    packet.vertices.push_back(
        makePositionColorVertex(min_x, max_y, 0.0f, color));
    packet.indices.push_back(base);
    packet.indices.push_back(static_cast<uint16_t>(base + 1));
    packet.indices.push_back(static_cast<uint16_t>(base + 2));
    packet.indices.push_back(base);
    packet.indices.push_back(static_cast<uint16_t>(base + 2));
    packet.indices.push_back(static_cast<uint16_t>(base + 3));
  }
};

}  // namespace

GameWorld::GameWorld() : level(Level::createPrototypeArena()) {
  targets.push_back({{{-0.4f, 0.75f, -6.9f}, {0.4f, 1.55f, -6.1f}}});
  targets.push_back({{{4.6f, 0.7f, -4.2f}, {5.3f, 1.45f, -3.5f}}});
  targets.push_back({{{-5.4f, 0.8f, 2.7f}, {-4.6f, 1.6f, 3.5f}}});
}

void GameWorld::update(float dt, const InputState& input) {
  player.update(dt, input, level.getPlayerColliders());

  crosshair_flash = std::max(0.0f, crosshair_flash - dt);
  for (Target& target : targets) {
    target.hit_flash = std::max(0.0f, target.hit_flash - dt);
  }

  if (input.wasMouseButtonPressed(SDL_BUTTON_LEFT)) {
    fireHitscan();
  }
}

void GameWorld::fireHitscan() {
  const Camera& camera = player.getCamera();
  const math::Ray ray{camera.getPosition(), camera.forward()};

  float blocker_distance = std::numeric_limits<float>::infinity();
  for (const math::AABB& blocker : level.getShotBlockers()) {
    const math::RayHit hit = math::intersectRay(ray, blocker);
    if (hit.hit && hit.distance > 0.01f) {
      blocker_distance = std::min(blocker_distance, hit.distance);
    }
  }

  Target* best_target = nullptr;
  float best_distance = std::numeric_limits<float>::infinity();
  for (Target& target : targets) {
    if (!target.alive()) {
      continue;
    }

    const math::RayHit hit = math::intersectRay(ray, target.bounds);
    if (hit.hit && hit.distance < best_distance) {
      best_distance = hit.distance;
      best_target = &target;
    }
  }

  if (best_target != nullptr && best_distance <= blocker_distance + 0.001f) {
    best_target->health -= 1;
    best_target->hit_flash = 0.2f;
    crosshair_flash = 0.14f;
  }
}

RenderPacket GameWorld::render(float aspect_ratio) const {
  RenderPacket packet;
  FrameBuilder builder(player.getCamera(), aspect_ratio, packet);

  for (const StaticBox& box : level.getBoxes()) {
    builder.addBox(box.bounds, box.color);
  }

  for (const Target& target : targets) {
    Color color = {198, 68, 65, 255};
    if (!target.alive()) {
      color = {50, 54, 58, 255};
    } else if (target.hit_flash > 0.0f) {
      color = {255, 211, 96, 255};
    }
    builder.addBox(target.bounds, color);
  }

  const Color crosshair_color =
      crosshair_flash > 0.0f ? Color{255, 64, 64, 255}
                             : Color{230, 235, 238, 255};
  builder.addScreenQuad(-0.035f, -0.0045f, 0.035f, 0.0045f, crosshair_color);
  builder.addScreenQuad(-0.0045f, -0.035f, 0.0045f, 0.035f, crosshair_color);

  return packet;
}

std::size_t GameWorld::aliveTargetCount() const {
  std::size_t count = 0;
  for (const Target& target : targets) {
    if (target.alive()) {
      ++count;
    }
  }
  return count;
}
