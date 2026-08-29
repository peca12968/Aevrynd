#include "GameplayCombat3D.h"
#include "GameplayAnimation3D.h"
#include <array>
#include <cmath>
#include <iostream>

using namespace Aevrynd;using namespace GameplayLab;
static bool Different(const AnimationPose3D& left,const AnimationPose3D& right){for(std::size_t bone=0;bone<left.globalMatrices.size();++bone)for(int row=0;row<4;++row)for(int column=0;column<4;++column)if(std::abs(left.globalMatrices[bone][row][column]-right.globalMatrices[bone][row][column])>.00001f)return true;return false;}
int main(){CollisionWorld3D collision;PhysicsWorld3D physics;GameplayCombat3D combat(collision,physics);combat.SpawnMonster({0,0,1.8f},100);CombatInput3D input{{0,0,0},{0,0,1},true};for(int attack=0;attack<3;++attack){combat.Tick(input,.01f);input.attackPressed=false;for(int frame=0;frame<60;++frame)combat.Tick(input,.01f);input.attackPressed=true;}if(combat.Monster().alive||combat.Stats().hits!=3||combat.Stats().kills!=1||collision.Stats().colliders||physics.Stats().bodies)return 1;GameplayAnimation3D animation;std::array<AnimationPose3D,11> poses{};for(int value=0;value<11;++value){animation.SetState(AnimationState3D(value),true);animation.Tick(.17f);poses[std::size_t(value)]=animation.Pose();if(poses[std::size_t(value)].globalMatrices.size()!=6)return 2;}for(int left=0;left<11;++left)for(int right=left+1;right<11;++right)if(!Different(poses[std::size_t(left)],poses[std::size_t(right)]))return 3;animation.SetState(AnimationState3D::Walk,true);const auto before=animation.Pose();animation.Tick(.11f);if(!Different(before,animation.Pose()))return 4;std::cout<<"combat hits="<<combat.Stats().hits<<" kills="<<combat.Stats().kills<<" animationStates=11 bones=6\n";}
