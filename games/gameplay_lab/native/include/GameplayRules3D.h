#pragma once
#include <Aevrynd/Camera.h>
#include <Aevrynd/Math.h>
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace GameplayLab {
enum class CameraMode : std::uint8_t {FirstPerson,ThirdPerson,GodView};
enum class LightingPhase : std::uint8_t {Day,Dusk,Night};
enum class PaletteCategory : std::uint8_t {Primitives,Physics,Props,Building,Characters,Lights};
enum class LocomotionState : std::uint8_t {Idle,Walk,Run,Crouch,Jump,Fall,Hit,Dead};
struct TickInput {Aevrynd::Vec3 movement{};bool run{},jump{},crouch{},attack{},togglePalette{},place{},cancel{},erase{},undo{},redo{},grab{};float rotate{};};
struct Entity {std::uint64_t id{};PaletteCategory category{};Aevrynd::Vec3 position{};float rotation{};bool dynamic{},alive{true};};
struct Stats {std::uint64_t ticks{},cameraSwitches{},jumps{},crouches{},headroomRejected{},attacks{},hits{},deduplicatedHits{},kills{},placements{},placementRejected{},deletes{},undo{},redo{},grabs{},drops{};std::size_t entities{},undoDepth{},redoDepth{};};
struct LightingState {Aevrynd::Vec3 sunDirection{};Aevrynd::Color sunColor{},fogColor{},clearColor{};float intensity{},exposure{},fogDensity{};};

class Rules3D {
public:
    static constexpr std::size_t MaximumEntities=256,HistoryCapacity=32;
    static constexpr float MaximumGrabDistance=8.f,MaximumGrabForce=120.f;
    void Reset();
    void SetCamera(CameraMode mode);void CycleCamera();
    void SetLighting(LightingPhase phase);void CycleLighting();
    void SetCharacterFemale(bool female){female_=female;} void SetHeadroom(bool clear){headroomClear_=clear;}
    void SetPaletteCategory(PaletteCategory category){category_=category;}
    bool ApplyPalettePresentationInput(TickInput input)noexcept;
    void Tick(TickInput input,float dt=1.f/60.f);
    bool Place(Aevrynd::Vec3 position,bool dynamic=false);bool RollbackLastPlacement(std::uint64_t id);bool Delete(std::uint64_t id);bool Undo();bool Redo();
    bool BeginGrab(std::uint64_t id,float distance);void EndGrab();
    CameraMode Camera()const{return camera_;} LightingPhase Lighting()const{return lighting_;} PaletteCategory Category()const{return category_;}
    LocomotionState Locomotion()const{return locomotion_;} Aevrynd::Vec3 CharacterPosition()const{return characterPosition_;}
    float CharacterHeight()const{return crouched_?1.15f:1.8f;} bool Crouched()const{return crouched_;} bool PaletteOpen()const{return paletteOpen_;}
    bool Female()const{return female_;}
    std::optional<std::uint64_t> Grabbed()const{return grabbed_;} std::span<const Entity> Entities()const{return entities_;}
    LightingState EvaluateLighting(float blend=1.f)const; Stats GetStats()const;
private:
    struct Edit {bool placement{};Entity entity{};};
    void PushEdit(Edit edit);
    CameraMode camera_{CameraMode::ThirdPerson};LightingPhase lighting_{LightingPhase::Day};PaletteCategory category_{PaletteCategory::Primitives};LocomotionState locomotion_{LocomotionState::Idle};
    Aevrynd::Vec3 characterPosition_{};float verticalVelocity_{};std::uint64_t tick_{},nextEntityId_{1};bool grounded_{true},crouched_{},headroomClear_{true},paletteOpen_{},female_{true};
    std::optional<std::uint64_t> grabbed_{};std::vector<Entity> entities_{};std::vector<Edit> undo_{},redo_{};Stats stats_{};
};
}
