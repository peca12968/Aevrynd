#include "GameplayObjectPaletteUi3D.h"
#include <algorithm>
#include <cmath>

namespace GameplayLab {

bool PaletteUiRect3D::Contains(PaletteUiPoint3D point) const {
  return std::isfinite(point.x) && std::isfinite(point.y) && width > 0.f &&
         height > 0.f && point.x >= x && point.y >= y && point.x < x + width &&
         point.y < y + height;
}

void GameplayObjectPaletteUi3D::SetLayout(PaletteUiRect3D bounds,
                                          float rowGap) {
  if (!std::isfinite(bounds.x) || !std::isfinite(bounds.y) ||
      !std::isfinite(bounds.width) || !std::isfinite(bounds.height) ||
      !std::isfinite(rowGap) || bounds.width <= 0.f || bounds.height <= 0.f ||
      rowGap < 0.f)
    return;
  bounds_ = bounds;
  rowGap_ = rowGap;
}

PaletteUiRect3D
GameplayObjectPaletteUi3D::ItemBounds(std::size_t index) const {
  if (index >= Categories.size())
    return {};
  const float totalGap = rowGap_ * float(Categories.size() - 1);
  const float rowHeight =
      (std::max)(0.f, bounds_.height - totalGap) / float(Categories.size());
  return {bounds_.x, bounds_.y + float(index) * (rowHeight + rowGap_),
          bounds_.width, rowHeight};
}

std::optional<PaletteCategory>
GameplayObjectPaletteUi3D::HitTest(PaletteUiPoint3D point) const {
  for (std::size_t index = 0; index < Categories.size(); ++index)
    if (ItemBounds(index).Contains(point))
      return Categories[index];
  return std::nullopt;
}

bool GameplayObjectPaletteUi3D::PointerDown(PaletteUiPoint3D point,
                                            Aevrynd::Ray3D worldRay) {
  const auto category = HitTest(point);
  if (!category)
    return false;
  ++receipt_.pointerDowns;
  return Begin(*category, worldRay, Event::PointerDown);
}

bool GameplayObjectPaletteUi3D::PointerDrag(Aevrynd::Ray3D worldRay,
                                            float rotationDelta) {
  if (!active_)
    return false;
  ++receipt_.drags;
  return Preview(worldRay, rotationDelta);
}

bool GameplayObjectPaletteUi3D::PointerUp(bool commit) {
  return Finish(commit);
}

bool GameplayObjectPaletteUi3D::KeyboardBegin(PaletteCategory category,
                                              Aevrynd::Ray3D worldRay) {
  return Begin(category, worldRay, Event::KeyboardBegin);
}

bool GameplayObjectPaletteUi3D::KeyboardPreview(Aevrynd::Ray3D worldRay,
                                                float rotationDelta) {
  return Preview(worldRay, rotationDelta);
}

bool GameplayObjectPaletteUi3D::KeyboardCommit() { return Finish(true); }

void GameplayObjectPaletteUi3D::Cancel() { (void)Finish(false); }

bool GameplayObjectPaletteUi3D::Begin(PaletteCategory category,
                                      Aevrynd::Ray3D worldRay, Event source) {
  if (active_)
    return false;
  // UI只持有transaction狀態，物件與preview仍由既有adapter唯一擁有。
  selectedCategory_ = category;
  active_ = adapter_.Begin(worldRay, category);
  Record(source, category, active_);
  if (!active_)
    ResetInteraction();
  return active_;
}

bool GameplayObjectPaletteUi3D::Preview(Aevrynd::Ray3D worldRay,
                                        float rotationDelta) {
  if (!active_ || !selectedCategory_ || !std::isfinite(rotationDelta))
    return false;
  ++receipt_.previews;
  const bool accepted = adapter_.Update(worldRay, rotationDelta);
  Record(Event::Preview, *selectedCategory_, accepted);
  return accepted;
}

bool GameplayObjectPaletteUi3D::Finish(bool commit) {
  if (!active_ || !selectedCategory_)
    return false;
  const PaletteCategory category = *selectedCategory_;
  if (!commit) {
    adapter_.Cancel();
    ++receipt_.cancels;
    Record(Event::Cancel, category, true);
    ResetInteraction();
    return true;
  }
  const bool accepted = adapter_.Preview().valid && adapter_.Commit();
  if (accepted) {
    ++receipt_.commits;
    Record(Event::Commit, category, true);
  } else {
    // 無效放置必須回滾同一個adapter transaction，不殘留preview。
    adapter_.Cancel();
    ++receipt_.invalidPlacements;
    Record(Event::Invalid, category, false);
  }
  ResetInteraction();
  return accepted;
}

void GameplayObjectPaletteUi3D::Record(Event event, PaletteCategory category,
                                       bool accepted) {
  constexpr std::uint64_t prime = 1099511628211ull;
  const std::array<std::uint8_t, 3> bytes{
      static_cast<std::uint8_t>(event), static_cast<std::uint8_t>(category),
      static_cast<std::uint8_t>(accepted ? 1 : 0)};
  for (const std::uint8_t value : bytes) {
    receipt_.deterministicHash ^= value;
    receipt_.deterministicHash *= prime;
  }
}

void GameplayObjectPaletteUi3D::ResetInteraction() {
  active_ = false;
  selectedCategory_.reset();
}

} // namespace GameplayLab
