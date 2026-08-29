#include <Aevrynd/SkeletalSocket3D.h>
#include <cmath>

namespace Aevrynd { namespace {
bool Finite(const Matrix4& value)noexcept{for(std::size_t row=0;row<4;++row)for(std::size_t column=0;column<4;++column)if(!std::isfinite(value[row][column]))return false;return true;}
}
bool EvaluateSkeletalSocket3D(const SkeletalSocket3D& socket,SkeletonCompatibilityId3D compatibility,
    const AnimationPose3D& pose,const Matrix4& modelToWorld,Matrix4& output)noexcept{
    if(!socket.compatibility||socket.compatibility!=compatibility||socket.joint>=pose.globalMatrices.size()||
        !Finite(socket.localOffset)||!Finite(modelToWorld)||!Finite(pose.globalMatrices[socket.joint]))return false;
    const Matrix4 candidate=modelToWorld*pose.globalMatrices[socket.joint]*socket.localOffset;
    if(!Finite(candidate))return false;output=candidate;return true;
}
}
