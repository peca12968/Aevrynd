#pragma once
#include <Aevrynd/GeometryNormals3D.h>
#include <Aevrynd/Mesh3D.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace Aevrynd {
namespace BeveledBoxDetail3D {
inline constexpr std::size_t VertexCount=96,IndexCount=132;
inline constexpr double BasisTolerance=8.0*double(std::numeric_limits<float>::epsilon());
inline bool Finite(Vec3 p) noexcept {
    return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);
}
inline double Scalar(Vec3 a,Vec3 b) noexcept {
    return double(a.x)*double(b.x)+double(a.y)*double(b.y)+double(a.z)*double(b.z);
}
// 共用純計數守門，測試可覆蓋uint32邊界而不配置數十GB假網格。
inline constexpr bool CanAppendCounts(std::size_t vertices,std::size_t indices) noexcept {
    constexpr auto limit=std::size_t(std::numeric_limits<std::uint32_t>::max());
    return vertices<=limit-VertexCount&&indices<=limit-IndexCount&&indices%3==0;
}
inline bool RightOrthonormal(Vec3 a,Vec3 b,Vec3 c) noexcept {
    if(!Finite(a)||!Finite(b)||!Finite(c))return false;
    if(std::abs(Scalar(a,a)-1.0)>BasisTolerance||std::abs(Scalar(b,b)-1.0)>BasisTolerance||
       std::abs(Scalar(c,c)-1.0)>BasisTolerance||std::abs(Scalar(a,b))>BasisTolerance||
       std::abs(Scalar(a,c))>BasisTolerance||std::abs(Scalar(b,c))>BasisTolerance)return false;
    const double determinant=double(a.x)*(double(b.y)*c.z-double(b.z)*c.y)
        -double(a.y)*(double(b.x)*c.z-double(b.z)*c.x)
        +double(a.z)*(double(b.x)*c.y-double(b.y)*c.x);
    return std::abs(determinant-1.0)<=2.0*BasisTolerance;
}
}

// 右手正交basis只驗證、不正規化；尺寸為全長，bevel為沿各local軸退縮的距離。
// 原箱板運算順序／面順序／包法線／零UV不改。數值不可表示或配置失敗時回false。
// 失敗保留destination內容與cache；reserve可能改capacity，但不提交任何幾何。
[[nodiscard]] inline bool AppendBeveledBox3D(Mesh3D& destination,Vec3 center,Vec3 size,float bevel,
    Color tint,Vec3 axis0={1,0,0},Vec3 axis1={0,1,0},Vec3 axis2={0,0,1}) noexcept {
    using namespace BeveledBoxDetail3D;
    static_assert(std::is_trivially_copyable_v<Vertex3D> && std::is_nothrow_copy_constructible_v<Vertex3D>);
    if(!Finite(center)||!Finite(size)||size.x<=0.f||size.y<=0.f||size.z<=0.f||
       !std::isfinite(bevel)||bevel<=0.f||!std::isfinite(tint.r)||!std::isfinite(tint.g)||
       !std::isfinite(tint.b)||!std::isfinite(tint.a)||!RightOrthonormal(axis0,axis1,axis2)||
       !destination.raycastCache||!CanAppendCounts(destination.vertices.size(),destination.indices.size()))return false;
    const std::array<float,3> h{size.x*.5f,size.y*.5f,size.z*.5f};
    if(!(bevel<(std::min)({h[0],h[1],h[2]})))return false;
    if(destination.vertices.max_size()<VertexCount||destination.indices.max_size()<IndexCount||
       destination.vertices.size()>destination.vertices.max_size()-VertexCount||
       destination.indices.size()>destination.indices.max_size()-IndexCount)return false;
    for(const auto index:destination.indices)if(std::size_t(index)>=destination.vertices.size())return false;
    const std::array<Vec3,3> axes{axis0,axis1,axis2};
    const auto world=[&](const std::array<float,3>& p){return center+axes[0]*p[0]+axes[1]*p[1]+axes[2]*p[2];};
    try {
        Mesh3D mesh;
        mesh.vertices.reserve(VertexCount);mesh.indices.reserve(IndexCount);
        const auto face=[&](std::vector<Vec3> points) {
            Vec3 sum{};
            for(const auto p:points){if(!Finite(p))return false;sum=sum+(p-center);}
            // 保留原float定向運算；其溢位／下溢不猜方向、不產替代法線。
            const float orientation=Dot(Cross(points[1]-points[0],points[2]-points[0]),sum);
            if(!Finite(sum)||!std::isfinite(orientation)||orientation==0.f)return false;
            if(orientation<0)std::reverse(points.begin()+1,points.end());
            Vec3 faceNormal{};
            if(!TryComputeTriangleNormal(points[0],points[1],points[2],faceNormal))return false;
            const auto normal=PackNormalSnorm8(faceNormal);
            const auto first=static_cast<std::uint32_t>(mesh.vertices.size());
            for(const auto p:points)mesh.vertices.push_back({p.x,p.y,p.z,normal,PackColorRgba8(tint)});
            for(std::uint32_t k=1;k+1<static_cast<std::uint32_t>(points.size());++k){
                Vec3 checkNormal{};
                if(!TryComputeTriangleNormal(points[0],points[k],points[k+1],checkNormal))return false;
                mesh.indices.insert(mesh.indices.end(),{first,first+k,first+k+1});
            }
            return true;
        };
        for(unsigned a=0;a<3;++a)for(const float sign:{-1.f,1.f}) {
            const unsigned b=(a+1)%3,c=(a+2)%3;
            std::vector<Vec3> points;
            for(const auto st:std::array<std::array<float,2>,4>{{{{-1,-1}},{{1,-1}},{{1,1}},{{-1,1}}}}) {
                std::array<float,3> p{};p[a]=sign*h[a];p[b]=st[0]*(h[b]-bevel);p[c]=st[1]*(h[c]-bevel);
                points.push_back(world(p));
            }
            if(!face(points))return false;
        }
        for(unsigned a=0;a<3;++a)for(unsigned b=a+1;b<3;++b) {
            const unsigned c=3-a-b;
            for(const float sa:{-1.f,1.f})for(const float sb:{-1.f,1.f}) {
                std::vector<Vec3> points;
                for(const auto st:std::array<std::array<unsigned,2>,4>{{{{0,0}},{{1,0}},{{1,1}},{{0,1}}}}) {
                    std::array<float,3> p{};
                    p[a]=sa*(h[a]-(st[0]?bevel:0.f));p[b]=sb*(h[b]-(st[0]?0.f:bevel));
                    p[c]=(st[1]?1.f:-1.f)*(h[c]-bevel);points.push_back(world(p));
                }
                if(!face(points))return false;
            }
        }
        for(const float sx:{-1.f,1.f})for(const float sy:{-1.f,1.f})for(const float sz:{-1.f,1.f}) {
            const std::array<float,3> signs{sx,sy,sz};std::vector<Vec3> points;
            for(unsigned a=0;a<3;++a) {
                std::array<float,3> p{};
                for(unsigned b=0;b<3;++b)p[b]=signs[b]*(h[b]-(a==b?0.f:bevel));
                points.push_back(world(p));
            }
            if(!face(points))return false;
        }
        if(mesh.vertices.size()!=VertexCount||mesh.indices.size()!=IndexCount)return false;
        const auto base=static_cast<std::uint32_t>(destination.vertices.size());
        for(auto& index:mesh.indices)index+=base;
        // 兩次reserve先完成，之後trivial元素插入不配置；避免一半commit。
        destination.vertices.reserve(destination.vertices.size()+VertexCount);
        destination.indices.reserve(destination.indices.size()+IndexCount);
        destination.vertices.insert(destination.vertices.end(),mesh.vertices.begin(),mesh.vertices.end());
        destination.indices.insert(destination.indices.end(),mesh.indices.begin(),mesh.indices.end());
        destination.InvalidateRaycastIndex();
        return true;
    } catch(const std::bad_alloc&) {return false;}
      catch(const std::length_error&) {return false;}
}
}
