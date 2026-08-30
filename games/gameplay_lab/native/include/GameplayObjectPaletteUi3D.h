#pragma once

#include "ObjectPaletteAdapter3D.h"
#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

namespace GameplayLab {

struct PaletteUiPoint3D {
  float x{};
  float y{};
};

struct PaletteUiRect3D {
  float x{};
  float y{};
  float width{};
  float height{};

  bool Contains(PaletteUiPoint3D point) const;
};

struct PaletteUiReceipt3D {
  std::uint64_t pointerDowns{};
  std::uint64_t drags{};
  std::uint64_t previews{};
  std::uint64_t commits{};
  std::uint64_t invalidPlacements{};
  std::uint64_t cancels{};
  std::uint64_t deterministicHash{1469598103934665603ull};
};

class GameplayObjectPaletteUi3D {
public:
  static constexpr std::array<PaletteCategory, 6> Categories{
      PaletteCategory::Primitives, PaletteCategory::Physics,
      PaletteCategory::Props, PaletteCategory::Building,
      PaletteCategory::Characters, PaletteCategory::Lights};
  static constexpr std::array<std::string_view, 6> Labels{
      "Primitives", "Physics", "Props", "Building", "Characters",
      "Lights"};

  explicit GameplayObjectPaletteUi3D(ObjectPaletteAdapter3D &adapter)
      : adapter_(adapter) {}

  void SetLayout(PaletteUiRect3D bounds, float rowGap = 4.f);
  PaletteUiRect3D ItemBounds(std::size_t index) const;
  std::optional<PaletteCategory> HitTest(PaletteUiPoint3D point) const;

  bool PointerDown(PaletteUiPoint3D point, Aevrynd::Ray3D worldRay);
  bool PointerDrag(Aevrynd::Ray3D worldRay, float rotationDelta = 0.f);
  bool PointerUp(bool commit = true);

  bool KeyboardBegin(PaletteCategory category, Aevrynd::Ray3D worldRay);
  bool KeyboardPreview(Aevrynd::Ray3D worldRay,
                       float rotationDelta = 0.f);
  bool KeyboardCommit();
  void Cancel();

  bool Active() const { return active_; }
  std::optional<PaletteCategory> SelectedCategory() const {
    return selectedCategory_;
  }
  PaletteUiReceipt3D Receipt() const { return receipt_; }

private:
  enum class Event : std::uint8_t {
    PointerDown = 1,
    KeyboardBegin,
    Preview,
    Commit,
    Invalid,
    Cancel
  };

  bool Begin(PaletteCategory category, Aevrynd::Ray3D worldRay, Event source);
  bool Preview(Aevrynd::Ray3D worldRay, float rotationDelta);
  bool Finish(bool commit);
  void Record(Event event, PaletteCategory category, bool accepted);
  void ResetInteraction();

  ObjectPaletteAdapter3D &adapter_;
  PaletteUiRect3D bounds_{0.f, 0.f, 240.f, 288.f};
  float rowGap_{4.f};
  bool active_{};
  std::optional<PaletteCategory> selectedCategory_{};
  PaletteUiReceipt3D receipt_{};
};

} // namespace GameplayLab
