#pragma once
#include "GameplayRules3D.h"
#include <Aevrynd/Input.h>
#include <cmath>
#include <optional>
namespace GameplayLab {
struct ProductionInput3D {TickInput tick{};std::optional<CameraMode> camera{};std::optional<LightingPhase> lighting{};std::optional<PaletteCategory> category{};bool cycleCamera{},cycleLighting{},female{},male{};Aevrynd::Vec2 mouse{},mouseDelta{};float wheel{},rotate{};};
inline ProductionInput3D ReadProductionInput3D(const Aevrynd::Input& input,float yaw){ProductionInput3D r{};if(input.Pressed('1'))r.camera=CameraMode::FirstPerson;if(input.Pressed('2'))r.camera=CameraMode::ThirdPerson;if(input.Pressed('3'))r.camera=CameraMode::GodView;for(int key='4';key<='9';++key)if(input.Pressed(key))r.category=PaletteCategory(key-'4');r.cycleCamera=input.Pressed('V');if(input.Pressed(VK_F5))r.lighting=LightingPhase::Day;if(input.Pressed(VK_F6))r.lighting=LightingPhase::Dusk;if(input.Pressed(VK_F7))r.lighting=LightingPhase::Night;r.cycleLighting=input.Pressed('T');r.female=input.Pressed(VK_F2);r.male=input.Pressed(VK_F3);const Aevrynd::Vec3 f{std::sin(yaw),0,std::cos(yaw)},right{std::cos(yaw),0,-std::sin(yaw)};r.tick.movement=f*(float(input.Down('W'))-float(input.Down('S')))+right*(float(input.Down('D'))-float(input.Down('A')));r.tick.run=input.Down(VK_SHIFT);r.tick.jump=input.Pressed(VK_SPACE);r.tick.crouch=input.Pressed('C');r.tick.attack=input.MousePressed();r.tick.togglePalette=input.Pressed(VK_TAB)||input.Pressed('B');r.tick.cancel=input.RightMousePressed();r.tick.erase=input.Pressed(VK_DELETE);r.tick.undo=input.Down(VK_CONTROL)&&input.Pressed('Z');r.tick.redo=input.Down(VK_CONTROL)&&input.Pressed('Y');r.rotate=input.Pressed('R')?1.f:input.MouseWheel();r.mouse=input.Mouse();r.mouseDelta=input.MouseDelta();r.wheel=input.MouseWheel();return r;}
}
