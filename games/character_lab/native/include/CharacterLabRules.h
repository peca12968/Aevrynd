#pragma once
#include <Aevrynd/Math.h>
#include <algorithm>

namespace CharacterLabRules {
inline int AutomaticPhase(float time){return int((std::max)(0.f,time)/3.f)%4;}
inline bool FirstPerson(int phase){return phase%2==0;}
inline Aevrynd::Vec3 Movement(int phase){constexpr Aevrynd::Vec3 directions[4]={{0,0,1},{1,0,0},{0,0,-1},{-1,0,0}};return directions[unsigned(phase)%4];}
inline int JumpCycle(float time){return int((std::max)(0.f,time)/2.5f);}
}
