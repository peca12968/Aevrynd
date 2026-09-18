#include "ObjectPaletteAdapter3D.h"
#include <cmath>
using namespace Aevrynd;
namespace GameplayLab {
namespace {
bool Finite(Vec3 value) {
  return std::isfinite(value.x) && std::isfinite(value.y) &&
         std::isfinite(value.z);
}
}
bool ObjectPaletteAdapter3D::Project(Ray3D ray) {
  if (std::abs(ray.direction.y) < .0001f)
    return false;
  const float t = -ray.origin.y / ray.direction.y;
  if (t < 0 || t > 200)
    return false;
  Vec3 p = ray.origin + ray.direction * t;
  p.x = std::round(p.x * .5f) * 2;
  p.z = std::round(p.z * .5f) * 2;
  p.y = .5f;
  preview_.position = p;
  preview_.valid = !collision_.OverlapsBox(p, {.48f, .48f, .48f});
  if (preview_.valid) {
    ++stats_.validPreviews;
    lastValid_ = preview_.position;
  } else {
    ++stats_.collisionRejected;
    lastInvalid_ = preview_.position;
  }
  return true;
}
bool ObjectPaletteAdapter3D::Begin(Ray3D ray, PaletteCategory category) {
  preview_ = {true, false, {}, 0, category};
  rules_.SetPaletteCategory(category);
  ++stats_.previews;
  return Project(ray);
}
bool ObjectPaletteAdapter3D::PreviewAt(Vec3 position,
                                       PaletteCategory category) {
  preview_ = {true, !collision_.OverlapsBox(position, {.48f, .48f, .48f}),
              position, 0, category};
  rules_.SetPaletteCategory(category);
  ++stats_.previews;
  if (preview_.valid) {
    ++stats_.validPreviews;
    lastValid_ = position;
  } else {
    ++stats_.collisionRejected;
    lastInvalid_ = position;
  }
  return preview_.valid;
}
bool ObjectPaletteAdapter3D::PlaceAt(Vec3 position, PaletteCategory category) {
  preview_ = {true, !collision_.OverlapsBox(position, {.48f, .48f, .48f}),
              position, 0, category};
  rules_.SetPaletteCategory(category);
  ++stats_.previews;
  if (preview_.valid) {
    ++stats_.validPreviews;
    lastValid_ = preview_.position;
  } else {
    ++stats_.collisionRejected;
    lastInvalid_ = preview_.position;
  }
  return Commit();
}
bool ObjectPaletteAdapter3D::Update(Ray3D ray, float delta) {
  if (!preview_.active || !std::isfinite(delta))
    return false;
  const float rotation =
      std::round((preview_.rotation + delta * .2617994f) / .2617994f) *
      .2617994f;
  if (!std::isfinite(rotation) || !Project(ray))
    return false;
  preview_.rotation = rotation;
  return true;
}
bool ObjectPaletteAdapter3D::Commit() {
  if (!preview_.active || !preview_.valid)
    return false;
  const bool dynamic = preview_.category == PaletteCategory::Physics;
  // Direct production helpers (PlaceAt) and pointer UI both commit through the
  // adapter.  The authoritative entity category must therefore come from this
  // transaction, not from whichever palette category happened to be selected
  // previously in Rules3D.
  rules_.SetPaletteCategory(preview_.category);
  if (!rules_.Place(preview_.position, dynamic, preview_.rotation))
    return false;
  const auto &placed = rules_.Entities().back();
  const auto id = placed.id;
  RigidBodySettings3D body{};
  body.type = dynamic ? RigidBodyType3D::Dynamic : RigidBodyType3D::Static;
  body.transform.position = placed.position;
  body.transform.rotation = {0, placed.rotation, 0};
  std::vector<SceneEntity3D> created;
  SceneEntityDefinition3D definition{};
  definition.transform = SceneTransformComponent3D{body.transform};
  const auto handle = physics_.CreateBody(body);
  const auto collider = collision_.CreateBox(
      {{placed.position, {.5f, .5f, .5f}}, 4, 0xffffffffu, id, false});
  const bool sceneCreated = scene_.CreateBatchAtomic(
      std::span<const SceneEntityDefinition3D>{&definition, 1}, created);
  if (!handle || !collider || !sceneCreated || created.size() != 1) {
    if (handle)
      physics_.DestroyBody(handle);
    if (collider)
      collision_.Destroy(collider);
    if (!created.empty())
      scene_.DestroyBatchAtomic(created);
    rules_.RollbackLastPlacement(id);
    return false;
  }
  bodies_.emplace(id, handle);
  colliders_.emplace(id, collider);
  sceneEntities_.emplace(id, created.front());
  ++stats_.commits;
  preview_ = {};
  return true;
}
void ObjectPaletteAdapter3D::Cancel() {
  if (preview_.active)
    ++stats_.cancels;
  preview_ = {};
}
bool ObjectPaletteAdapter3D::Delete(std::uint64_t id) {
  auto f = bodies_.find(id);
  auto s = sceneEntities_.find(id);
  if (f == bodies_.end() || s == sceneEntities_.end() ||
      !scene_.Alive(s->second))
    return false;
  EndGrab();
  if (!rules_.Delete(id))
    return false;
  physics_.DestroyBody(f->second);
  bodies_.erase(f);
  if (const auto c = colliders_.find(id); c != colliders_.end()) {
    collision_.Destroy(c->second);
    colliders_.erase(c);
  }
  scene_.Destroy(s->second);
  sceneEntities_.erase(s);
  ++stats_.deletes;
  return true;
}
bool ObjectPaletteAdapter3D::Reconcile() {
  EndGrab();
  std::unordered_map<std::uint64_t, const Entity *> desired;
  for (const auto &entity : rules_.Entities())
    desired.emplace(entity.id, &entity);
  struct Staged {
    std::uint64_t id{};
    RigidBodyHandle3D body{};
    ColliderHandle3D collider{};
    SceneEntity3D scene{};
  };
  std::vector<Staged> staged;
  for (const auto &[id, entity] : desired) {
    if (bodies_.contains(id))
      continue;
    RigidBodySettings3D body{};
    body.type =
        entity->dynamic ? RigidBodyType3D::Dynamic : RigidBodyType3D::Static;
    body.transform.position = entity->position;
    body.transform.rotation = {0, entity->rotation, 0};
    Staged item{
        id,
        physics_.CreateBody(body),
        collision_.CreateBox(
            {{entity->position, {.5f, .5f, .5f}}, 4, 0xffffffffu, id, false}),
        {}};
    SceneEntityDefinition3D definition{};
    definition.transform = SceneTransformComponent3D{body.transform};
    std::vector<SceneEntity3D> created;
    const bool sceneCreated = scene_.CreateBatchAtomic(
        std::span<const SceneEntityDefinition3D>{&definition, 1}, created);
    if (sceneCreated && created.size() == 1)
      item.scene = created.front();
    if (!item.body || !item.collider || !item.scene) {
      if (item.body)
        physics_.DestroyBody(item.body);
      if (item.collider)
        collision_.Destroy(item.collider);
      if (item.scene)
        scene_.Destroy(item.scene);
      for (const auto &value : staged) {
        physics_.DestroyBody(value.body);
        collision_.Destroy(value.collider);
        scene_.Destroy(value.scene);
      }
      return false;
    }
    staged.push_back(item);
  }
  for (auto it = bodies_.begin(); it != bodies_.end();) {
    if (desired.contains(it->first)) {
      ++it;
      continue;
    }
    const auto id = it->first;
    physics_.DestroyBody(it->second);
    if (const auto c = colliders_.find(id); c != colliders_.end()) {
      collision_.Destroy(c->second);
      colliders_.erase(c);
    }
    if (const auto s = sceneEntities_.find(id); s != sceneEntities_.end()) {
      scene_.Destroy(s->second);
      sceneEntities_.erase(s);
    }
    it = bodies_.erase(it);
  }
  for (const auto &value : staged) {
    bodies_.emplace(value.id, value.body);
    colliders_.emplace(value.id, value.collider);
    sceneEntities_.emplace(value.id, value.scene);
  }
  return true;
}
bool ObjectPaletteAdapter3D::Undo() {
  EndGrab();
  if (!rules_.Undo())
    return false;
  if (!Reconcile()) {
    rules_.Redo();
    Reconcile();
    return false;
  }
  ++stats_.undo;
  return true;
}
bool ObjectPaletteAdapter3D::Redo() {
  EndGrab();
  if (!rules_.Redo())
    return false;
  if (!Reconcile()) {
    rules_.Undo();
    Reconcile();
    return false;
  }
  ++stats_.redo;
  return true;
}
bool ObjectPaletteAdapter3D::BeginGrab(Ray3D ray, float maximumDistance) {
  ++stats_.pickAttempts;
  const float directionLength = Length(ray.direction);
  if (grabbed_ || !Finite(ray.origin) || !Finite(ray.direction) ||
      !std::isfinite(directionLength) || directionLength < .0001f ||
      !std::isfinite(maximumDistance) || maximumDistance <= 0.f ||
      maximumDistance > Rules3D::MaximumGrabDistance) {
    ++stats_.pickRejected;
    return false;
  }
  ray.direction = ray.direction * (1.f / directionLength);
  const CollisionRayHit3D hit =
      collision_.Raycast(ray, maximumDistance, {4, 1, false});
  if (!hit || !std::isfinite(hit.distance) || hit.distance < 0.f ||
      hit.distance > maximumDistance || !Finite(hit.position)) {
    ++stats_.pickRejected;
    return false;
  }
  const std::uint64_t id = hit.userData;
  const auto body = Body(id);
  const auto &readPhysics = static_cast<const PhysicsWorld3D &>(physics_);
  const RigidBodyState3D *state = readPhysics.GetBody(body);
  if (!body || !state || state->settings.type != RigidBodyType3D::Dynamic ||
      !rules_.BeginGrab(id, hit.distance)) {
    ++stats_.pickRejected;
    return false;
  }
  grabbed_ = id;
  grabDistance_ = hit.distance;
  grabTarget_ = hit.position;
  ++stats_.grabs;
  return true;
}
bool ObjectPaletteAdapter3D::UpdateGrab(Ray3D ray, float deltaTime) {
  if (!grabbed_)
    return false;
  const auto body = Body(*grabbed_);
  const auto &readPhysics = static_cast<const PhysicsWorld3D &>(physics_);
  const RigidBodyState3D *state = readPhysics.GetBody(body);
  const float directionLength = Length(ray.direction);
  if (!state || !Finite(ray.origin) || !Finite(ray.direction) ||
      !std::isfinite(directionLength) || directionLength < .0001f ||
      !std::isfinite(deltaTime) || deltaTime <= 0.f || deltaTime > .1f ||
      !Finite(state->settings.transform.position) || !Finite(state->velocity) ||
      !std::isfinite(state->settings.mass) || state->settings.mass <= 0.f ||
      !std::isfinite(state->settings.gravityScale) ||
      state->settings.gravityScale < 0.f || !Finite(physics_.Gravity())) {
    return false;
  }
  ray.direction = ray.direction * (1.f / directionLength);
  const Vec3 target = ray.origin + ray.direction * grabDistance_;
  if (!Finite(target))
    return false;
  // 以質量縮放加速度與重力補償，使極輕到重型物件都維持相同可控手感。
  const Vec3 requestedAcceleration =
      (target - state->settings.transform.position) * 72.f -
      state->velocity * 14.f - physics_.Gravity() * state->settings.gravityScale;
  const Vec3 requested = requestedAcceleration * state->settings.mass;
  const float magnitude = Length(requested);
  const float maximumForce =
      Rules3D::MaximumGrabForce * state->settings.mass;
  if (!Finite(requested) || !std::isfinite(magnitude) ||
      !std::isfinite(maximumForce) || maximumForce <= 0.f)
    return false;
  const Vec3 force = magnitude > maximumForce
                         ? requested * (maximumForce / magnitude)
                         : requested;
  if (!Finite(force) || !physics_.AddForce(body, force)) {
    return false;
  }
  grabTarget_ = target;
  ++stats_.dragSteps;
  return true;
}
void ObjectPaletteAdapter3D::EndGrab() {
  if (!grabbed_)
    return;
  rules_.EndGrab();
  grabbed_.reset();
  grabDistance_ = 0;
  grabTarget_ = {};
  ++stats_.drops;
}
std::optional<PaletteCategory> ObjectPaletteAdapter3D::GrabbedCategory() const {
  if (!grabbed_)
    return std::nullopt;
  for (const auto &entity : rules_.Entities())
    if (entity.id == *grabbed_)
      return entity.category;
  return std::nullopt;
}
float ObjectPaletteAdapter3D::GrabError() const {
  if (!grabbed_)
    return 0.f;
  const auto &readPhysics = static_cast<const PhysicsWorld3D &>(physics_);
  const auto *state = readPhysics.GetBody(Body(*grabbed_));
  return state && Finite(state->settings.transform.position)
             ? Length(grabTarget_ - state->settings.transform.position)
             : 0.f;
}
bool ObjectPaletteAdapter3D::SyncDynamicTransforms() {
  const auto &readPhysics = static_cast<const PhysicsWorld3D &>(physics_);
  for (const auto &entity : rules_.Entities()) {
    if (!entity.dynamic)
      continue;
    const auto body = Body(entity.id);
    const auto collider = Collider(entity.id);
    const auto sceneEntity = SceneEntity(entity.id);
    const auto *state = readPhysics.GetBody(body);
    if (!state || !collider || !sceneEntity)
      return false;
    const auto &transform = state->settings.transform;
    if (!scene_.SetTransform(sceneEntity,
                             SceneTransformComponent3D{transform}) ||
        !collision_.UpdateBox(collider, {{transform.position, {.5f, .5f, .5f}},
                                         4,
                                         0xffffffffu,
                                         entity.id,
                                         false}))
      return false;
  }
  return true;
}
void ObjectPaletteAdapter3D::Clear() {
  EndGrab();
  for (auto [id, b] : bodies_) {
    (void)id;
    physics_.DestroyBody(b);
  }
  for (auto [id, c] : colliders_) {
    (void)id;
    collision_.Destroy(c);
  }
  for (auto [id, e] : sceneEntities_) {
    (void)id;
    scene_.Destroy(e);
  }
  bodies_.clear();
  colliders_.clear();
  sceneEntities_.clear();
  rules_.Reset();
  preview_ = {};
}
} // namespace GameplayLab
