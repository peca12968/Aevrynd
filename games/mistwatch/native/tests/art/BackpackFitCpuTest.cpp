#include "MistwatchExplorerBackpack.h"
#include "MistwatchExplorerNapeHairBinding.h"
#include "MistwatchLoadPreparation.h"
#include <Aevrynd/ExactTriangleContact3D.h>
#include "MistwatchWorld.h"
#include <Aevrynd/SkinnedModelAsset.h>
#include <Aevrynd/WorldDeltaPersistence3D.h>
#include <Windows.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cfenv>
#include <chrono>
#include <span>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
#if defined(_M_FP_FAST) && _M_FP_FAST
#error V994_exact_expansion_requires_non_fast_math
#endif

namespace {
using namespace Aevrynd;
unsigned checks{},negatives{};
const auto Started=std::chrono::steady_clock::now();std::uint64_t narrow{},disjoint{},allowed{},forbidden{},invalid{};
void Budget(){if(std::chrono::duration<double>(std::chrono::steady_clock::now()-Started).count()>83.)throw std::runtime_error("original83s shared budget");}
void Pair(){Budget();if(++narrow>200000)throw std::runtime_error("original200000 narrowpair shared budget");}
void Need(bool ok,const char* name){Budget();++checks;if(!ok)throw std::runtime_error(name);}
template<class F> void Reject(F action,const char* name){bool failed=false;try{action();}catch(const std::runtime_error&){failed=true;}Need(failed,name);++negatives;}
bool Bits(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
bool SameVec(Vec3 a,Vec3 b){return Bits(a.x,b.x)&&Bits(a.y,b.y)&&Bits(a.z,b.z);}
bool VertexRaw(const Vertex3D& a,const Vertex3D& b){return Bits(a.x,b.x)&&Bits(a.y,b.y)&&Bits(a.z,b.z)&&a.normal==b.normal&&a.color==b.color&&Bits(a.u,b.u)&&Bits(a.v,b.v);}
Vec3 Position(const Vertex3D& a){return {a.x,a.y,a.z};}
Vec3 Normal(const Vertex3D& a){const auto n=UnpackNormalSnorm8(a.normal);return {n.x,n.y,n.z};}
void MatrixRaw(const Matrix4& a,const Matrix4& b){for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)Need(Bits(a[r][c],b[r][c]),"rig/pose matrix changed");}
void TransformRaw(const JointTransform3D& a,const JointTransform3D& b){
    Need(SameVec(a.translation,b.translation)&&SameVec(a.rotation,b.rotation)&&SameVec(a.scale,b.scale)&&
        Bits(a.quaternion.x,b.quaternion.x)&&Bits(a.quaternion.y,b.quaternion.y)&&Bits(a.quaternion.z,b.quaternion.z)&&
        Bits(a.quaternion.w,b.quaternion.w)&&a.usesQuaternion==b.usesQuaternion,"joint transform raw fields changed");}
// 正式回歸不依賴主機Evidence或來源位置；來源封印由Root建置receipt負責。
// 下方實體模型SHA、完整幾何／接觸／33負控／242姿態與存讀檔門保持原樣。
constexpr std::uint32_t First=6931,OldEnd=7093,NewEnd=7149,Delta=56;
bool OldOwned(std::uint32_t id){return id>=First&&id<OldEnd;}
bool NewOwned(std::uint32_t id){return id>=First&&id<NewEnd;}
bool CurrentOwned(std::uint32_t id){return OldOwned(id)||(id>=9225&&id<9281);}
std::uint32_t Actual(std::uint32_t logical){Need(NewOwned(logical),"logical owned range");return logical<7093?logical:9225u+logical-7093u;}
std::uint32_t Remap(std::uint32_t id){Need(!OldOwned(id),"attempted nonowned mapping of old owned ID");return id;}
// 可逆排序視圖只是檢查V994局部合同；actual model不搬非袋ID，不存此視圖。
SkinnedModel3D View(const SkinnedModel3D& actual){
    Need(actual.bindMesh.vertices.size()==9281&&actual.influences.size()==9281,"view actual population");auto out=actual;
    const auto map=[](std::uint32_t id){return id>=9225?7093u+id-9225u:id>=7093?id+56u:id;};
    for(std::uint32_t id=0;id<9281;++id){out.bindMesh.vertices[map(id)]=actual.bindMesh.vertices[id];out.influences[map(id)]=actual.influences[id];}
    for(auto& id:out.bindMesh.indices)id=map(id);out.bindMesh.InvalidateRaycastIndex();Need(out.Valid(),"bijective audit view valid");return out;
}
void NonOwned(const SkinnedModel3D& old,const SkinnedModel3D& now){
    Need(old.Valid()&&old.bindMesh.vertices.size()==9225&&old.bindMesh.indices.size()==36792,"current original9225/12264 contract");
    Need(now.Valid()&&now.bindMesh.vertices.size()==9281&&now.bindMesh.indices.size()==37032,"separate9281/12344 composition contract");
    Need(old.influences.size()==9225&&now.influences.size()==9281&&old.tangents.empty()&&now.tangents.empty(),"original influence/tangent storage contract");
    for(std::uint32_t i=0;i<9225;++i)if(!OldOwned(i)){Need(VertexRaw(old.bindMesh.vertices[i],now.bindMesh.vertices[i]),"allcurrent nonowned ID/Vertex raw changed");
        Need(old.influences[i].joints==now.influences[i].joints&&old.influences[i].weights==now.influences[i].weights,"current nonowned influences raw changed");}
    std::vector<std::uint32_t> before,after;
    for(std::size_t at=0;at<old.bindMesh.indices.size();at+=3){unsigned owned{};for(unsigned j=0;j<3;++j)owned+=OldOwned(old.bindMesh.indices[at+j]);Need(owned==0||owned==3,"source mixed bag span");
        if(!owned)before.insert(before.end(),old.bindMesh.indices.begin()+static_cast<std::ptrdiff_t>(at),old.bindMesh.indices.begin()+static_cast<std::ptrdiff_t>(at+3));}
    for(std::size_t at=0;at<now.bindMesh.indices.size();at+=3){unsigned owned{};for(unsigned j=0;j<3;++j)owned+=CurrentOwned(now.bindMesh.indices[at+j]);Need(owned==0||owned==3,"output mixed bag span");
        if(!owned)after.insert(after.end(),now.bindMesh.indices.begin()+static_cast<std::ptrdiff_t>(at),now.bindMesh.indices.begin()+static_cast<std::ptrdiff_t>(at+3));}
    Need(before==after,"complete nonowned directed indices/IDs raw");
    Need(old.skeleton.joints.size()==15&&now.skeleton.joints.size()==15&&old.inverseBindMatrices.size()==15&&now.inverseBindMatrices.size()==15&&old.skeletonCompatibilityId==now.skeletonCompatibilityId,"original rig identity changed");
    for(unsigned j=0;j<15;++j){Need(old.skeleton.joints[j].parent==now.skeleton.joints[j].parent,"rig parent changed");TransformRaw(old.skeleton.joints[j].bindTransform,now.skeleton.joints[j].bindTransform);MatrixRaw(old.inverseBindMatrices[j],now.inverseBindMatrices[j]);}
    const auto a=old.bindMesh.Bounds(),b=now.bindMesh.Bounds();Need(SameVec(a.center,b.center)&&SameVec(a.extents,b.extents),"whole character bind bounds changed");
}
struct D {double x{},y{},z{};D operator+(D b)const{return{x+b.x,y+b.y,z+b.z};}D operator-(D b)const{return{x-b.x,y-b.y,z-b.z};}D operator*(double s)const{return{x*s,y*s,z*s};}};
D Precise(const Vertex3D& v){return{v.x,v.y,v.z};}
double DotD(D a,D b){return a.x*b.x+a.y*b.y+a.z*b.z;}
D CrossD(D a,D b){return{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
using Face=std::array<std::uint32_t,3>;using Triangle=std::array<D,3>;
Triangle Points(const Mesh3D& mesh,Face f){return{Precise(mesh.vertices.at(f[0])),Precise(mesh.vertices.at(f[1])),Precise(mesh.vertices.at(f[2]))};}
double PointDistance2(D p,const Triangle& t){const D ab=t[1]-t[0],ac=t[2]-t[0],ap=p-t[0];const double d1=DotD(ab,ap),d2=DotD(ac,ap);
    if(d1<=0&&d2<=0)return DotD(ap,ap);const D bp=p-t[1];const double d3=DotD(ab,bp),d4=DotD(ac,bp);if(d3>=0&&d4<=d3)return DotD(bp,bp);
    const double vc=d1*d4-d3*d2;if(vc<=0&&d1>=0&&d3<=0){const D q=ap-ab*(d1/(d1-d3));return DotD(q,q);}
    const D cp=p-t[2];const double d5=DotD(ab,cp),d6=DotD(ac,cp);if(d6>=0&&d5<=d6)return DotD(cp,cp);
    const double vb=d5*d2-d1*d6;if(vb<=0&&d2>=0&&d6<=0){const D q=ap-ac*(d2/(d2-d6));return DotD(q,q);}
    const double va=d3*d6-d5*d4;if(va<=0&&(d4-d3)>=0&&(d5-d6)>=0){const D q=bp-(t[2]-t[1])*((d4-d3)/((d4-d3)+(d5-d6)));return DotD(q,q);}
    const double sum=va+vb+vc;Need(std::isfinite(sum)&&sum!=0,"triangle distance finite domain");const D q=ap-ab*(vb/sum)-ac*(vc/sum);return DotD(q,q);}
double Clamp01(double t){return (std::max)(0.,(std::min)(1.,t));}
double EdgeDistance2(D a,D b,D c,D d){const D u=b-a,v=d-c,w=a-c;const double aa=DotD(u,u),bb=DotD(u,v),cc=DotD(v,v),dd=DotD(u,w),ee=DotD(v,w),den=aa*cc-bb*bb;
    Need(aa>0&&cc>0,"distance nonzero edges");double s=den>0?Clamp01((bb*ee-cc*dd)/den):0.;double t=(bb*s+ee)/cc;
    if(t<0){t=0;s=Clamp01(-dd/aa);}else if(t>1){t=1;s=Clamp01((bb-dd)/aa);}const D delta=w+u*s-v*t;return DotD(delta,delta);}
std::array<Vec3,3> Stored(const Triangle& t){std::array<Vec3,3> out;for(unsigned k=0;k<3;++k){const auto p=t[k];out[k]={static_cast<float>(p.x),static_cast<float>(p.y),static_cast<float>(p.z)};
    Need(std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&double(out[k].x)==p.x&&double(out[k].y)==p.y&&double(out[k].z)==p.z,"exact predicate requires actualf32, no hidden round");}return out;}
ExactTriangleContactResult3D Contact(const Triangle& a,const Triangle& b,std::span<const Vec3> shared={}){
    Pair();const auto value=ClassifyExactTriangleContact3D(Stored(a),Stored(b),shared);
    switch(value){case ExactTriangleContactResult3D::Disjoint:++disjoint;break;case ExactTriangleContactResult3D::AllowedSharedContact:++allowed;break;
        case ExactTriangleContactResult3D::ForbiddenContact:++forbidden;break;case ExactTriangleContactResult3D::Invalid:++invalid;throw std::runtime_error("exactcontact Invalid, unresolved first failure");}return value;
}
double DistanceDisjoint(const Triangle& a,const Triangle& b){double best=std::numeric_limits<double>::infinity();for(unsigned i=0;i<3;++i){
    best=(std::min)(best,PointDistance2(a[i],b));best=(std::min)(best,PointDistance2(b[i],a));
    for(unsigned j=0;j<3;++j)best=(std::min)(best,EdgeDistance2(a[i],a[(i+1u)%3u],b[j],b[(j+1u)%3u]));}return best;}
double TriangleDistance2(const Triangle& a,const Triangle& b){const auto contact=Contact(a,b);if(contact!=ExactTriangleContactResult3D::Disjoint)return 0.;return DistanceDisjoint(a,b);}
std::vector<Face> Faces(const Mesh3D& mesh,std::uint32_t first,std::uint32_t end){std::vector<Face> result;
    for(std::size_t i=0;i<mesh.indices.size();i+=3){Face f{mesh.indices[i],mesh.indices[i+1],mesh.indices[i+2]};unsigned hits{};for(auto v:f)hits+=v>=first&&v<end;
        Need(hits==0||hits==3,"component has mixed triangles");if(hits)result.push_back(f);}return result;}
using Key=std::array<std::uint32_t,3>;
Key KeyOf(const Vertex3D& v){const auto bits=[](float f){return f==0?0u:std::bit_cast<std::uint32_t>(f);};return{bits(v.x),bits(v.y),bits(v.z)};}
void Topology(const Mesh3D& mesh,std::uint32_t first,std::uint32_t end,std::size_t expected){
    const auto faces=Faces(mesh,first,end);Need(faces.size()==expected,"fixed component triangle count");std::map<Key,unsigned> alias;std::map<std::array<unsigned,2>,std::array<unsigned,2>> edges;
    for(std::uint32_t i=first;i<end;++i){const auto& v=mesh.vertices.at(i);Need(std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z)&&std::isfinite(v.u)&&std::isfinite(v.v),"finite owned XYZ/UV");
        const float n=Length(Normal(v));Need(n>.98f&&n<1.02f,"owned packed normal finite/unit");if(!alias.contains(KeyOf(v)))alias.emplace(KeyOf(v),static_cast<unsigned>(alias.size()));}
    std::vector<unsigned> refs(end-first);double volume6{};
    for(const auto& f:faces){const auto p=Points(mesh,f);const D area=CrossD(p[1]-p[0],p[2]-p[0]);Need(DotD(area,area)>1.e-16,"owned nonzero face area");
        const Vec3 sum=Normal(mesh.vertices[f[0]])+Normal(mesh.vertices[f[1]])+Normal(mesh.vertices[f[2]]);Need(DotD(area,{sum.x,sum.y,sum.z})>0,"owned face/normal orientation");
        volume6+=DotD(p[0],CrossD(p[1],p[2]));for(auto v:f)++refs[v-first];
        for(unsigned i=0;i<3;++i){unsigned a=alias.at(KeyOf(mesh.vertices[f[i]])),b=alias.at(KeyOf(mesh.vertices[f[(i+1u)%3u]]));Need(a!=b,"collapsed alias edge");const bool forward=a<b;if(!forward)std::swap(a,b);++edges[{a,b}][forward?0u:1u];}}
    Need(volume6>0&&std::isfinite(volume6),"closed component signed outward volume");for(auto uses:refs)Need(uses>0,"unused owned record");
    for(const auto& [edge,use]:edges){(void)edge;Need(use[0]==1&&use[1]==1,"closed alias topology/winding");}
}
// 以下expansion是誤差自由加乘；只驗實存f32點，不以幾何epsilon包容凸包外點。
using Expansion=std::vector<double>;
Expansion Grow(Expansion a,double b){Expansion out;double q=b;for(double x:a){const double s=q+x,bv=s-q,av=s-bv,br=x-bv,ar=q-av,e=ar+br;if(e!=0)out.push_back(e);q=s;}if(q!=0||out.empty())out.push_back(q);return out;}
Expansion Plus(Expansion a,const Expansion& b){for(double x:b)a=Grow(std::move(a),x);return a;}
Expansion Neg(Expansion a){for(double& x:a)x=-x;return a;}
Expansion Times(const Expansion& a,const Expansion& b){Expansion out;for(double x:a)for(double y:b){const double p=x*y,e=std::fma(x,y,-p);Need(std::isfinite(p)&&std::isfinite(e),"certificate expansion finite product");out=Grow(std::move(out),e);out=Grow(std::move(out),p);}return out;}
Expansion Difference(double a,double b){const double x=a-b,bv=a-x,av=x+bv,br=bv-b,ar=a-av,e=ar+br;Expansion out;if(e!=0)out.push_back(e);if(x!=0||out.empty())out.push_back(x);return out;}
int Sign(const Expansion& a){for(auto i=a.rbegin();i!=a.rend();++i)if(*i!=0)return *i>0?1:-1;return 0;}
Expansion Det(D a,D b,D c,D d){std::array<Expansion,3> u{Difference(b.x,a.x),Difference(b.y,a.y),Difference(b.z,a.z)},v{Difference(c.x,a.x),Difference(c.y,a.y),Difference(c.z,a.z)},w{Difference(d.x,a.x),Difference(d.y,a.y),Difference(d.z,a.z)};
    Expansion out;for(unsigned k=0;k<3;++k){const unsigned j=(k+1)%3,l=(k+2)%3;out=Plus(std::move(out),Times(u[k],Plus(Times(v[j],w[l]),Neg(Times(v[l],w[j])))));}return out;}
Expansion Det2(D a,D b,D c,bool xz){return Plus(Times(Difference(b.x,a.x),Difference(xz?c.z:c.y,xz?a.z:a.y)),Neg(Times(Difference(xz?b.z:b.y,xz?a.z:a.y),Difference(c.x,a.x))));}
bool Inside2(D q,const Triangle& t,bool xz){const int s=Sign(Det2(t[0],t[1],t[2],xz));if(!s)return false;for(unsigned k=0;k<3;++k)if(Sign(Det2(t[k],t[(k+1)%3],q,xz))*s<0)return false;return true;}
void PrintExpansion(const Expansion& e){std::cout<<'[';for(double x:e)std::cout<<std::hexfloat<<x<<',';std::cout<<']'<<std::defaultfloat;}
void PrintCertificate(std::uint32_t id,const std::vector<std::uint32_t>& refs,const std::vector<Expansion>& nums,const Expansion& den){std::cout<<"stored_f32_convex_certificate id="<<id<<" source_ids=";for(auto r:refs)std::cout<<r<<',';std::cout<<" denominator=";PrintExpansion(den);std::cout<<" numerator_expansions=";for(const auto& n:nums)PrintExpansion(n);std::cout<<'\n';}
// exact係數必須直接重建最終stored f32 XYZ，不只驗理論凸權重。
void ExactCombination1250(const Mesh3D& old,D q,const std::vector<std::uint32_t>& refs,const std::vector<Expansion>& nums,const Expansion& den){
    Need(refs.size()==nums.size()&&!refs.empty()&&Sign(den)!=0,"certificate coefficient domain");Expansion sum;
    for(const auto& n:nums){Need(Sign(n)*Sign(den)>=0,"certificate nonnegative convex coefficient");sum=Plus(std::move(sum),n);}
    Need(Sign(Plus(sum,Neg(den)))==0,"certificate exact coefficient partition");
    const auto axis=[](D p,unsigned k){return k==0?p.x:k==1?p.y:p.z;};
    for(unsigned k=0;k<3;++k){Expansion total;for(std::size_t i=0;i<refs.size();++i)total=Plus(std::move(total),Times(nums[i],{axis(Precise(old.vertices.at(refs[i])),k)}));
        Need(Sign(Plus(total,Neg(Times(den,{axis(q,k)}))))==0,"certificate exact storedXYZ reconstruction");}
}
bool TetraCertificate(const Mesh3D& old,D q,const std::vector<std::uint32_t>& refs,std::uint32_t id,bool report){
    for(unsigned i=0;i<refs.size();++i)for(unsigned j=i+1;j<refs.size();++j)for(unsigned k=j+1;k<refs.size();++k)for(unsigned l=k+1;l<refs.size();++l){
        const D a=Precise(old.vertices.at(refs[i])),b=Precise(old.vertices.at(refs[j])),c=Precise(old.vertices.at(refs[k])),d=Precise(old.vertices.at(refs[l]));
        const Expansion den=Det(a,b,c,d);const int s=Sign(den);if(!s)continue;
        const Expansion n1=Det(a,q,c,d),n2=Det(a,b,q,d),n3=Det(a,b,c,q),n0=Plus(Plus(Plus(den,Neg(n1)),Neg(n2)),Neg(n3));
        if(Sign(n0)*s<0||Sign(n1)*s<0||Sign(n2)*s<0||Sign(n3)*s<0)continue;
        Need(Sign(Plus(Plus(Plus(Plus(n0,n1),n2),n3),Neg(den)))==0,"exact coefficient partition of unity");
        ExactCombination1250(old,q,{refs[i],refs[j],refs[k],refs[l]},{n0,n1,n2,n3},den);
        if(report)PrintCertificate(id,{refs[i],refs[j],refs[k],refs[l]},{n0,n1,n2,n3},den);return true;
    }return false;
}
std::vector<std::uint32_t> MotherRefs(unsigned row,unsigned col){if(row==0||row==1||row==5)return{First+(row==5?3u:row)*16u+col};
    if(row==2||row==3)return{First+16u+col,First+32u+col,First+16u+(col+7u)%16u,First+32u+(col+9u)%16u};
    Need(row==4,"source mother row domain");return{First+32u+col,First+32u+(col+7u)%16u,First+32u+(col+9u)%16u};}
// 固定source域與有限refs；尋證只證實stored點，不改作者或試第二個形狀。
void RefDomain1250(const std::vector<std::uint32_t>& refs,std::size_t limit,bool motherOnly){
    Need(!refs.empty()&&refs.size()<=limit,"fixed certificate refs count cap");std::vector<std::uint32_t> seen;
    for(auto id:refs){Need(OldOwned(id)&&(!motherOnly||id<First+64u),"certificate source-owned span");
        Need(std::find(seen.begin(),seen.end(),id)==seen.end(),"certificate duplicate source ID");seen.push_back(id);}
}
bool BodySide1250(unsigned c){Need(c<16u,"fixed source column domain");const float a=2*Mistwatch::ExplorerArtAuthor::Pi*static_cast<float>(c)/16.f;return std::cos(a)>0;}
Vec3 FitSource1250(const Mesh3D& old,unsigned row,unsigned c){Need(row<4u&&c<16u,"fixed four source circles");
    const auto p=Position(old.vertices.at(First+row*16u+c));if(!BodySide1250(c))return p;
    const auto q=Position(old.vertices.at(First+row*16u+(8u+16u-c)%16u));
    return{static_cast<float>(double(p.x)*.65+double(q.x)*.35),static_cast<float>(double(p.y)*.65+double(q.y)*.35),static_cast<float>(double(p.z)*.65+double(q.z)*.35)};
}
std::vector<std::uint32_t> FitRefs1250(unsigned row,unsigned c){auto refs=MotherRefs(row,c);const auto originals=refs;
    for(auto id:originals){Need(id>=First&&id<First+64u,"original four-circle formula span");const unsigned col=(id-First)%16u;
        if(BodySide1250(col)){const auto mirror=First+((id-First)/16u)*16u+(8u+16u-col)%16u;if(std::find(refs.begin(),refs.end(),mirror)==refs.end())refs.push_back(mirror);}}
    RefDomain1250(refs,(row==2u||row==3u)?8u:6u,true);return refs;
}
bool PlanarCertificate1250(const Mesh3D& old,D q,const std::vector<std::uint32_t>& refs,std::uint32_t id,bool report){
    RefDomain1250(refs,16u,true);const float y=old.vertices.at(refs.front()).y;
    Need(std::isfinite(q.x)&&std::isfinite(q.y)&&std::isfinite(q.z)&&q.y==double(y),"exact source horizontal plane");
    for(auto ref:refs)Need(Bits(old.vertices.at(ref).y,y),"source refs one horizontal circle plane");
    for(unsigned i=0;i<refs.size();++i)for(unsigned j=i+1;j<refs.size();++j)for(unsigned k=j+1;k<refs.size();++k){Budget();
        const Triangle t{Precise(old.vertices.at(refs[i])),Precise(old.vertices.at(refs[j])),Precise(old.vertices.at(refs[k]))};if(!Inside2(q,t,true))continue;
        const auto den=Det2(t[0],t[1],t[2],true);const std::vector<Expansion> nums{Det2(t[1],t[2],q,true),Det2(t[2],t[0],q,true),Det2(t[0],t[1],q,true)};
        const std::vector<std::uint32_t> chosen{refs[i],refs[j],refs[k]};ExactCombination1250(old,q,chosen,nums,den);
        if(report)PrintCertificate(id,chosen,nums,den);return true;
    }return false;
}
std::vector<std::uint32_t> OriginalCircle1250(unsigned row){Need(row<4u,"source horizontal circle domain");std::vector<std::uint32_t> refs;
    for(unsigned c=0;c<16;++c)refs.push_back(First+row*16u+c);RefDomain1250(refs,16u,true);return refs;
}
bool BoundedTetra1250(const Mesh3D& old,D q,const std::vector<std::uint32_t>& refs,std::uint32_t id,bool report,bool attachment){
    RefDomain1250(refs,attachment?11u:8u,!attachment);return TetraCertificate(old,q,refs,id,report);
}
Vec3 ExpectedMother(const Mesh3D& old,unsigned r,unsigned c){Need(r<6u&&c<16u,"fixed six-circle replay domain");
    if(r==0u||r==1u||r==5u)return FitSource1250(old,r==5u?3u:r,c);
    const auto l=FitSource1250(old,r==4u?2u:1u,c),u=FitSource1250(old,2u,r==4u?(c+7u)%16u:c),a=FitSource1250(old,r==4u?2u:1u,r==4u?(c+9u)%16u:(c+7u)%16u);
    if(r==4u)return{static_cast<float>((double(l.x)*.925+double(u.x)*.0375)+double(a.x)*.0375),l.y,static_cast<float>((double(l.z)*.925+double(u.z)*.0375)+double(a.z)*.0375)};
    const auto d=FitSource1250(old,2u,(c+9u)%16u);const double t=r==2u?1./3.:2./3.,s=r==2u?.015:.030,w0=(1.-s)*(1.-t),w1=(1.-s)*t,w2=s/2.,w3=s/2.;
    return{static_cast<float>(((double(l.x)*w0+double(u.x)*w1)+double(a.x)*w2)+double(d.x)*w3),static_cast<float>(((double(l.y)*w0+double(u.y)*w1)+double(a.y)*w2)+double(d.y)*w3),static_cast<float>(((double(l.z)*w0+double(u.z)*w1)+double(a.z)*w2)+double(d.z)*w3)};
}
Vec3 ExpectedCap1250(const Mesh3D& old,unsigned cap){Need(cap<2u,"two fixed source caps");double x{},y{},z{};const unsigned row=cap?5u:0u;
    for(unsigned c=0;c<16;++c){const auto p=ExpectedMother(old,row,c);x+=double(p.x);y+=double(p.y);z+=double(p.z);}
    const Vec3 expected{static_cast<float>(x/16.),static_cast<float>(y/16.),static_cast<float>(z/16.)};
    Need(Bits(expected.y,old.vertices.at(First+(cap?3u:0u)*16u).y),"independent true cap plane bits");return expected;
}
void Containment(const SkinnedModel3D& old,const SkinnedModel3D& now,bool report){
    Need(std::numeric_limits<double>::is_iec559&&std::numeric_limits<double>::digits==53&&std::fegetround()==FE_TONEAREST,"exact expansion IEEE binary64 nearest domain");
    const auto& original=old.bindMesh;const auto& mesh=now.bindMesh;unsigned certified{};
    for(unsigned r=0;r<6;++r)for(unsigned c=0;c<16;++c){const auto id=First+r*16u+c;const auto refs=FitRefs1250(r,c);const auto q=Precise(mesh.vertices.at(id));
        if(r==0u||r==1u||r==5u){const unsigned sourceRow=r==5u?3u:r;const auto sourceID=First+sourceRow*16u+c;
            Need(SameVec(Position(mesh.vertices[id]),FitSource1250(original,sourceRow,c))&&Bits(mesh.vertices[id].u,original.vertices[sourceID].u)&&Bits(mesh.vertices[id].v,original.vertices[sourceID].v)&&mesh.vertices[id].color==original.vertices[sourceID].color,"source-circle fitXYZ with original UV/color raw");
            if(!BodySide1250(c)){Need(SameVec(Position(mesh.vertices[id]),Position(original.vertices[sourceID])),"rear source circle rawXYZ");if(report)PrintCertificate(id,{sourceID},{{1.}},{1.});}
            else Need(PlanarCertificate1250(original,q,OriginalCircle1250(sourceRow),id,report),"stored fitted source circle outside original convex triangle");
        }else if(r==4u)Need(PlanarCertificate1250(original,q,refs,id,report),"stored upper-ring outside original exact source triangle");
        else Need(BoundedTetra1250(original,q,refs,id,report,false),"stored middle-ring outside bounded original source tetrahedron");++certified;
    }
    for(unsigned c=0;c<2;++c){const auto id=First+96u+c,ref=First+64u+c;const auto expected=ExpectedCap1250(original,c);
        Need(SameVec(Position(mesh.vertices[id]),expected)&&Bits(mesh.vertices[id].u,original.vertices[ref].u)&&Bits(mesh.vertices[id].v,original.vertices[ref].v)&&mesh.vertices[id].color==original.vertices[ref].color,"true-plane doublemean cap with original UV/color raw");
        Need(PlanarCertificate1250(original,Precise(mesh.vertices[id]),OriginalCircle1250(c?3u:0u),id,report),"stored true-plane cap outside original convex triangle");++certified;
    }
    // 最終f32附件點與固定原面／凸橋形成的source-only tetra包含證書；不以理論offset代替。
    const auto oldPouch=Faces(original,6997,7045);const std::array<Face,2> bridge{{{6997,6999,7053},{6997,7053,7055}}};
    for(unsigned part=0;part<2;++part){const std::uint32_t begin=First+98u+part*60u,end=begin+60u;const unsigned row0=part?3u:2u;
        for(std::uint32_t id=begin;id<end;++id){unsigned node=20u;for(unsigned k=0;k<20u;++k)if(KeyOf(mesh.vertices[id])==KeyOf(mesh.vertices[begin+k])){Need(node==20u,"ambiguous sheet alias source");node=k;}
            Need(node<20u,"wall point is not exact supported-sheet alias");const unsigned r=node/10u,c=(node%10u)/2u;auto refs=FitRefs1250(row0+r,6u+c);const D q=Precise(mesh.vertices[id]);Face chosen{};bool found=false;
            if(!part){for(auto f:oldPouch){const auto t=Points(original,f);if(!Bits(original.vertices[f[0]].z,-.333f)||!Bits(original.vertices[f[1]].z,-.333f)||!Bits(original.vertices[f[2]].z,-.333f))continue;
                    if(Inside2(q,t,false)){chosen=f;found=true;break;}}}
            else for(auto f:bridge)if(Inside2(q,Points(original,f),false)){chosen=f;found=true;break;}
            Need(found,"stored sheet XY has no fixed original-owned projection witness");for(auto ref:chosen)if(std::find(refs.begin(),refs.end(),ref)==refs.end())refs.push_back(ref);
            for(auto ref:refs)Need(OldOwned(ref),"convex certificate source ID outside original owned scope");
            Need(BoundedTetra1250(original,q,refs,id,report,true),"stored attachment point outside original-owned convex certificate");++certified;
        }
    }
    Need(certified==218u,"every final stored owned record certificate required");
    // row1原支持點須是實際母袋面，絕非unused bbox sentinel。
    const auto faces=Faces(mesh,First,First+98u);unsigned refs{};for(auto f:faces)for(auto v:f)refs+=v==6955u;
    Need(refs>=4u&&SameVec(Position(mesh.vertices[6955]),Position(original.vertices[6955]))&&mesh.vertices[6955].color==original.vertices[6955].color&&Bits(mesh.vertices[6955].u,original.vertices[6955].u)&&Bits(mesh.vertices[6955].v,original.vertices[6955].v),"original row1 col8 real referenced raw support");
    if(report)std::cout<<"all_stored_owned_convex_certificates="<<certified<<" retained_row1_col8_surface_references="<<refs<<" full_hull_constructed=0 exact_local_certificates=1\n";
}
void CertificateControls1250(const SkinnedModel3D& old,const SkinnedModel3D& view){
    const auto& mesh=old.bindMesh;const D q=Precise(view.bindMesh.vertices.at(First+32u));
    std::vector<std::uint32_t> nine;for(unsigned i=0;i<9;++i)nine.push_back(First+i);
    Reject([&]{(void)BoundedTetra1250(mesh,q,nine,First+32u,false,false);},"middle refs9 exceeds fixed8 accepted");
    std::vector<std::uint32_t> twelve;for(unsigned i=0;i<12;++i)twelve.push_back(First+i);
    Reject([&]{(void)BoundedTetra1250(mesh,q,twelve,First+32u,false,true);},"attachment refs12 exceeds fixed11 accepted");
    Reject([&]{RefDomain1250({0u,First,First+1u},16u,true);},"certificate source span outside owned accepted");
    auto wrongCap=Position(view.bindMesh.vertices.at(First+96u));wrongCap.y=std::nextafter(wrongCap.y,-std::numeric_limits<float>::infinity());
    Reject([&]{(void)PlanarCertificate1250(mesh,{wrongCap.x,wrongCap.y,wrongCap.z},OriginalCircle1250(0u),First+96u,false);},"cap plane1ULP certificate accepted");
    const auto p=Precise(mesh.vertices.at(First));
    Reject([&]{ExactCombination1250(mesh,p,{First,First+1u},{{-1.},{2.}},{1.});},"negative convex coefficient accepted");
    Reject([&]{ExactCombination1250(mesh,p,{First+1u},{{1.}},{1.});},"wrong storedXYZ coefficient reconstruction accepted");
}
void Geometry(const SkinnedModel3D& old,const SkinnedModel3D& now){
    Need(now.Valid()&&now.bindMesh.vertices.size()==9281&&now.bindMesh.indices.size()==37032,"candidate model validity/count");
    // 只用原正式模型已存f32位置獨立重播，不呼叫新作者。
    for(unsigned r=0;r<6;++r)for(unsigned c=0;c<16;++c){const Vec3 expected=ExpectedMother(old.bindMesh,r,c);const auto& v=now.bindMesh.vertices[First+r*16u+c];
        Need(SameVec(Position(v),expected)&&Bits(v.u,static_cast<float>(c)/16.f)&&Bits(v.v,expected.y),"fixed source-convex six-ring position/UV replay");
        Need(v.x>=-.15f&&v.x<=.15f&&v.y>=1.03f&&v.y<=1.39f&&v.z>=-.315f&&v.z<=-.095f,"mother original envelope");}
    Containment(old,now,false);
    struct Part {unsigned row0,row1;float gap,thick;std::uint32_t first,end;std::size_t triangles;};
    const std::array<Part,2> parts{{{2,3,.004f,.012f,First+98u,First+158u,36},{3,4,.020f,.011f,First+158u,NewEnd,36}}};
    for(const auto& part:parts){const unsigned countRows=part.row1-part.row0+1u;
        for(unsigned r=0;r<countRows;++r)for(unsigned c=0;c<5;++c){const auto& source=now.bindMesh.vertices[First+(part.row0+r)*16u+6u+c];
            const auto& outer=now.bindMesh.vertices[part.first+2u*(r*5u+c)],&inner=now.bindMesh.vertices[part.first+2u*(r*5u+c)+1u];
            Need(Bits(outer.x,source.x)&&Bits(outer.y,source.y)&&Bits(outer.z,source.z+(-part.gap-part.thick))&&
                Bits(inner.x,source.x)&&Bits(inner.y,source.y)&&Bits(inner.z,source.z-part.gap),"sheet source-derived offsets/old thickness");
            Need(Bits(outer.u,source.x)&&Bits(outer.v,source.y)&&Bits(inner.u,source.x)&&Bits(inner.v,source.y),"sheet XY UV responsibility");}
        Topology(now.bindMesh,part.first,part.end,part.triangles);}
    Topology(now.bindMesh,First,First+98u,192);
    for(std::uint32_t i=First;i<NewEnd;++i){const auto& s=now.influences.at(i);Need(s.joints[0]==1&&s.weights[0]==65535&&
        !s.joints[1]&&!s.joints[2]&&!s.joints[3]&&!s.weights[1]&&!s.weights[2]&&!s.weights[3],"owned original Rigid(1) weights");}
    const auto mother=Faces(now.bindMesh,First,First+98u),pouch=Faces(now.bindMesh,First+98u,First+158u),flap=Faces(now.bindMesh,First+158u,NewEnd);
    const auto minimum=[&](const auto& a,const auto& b){double best=std::numeric_limits<double>::infinity();for(auto x:a)for(auto y:b){const double q=TriangleDistance2(Points(now.bindMesh,x),Points(now.bindMesh,y));Need(std::isfinite(q)&&q>=0,"whole triangle distance finite");best=(std::min)(best,q);}return std::sqrt(best);};
    const double pouchGap=minimum(pouch,mother),flapGap=minimum(flap,mother),between=minimum(pouch,flap);
    std::cout<<"whole_face_distances pouch_mother="<<pouchGap<<" flap_mother="<<flapGap<<" attachment_attachment="<<between<<'\n';
    Need(pouchGap>=.0025&&flapGap>=.0025&&between>=.0025,"complete attachment triangle clearance/intersection failed");
    std::vector<SkinnedVertex3D> gpu;Need(BuildGpuSkinnedVertices(now,gpu)&&gpu.size()==9281,"production GPU-record preparation rejected candidate");
    std::vector<std::byte> encoded;std::string error;Need(EncodeSkinnedModelAsset(now,encoded,&error),"candidate canonical encode");const auto loaded=LoadSkinnedModelAsset(encoded);Need(bool(loaded),"candidate independent decoder");
    Need(loaded.model.bindMesh.indices==now.bindMesh.indices&&loaded.model.tangents==now.tangents,"roundtrip indices/tangents");
    for(std::size_t i=0;i<9281;++i)Need(VertexRaw(now.bindMesh.vertices[i],loaded.model.bindMesh.vertices[i])&&now.influences[i].joints==loaded.model.influences[i].joints&&now.influences[i].weights==loaded.model.influences[i].weights,"roundtrip every record/weight");
}
void Ranges(const Mistwatch::Explorer& old,const Mistwatch::Explorer& now){
    Need(old.art.joins.size()==14&&old.art.joins.size()==now.art.joins.size()&&old.art.skinColorVertices==now.art.skinColorVertices,"nonbackpack metadata changed");
    for(unsigned j=0;j<15;++j)Need(old.art.criticalVertices[j]==now.art.criticalVertices[j],"critical vertex inventory changed");
    for(std::size_t j=0;j<old.art.joins.size();++j)Need(old.art.joins[j].joint==now.art.joins[j].joint&&old.art.joins[j].boundary==now.art.joins[j].boundary,"joint boundary metadata changed");
    for(unsigned i=0;i<5;++i){const auto& a=old.art.ranges[i];const auto& b=now.art.ranges[i];Need(unsigned(a.material)==unsigned(b.material)&&
        b.firstIndex==a.firstIndex+(i>=2?240u:0u)&&b.indexCount==a.indexCount+(i==1?240u:0u),"five ranges explicit Leather delta");}
    for(unsigned i=0;i<2;++i){const auto& a=old.art.eyeLensSpans[i];const auto& b=now.art.eyeLensSpans[i];Need(b.firstIndex==a.firstIndex+240u&&b.indexCount==a.indexCount&&b.firstVertex==a.firstVertex&&b.vertexCount==a.vertexCount,"original lens span identity");}
    const auto a=Mistwatch::ExplorerArtMaterials(),b=Mistwatch::ExplorerArtMaterials();
    for(unsigned i=0;i<5;++i){const auto& x=a[i];const auto& y=b[i];Need(Bits(x.tint.r,y.tint.r)&&Bits(x.tint.g,y.tint.g)&&Bits(x.tint.b,y.tint.b)&&Bits(x.tint.a,y.tint.a)&&
        Bits(x.surface.roughness,y.surface.roughness)&&Bits(x.surface.metallic,y.surface.metallic)&&Bits(x.surface.emissive,y.surface.emissive)&&Bits(x.surface.occlusion,y.surface.occlusion)&&
        x.castsShadow==y.castsShadow&&x.textures.value==y.textures.value&&Bits(x.textureScale,y.textureScale)&&x.useUv==y.useUv&&x.materialClass==y.materialClass&&x.shadingModel==y.shadingModel&&
        x.alphaMode==y.alphaMode&&Bits(x.alphaCutoff,y.alphaCutoff)&&x.doubleSided==y.doubleSided&&Bits(x.normalScale,y.normalScale)&&Bits(x.baseTextureScale,y.baseTextureScale)&&Bits(x.detailTextureScale,y.detailTextureScale),"all five material fields changed");}
    Mistwatch::ExplorerSurface::Plan plan;Need(Mistwatch::ExplorerSurface::BuildPlan(now.model,now.art,plan),"candidate nine-window production plan");
    DrawList draw;SkinningWorkspace3D work;const auto p=AddSkinPaletteFromPose(draw,now.model,now.pose,work);Mistwatch::ExplorerSurface::Submission receipt;
    SkinnedMeshHandle3D h{101,now.model.skeletonCompatibilityId,15,201,0,0,9281,37032};
    Need(Mistwatch::ExplorerSurface::Submit(draw,now.model,now.art,h,p,Matrix4::Identity(),b,receipt),"candidate production Submit");
    Need(receipt.acceptedWindows==9&&receipt.skinWindows==3&&receipt.eyeWindows==2&&receipt.hairWindows==1&&receipt.clothWindows==1&&receipt.texturedWindows==0&&receipt.lensIndices==1404,"unchanged formal untextured routing");
}
void Poses(Mistwatch::Explorer& old,Mistwatch::Explorer& now){
    const auto terrain=Mistwatch::CreateTerrain();const auto solids=Mistwatch::WorldSolids(true);Vec3 feet{};SkinningWorkspace3D ow,nw;Mesh3D a,b;
    for(unsigned tick=0;tick<242;++tick){const bool crouched=tick==241;const Vec3 step=tick<180?Vec3{0,0,.025f}:Vec3{};feet=feet+step;
        old.Update(step,1.f/60.f,crouched);now.Update(step,1.f/60.f,crouched);Need(old.PlaceFeet(feet,true,crouched,terrain,solids)&&now.PlaceFeet(feet,true,crouched,terrain,solids),"original foot IK rejected");
        Need(old.planted==now.planted&&SameVec(old.footTargets[0],now.footTargets[0])&&SameVec(old.footTargets[1],now.footTargets[1]),"foot target/support changed");
        for(unsigned j=0;j<15;++j){TransformRaw(old.pose.localTransforms[j],now.pose.localTransforms[j]);MatrixRaw(old.pose.globalMatrices[j],now.pose.globalMatrices[j]);}
        Need(SkinModelInto(old.model,old.pose,ow,a)&&SkinModelInto(now.model,now.pose,nw,b),"stand/walk/crouch CPU skin");
        for(std::uint32_t i=0;i<9225;++i)if(!OldOwned(i))Need(VertexRaw(a.vertices[i],b.vertices[Remap(i)]),"posed nonowned/sole vertex changed");
        const auto x=a.Bounds(),y=b.Bounds();Need(SameVec(x.center,y.center)&&SameVec(x.extents,y.extents),"posed whole-player bounds changed");
        Need(y.center.y-y.extents.y>=-.02f&&y.center.y+y.extents.y<(crouched?1.40f:1.83f),"original sole/height contract");}
}

// 下列檢查actual模型而非排序view；V1 canonical語義獨立依writer/loader兩次正規化。
void Canonical(const SkinnedModel3D& now){const auto& mesh=now.bindMesh;
    std::vector<std::byte> encoded;std::string error;Need(EncodeSkinnedModelAsset(now,encoded,&error),"canonical candidate encode");
    const auto loaded=LoadSkinnedModelAsset(encoded);Need(bool(loaded),"independent canonical decoder");
    // V1作者來源獨立expected：writer與loader各自一次正規化，不以decoded欄位刷新預期。
    Need(now.skeleton.joints.size()==15&&now.inverseBindMatrices.size()==15,"V1 fixed source rig count");
    Skeleton3D expectedRig=now.skeleton;
    for(unsigned j=0;j<15;++j){
        const auto& source=now.skeleton.joints[j].bindTransform;
        const Quaternion3D written=source.usesQuaternion?NormalizeQuaternion3D(source.quaternion):QuaternionFromEuler3D(source.rotation);
        auto& expected=expectedRig.joints[j].bindTransform;
        expected.rotation={};expected.quaternion=NormalizeQuaternion3D(written);expected.usesQuaternion=true;
    }
    std::vector<std::string> legacyNames;legacyNames.reserve(15);
    for(unsigned j=0;j<15;++j)legacyNames.push_back("Joint_"+std::to_string(j));
    const auto legacyExpected=ComputeSkeletonCompatibilityId3D(legacyNames,expectedRig,now.inverseBindMatrices,SkeletonCompatibilityProvenance3D::LegacySynthetic);
    const auto originalStructural=ComputeStructuralSkeletonCompatibilityId3D(now.skeleton,now.inverseBindMatrices);
    const auto decodedStructuralExpected=ComputeStructuralSkeletonCompatibilityId3D(expectedRig,now.inverseBindMatrices);
    Need(bool(legacyExpected)&&bool(originalStructural)&&bool(decodedStructuralExpected)&&now.skeletonCompatibilityId==originalStructural,"V1 source-derived identity descriptors");
    const auto readback=[&](const SkinnedModel3D& actual,bool witness){
        const bool indexEqual=actual.bindMesh.indices==now.bindMesh.indices;
        const bool tangentEqual=now.tangents.empty()&&actual.tangents.empty();
        const bool legacyEqual=actual.skeletonCompatibilityId==legacyExpected;
        const auto actualStructural=ComputeStructuralSkeletonCompatibilityId3D(actual.skeleton,actual.inverseBindMatrices);
        if(witness){
            std::cout<<"v1_readback indices_equal="<<indexEqual<<" source_tangents_empty="<<now.tangents.empty()
                <<" decoded_tangents_empty="<<actual.tangents.empty()<<" tangents_equal="<<tangentEqual
                <<" legacy_identity_equal="<<legacyEqual
                <<" source_structural_id=0x"<<std::hex<<now.skeletonCompatibilityId.value
                <<" decoded_legacy_id=0x"<<actual.skeletonCompatibilityId.value
                <<" source_expected_legacy_id=0x"<<legacyExpected.value
                <<" source_expected_decoded_structural_id=0x"<<decodedStructuralExpected.value
                <<" decoded_recomputed_structural_id=0x"<<actualStructural.value<<std::dec
                <<" source_joint0_usesQuaternion="<<now.skeleton.joints[0].bindTransform.usesQuaternion
                <<" decoded_joint0_usesQuaternion="<<actual.skeleton.joints[0].bindTransform.usesQuaternion<<'\n';
        }
        Need(indexEqual,"V1 actual full index bytes");
        Need(tangentEqual,"V1 source and decoded tangents must both be empty");
        Need(legacyEqual&&actualStructural==decodedStructuralExpected,"V1 independently source-defined LegacySynthetic/Structural descriptors");
        Need(actual.Valid()&&actual.bindMesh.vertices.size()==9281&&actual.skeleton.joints.size()==15&&actual.inverseBindMatrices.size()==15&&actual.influences.size()==9281,"V1 actual model/rig/record counts");
        for(unsigned j=0;j<15;++j){
            Need(actual.skeleton.joints[j].parent==now.skeleton.joints[j].parent,"V1 exact source parent");
            const auto& expected=expectedRig.joints[j].bindTransform;const auto& found=actual.skeleton.joints[j].bindTransform;
            Need(SameVec(found.translation,now.skeleton.joints[j].bindTransform.translation)&&SameVec(found.scale,now.skeleton.joints[j].bindTransform.scale),"V1 exact source translation/scale");
            Need(SameVec(found.rotation,Vec3{})&&found.usesQuaternion,"V1 required decoded Euler-zero/quaternion representation");
            Need(Bits(found.quaternion.x,expected.quaternion.x)&&Bits(found.quaternion.y,expected.quaternion.y)&&Bits(found.quaternion.z,expected.quaternion.z)&&Bits(found.quaternion.w,expected.quaternion.w),"V1 exact writer-then-loader quaternion bits");
            MatrixRaw(actual.inverseBindMatrices[j],now.inverseBindMatrices[j]);
        }
        for(std::size_t i=0;i<9281;++i)Need(VertexRaw(mesh.vertices[i],actual.bindMesh.vertices[i])&&now.influences[i].joints==actual.influences[i].joints&&now.influences[i].weights==actual.influences[i].weights,"roundtrip every record/influence");
    };
    readback(loaded.model,true);
    // 僅新增格式oracle負控；原十項V994負控與全部幾何、姿態門完全保留。
    auto badReadback=loaded.model;badReadback.skeletonCompatibilityId.value^=1ull;
    Reject([&]{readback(badReadback,false);},"V1 identity mutation accepted");
    badReadback=loaded.model;badReadback.skeleton.joints[0].bindTransform.quaternion.x+=.01f;
    Reject([&]{readback(badReadback,false);},"V1 quaternion mutation accepted");
    badReadback=loaded.model;badReadback.skeleton.joints[1].parent=-1;
    Reject([&]{readback(badReadback,false);},"V1 parent mutation accepted");
    badReadback=loaded.model;badReadback.skeleton.joints[0].bindTransform.translation.x+=.01f;
    Reject([&]{readback(badReadback,false);},"V1 translation mutation accepted");
    badReadback=loaded.model;badReadback.skeleton.joints[0].bindTransform.scale.x+=.01f;
    Reject([&]{readback(badReadback,false);},"V1 scale mutation accepted");
    badReadback=loaded.model;badReadback.inverseBindMatrices[0][0][3]+=.01f;
    Reject([&]{readback(badReadback,false);},"V1 inverseBind mutation accepted");
    badReadback=loaded.model;badReadback.skeleton.joints[0].bindTransform.rotation.x=.01f;
    Reject([&]{readback(badReadback,false);},"V1 decoded Euler mutation accepted");
    badReadback=loaded.model;badReadback.skeleton.joints[0].bindTransform.usesQuaternion=false;
    Reject([&]{readback(badReadback,false);},"V1 decoded quaternion flag mutation accepted");
    Budget();
}
std::vector<std::byte> Encode(const SkinnedModel3D& m){std::vector<std::byte> bytes;Need(EncodeSkinnedModelAsset(m,bytes),"actual full canonical encode");return bytes;}
std::string Hash(const std::vector<std::byte>& bytes){std::array<std::byte,32> digest{};Need(WorldDeltaSha2563D(bytes,digest),"whole canonical SHA");const char* hex="0123456789ABCDEF";std::string out;for(auto b:digest){const auto v=std::to_integer<unsigned>(b);out+=hex[v>>4];out+=hex[v&15u];}return out;}
void ArtSame(const Mistwatch::ExplorerArtReceipt& a,const Mistwatch::ExplorerArtReceipt& b);
void Inverse(const Mistwatch::Explorer& old,const Mistwatch::Explorer& now){auto back=now.model;
    for(std::uint32_t i=6931;i<7093;++i){back.bindMesh.vertices[i]=old.model.bindMesh.vertices[i];back.influences[i]=old.model.influences[i];}
    back.bindMesh.vertices.resize(9225);back.influences.resize(9225);back.bindMesh.indices.erase(back.bindMesh.indices.begin()+12084,back.bindMesh.indices.begin()+12876);
    back.bindMesh.indices.insert(back.bindMesh.indices.begin()+12084,old.model.bindMesh.indices.begin()+12084,old.model.bindMesh.indices.begin()+12636);back.bindMesh.InvalidateRaycastIndex();
    Need(Encode(back)==Encode(old.model),"whole raw inverse to actual current9225 input, not7861");
    auto art=now.art;Need(art.ranges[1].indexCount>=240,"inverse Leather count domain");art.ranges[1].indexCount-=240;
    for(unsigned i=2;i<5;++i){Need(art.ranges[i].firstIndex>=240,"inverse range offset domain");art.ranges[i].firstIndex-=240;}
    for(auto& lens:art.eyeLensSpans){Need(lens.firstIndex>=240,"inverse lens offset domain");lens.firstIndex-=240;}ArtSame(old.art,art);
}
Vec3 UnitActual(Vec3 v){const float n=std::sqrt(Dot(v,v));Need(std::isfinite(n)&&n>1.e-12f,"independent source Finish unit domain");return v*(1.f/n);}
void Normals(const SkinnedModel3D& view){std::array<Vec3,218> sums{};for(std::size_t at=0;at<view.bindMesh.indices.size();at+=3){const auto a=view.bindMesh.indices[at],b=view.bindMesh.indices[at+1],c=view.bindMesh.indices[at+2];if(!NewOwned(a))continue;
    Need(NewOwned(b)&&NewOwned(c),"owned frame mixed face");const auto n=UnitActual(Cross(Position(view.bindMesh.vertices[b])-Position(view.bindMesh.vertices[a]),Position(view.bindMesh.vertices[c])-Position(view.bindMesh.vertices[a])));
    for(auto i:{a,b,c})sums[i-First]=sums[i-First]+n;
    }for(std::uint32_t i=First;i<NewEnd;++i){Need(Dot(sums[i-First],sums[i-First])>=1.e-10f,"independent incident normal nonopposing");Need(view.bindMesh.vertices[i].normal==PackNormalSnorm8(UnitActual(sums[i-First])),"all218 finalpacked incident normals exact");}}
// 固定source sheet alias，不因位置碰巧相同授權；三閉域之間沒有共享alias。
std::uint32_t SourceAlias(std::uint32_t id){Need(NewOwned(id),"alias source-owned domain");if(id<7029)return id;
    const auto begin=id<7089?7029u:7089u,local=id-begin;if(local<20)return id;
    constexpr std::array<std::array<unsigned,2>,10> boundary{{{0,0},{0,1},{0,2},{0,3},{0,4},{1,4},{1,3},{1,2},{1,1},{1,0}}};
    const auto edge=(local-20u)/4u,corner=(local-20u)%4u;Need(edge<10,"fixed sheet wall edge domain");const auto q=boundary[corner<2?edge:(edge+1u)%10u];
    return begin+2u*(q[0]*5u+q[1])+(corner%2u);
}
bool Near(const Triangle& a,const Triangle& b,double margin){for(unsigned axis=0;axis<3;++axis){const auto get=[&](D p){return axis==0?p.x:axis==1?p.y:p.z;};
    double al=get(a[0]),ah=al,bl=get(b[0]),bh=bl;for(unsigned k=1;k<3;++k){al=(std::min)(al,get(a[k]));ah=(std::max)(ah,get(a[k]));bl=(std::min)(bl,get(b[k]));bh=(std::max)(bh,get(b[k]));}
    if(ah+margin<bl||bh+margin<al)return false;}return true;}
void Contacts(const SkinnedModel3D& view){const auto& mesh=view.bindMesh;std::vector<Face> owned,protectedFaces;
    for(std::size_t at=0;at<mesh.indices.size();at+=3){Face f{mesh.indices[at],mesh.indices[at+1],mesh.indices[at+2]};unsigned count{};for(auto i:f)count+=NewOwned(i);
        Need(count==0||count==3,"full contact complete triangle scope");(count?owned:protectedFaces).push_back(f);}
    Need(owned.size()==264&&protectedFaces.size()==12080,"full current combined contact populations");
    for(auto f:owned)for(auto id:f){const auto alias=SourceAlias(id);Need(SameVec(Position(mesh.vertices[id]),Position(mesh.vertices[alias])),"source-owned alias raw position replay");}
    for(std::size_t i=0;i<owned.size();++i)for(std::size_t j=i+1;j<owned.size();++j){Budget();const auto a=Points(mesh,owned[i]),b=Points(mesh,owned[j]);if(!Near(a,b,0))continue;
        std::vector<Vec3> shared;for(auto ai:owned[i])for(auto bi:owned[j])if(SourceAlias(ai)==SourceAlias(bi)){const auto p=Position(mesh.vertices[ai]);bool seen=false;for(auto q:shared)seen=seen||SameVec(p,q);if(!seen)shared.push_back(p);}
        Need(shared.size()<=2,"duplicate whole triangle/shared3 first failure");const auto result=Contact(a,b,shared);
        if(result==ExactTriangleContactResult3D::ForbiddenContact){std::cerr<<"owned_exact_first faces="<<i<<','<<j<<" ids="<<owned[i][0]<<','<<owned[i][1]<<','<<owned[i][2]<<" vs "<<owned[j][0]<<','<<owned[j][1]<<','<<owned[j][2]<<'\n';throw std::runtime_error("owned proper/coplanar self contact");}
    }
    for(std::size_t i=0;i<owned.size();++i)for(std::size_t j=0;j<protectedFaces.size();++j){Budget();const auto a=Points(mesh,owned[i]),b=Points(mesh,protectedFaces[j]);if(!Near(a,b,.0025))continue;
        const auto result=Contact(a,b);if(result!=ExactTriangleContactResult3D::Disjoint){std::cerr<<"protected_exact_first faces="<<i<<','<<j<<" ids="<<protectedFaces[j][0]<<','<<protectedFaces[j][1]<<','<<protectedFaces[j][2]<<'\n';throw std::runtime_error("owned vs complete protected proper/coplanar contact");}
        const double d=DistanceDisjoint(a,b);Need(std::isfinite(d)&&d>=0,"full protected distance finite");if(d<.0025*.0025){std::cerr<<"protected_distance_first faces="<<i<<','<<j<<" distance="<<std::sqrt(d)<<'\n';throw std::runtime_error("owned vs complete protected2.5mm first failure");}
    }
    Need(narrow==disjoint+allowed+forbidden+invalid,"exact class counts equal every narrow call");
}
void ExactControls(){const Triangle a{{{0,0,0},{1,0,0},{0,1,0}}},edge{{{1,0,0},{0,0,0},{0,-1,0}}},cross{{{.25,.25,-1},{.25,.25,1},{1,.25,0}}},inside{{{.125,.125,0},{.5,.125,0},{.125,.5,0}}};
    const std::array<Vec3,2> shared{{{0,0,0},{1,0,0}}};Need(Contact(a,edge,shared)==ExactTriangleContactResult3D::AllowedSharedContact,"true complete sharededge");
    Reject([&]{Need(Contact(a,cross)!=ExactTriangleContactResult3D::ForbiddenContact,"proper rejected");},"proper exact mutation accepted");
    Reject([&]{Need(Contact(a,inside)!=ExactTriangleContactResult3D::ForbiddenContact,"coplanar rejected");},"coplanar containment accepted");
    const std::array<Vec3,1> falseShared{{{.5f,.5f,0}}};Reject([&]{(void)Contact(a,edge,falseShared);},"illegal shared declaration accepted");
    const std::array<Vec3,2> duplicate{{{0,0,0},{0,0,0}}};Reject([&]{(void)Contact(a,edge,duplicate);},"duplicate shared declaration accepted");
    const double ulp=std::nextafter(0.f,1.f);const Triangle parallel{{{0,0,ulp},{1,0,ulp},{0,1,ulp}}};Need(Contact(a,parallel)==ExactTriangleContactResult3D::Disjoint,"oneULP real separation remains disjoint");
}
void State(const Mistwatch::Explorer& a,const Mistwatch::Explorer& b){Need(Bits(a.phase,b.phase)&&Bits(a.yaw,b.yaw)&&Bits(a.locomotionDistance,b.locomotionDistance)&&SameVec(a.lastFeet,b.lastFeet)&&
    a.feetInitialized==b.feetInitialized&&a.planted==b.planted,"phase and IK state changed");for(unsigned i=0;i<2;++i)Need(SameVec(a.footTargets[i],b.footTargets[i]),"foot target changed");
    Need(a.pose.localTransforms.size()==15&&b.pose.localTransforms.size()==15&&a.pose.globalMatrices.size()==15&&b.pose.globalMatrices.size()==15,"full15 pose domains");
    for(unsigned j=0;j<15;++j){TransformRaw(a.pose.localTransforms[j],b.pose.localTransforms[j]);MatrixRaw(a.pose.globalMatrices[j],b.pose.globalMatrices[j]);}}
void ArtSame(const Mistwatch::ExplorerArtReceipt& a,const Mistwatch::ExplorerArtReceipt& b){Need(a.skinColorVertices==b.skinColorVertices&&a.criticalVertices==b.criticalVertices&&a.joins.size()==b.joins.size(),"save whole art inventories");
    for(unsigned i=0;i<5;++i)Need(a.ranges[i].firstIndex==b.ranges[i].firstIndex&&a.ranges[i].indexCount==b.ranges[i].indexCount&&a.ranges[i].material==b.ranges[i].material,"save materialrange raw");
    for(unsigned i=0;i<2;++i){const auto& x=a.eyeLensSpans[i];const auto& y=b.eyeLensSpans[i];Need(x.firstIndex==y.firstIndex&&x.indexCount==y.indexCount&&x.firstVertex==y.firstVertex&&x.vertexCount==y.vertexCount,"save lensraw");}
    for(std::size_t i=0;i<a.joins.size();++i)Need(a.joins[i].boundary==b.joins[i].boundary&&a.joins[i].joint==b.joins[i].joint,"save joinraw");}
void Save(Mistwatch::Explorer& now){const auto bytes=Encode(now.model);const auto before=now;Mistwatch::SaveGame saved,restored;const auto text=saved.Serialize();Need(!text.empty()&&restored.Restore(text),"save actual format restore");
    CharacterController3D controller;Mistwatch::Boar boar;Mistwatch::OrbitCamera orbit{-.65f,.3f,6};const auto terrain=Mistwatch::CreateTerrain();
    std::vector<BoxCollider3D> solids=Mistwatch::WorldSolids(false);ThirdPersonCameraMotion3D motion;ThirdPersonFramingResult3D framing;ThirdPersonCameraMotionResult3D result;Camera3D rendered;
    auto prepared=Mistwatch::PrepareSavedGame(restored,{controller,now,boar,orbit},terrain);Need(bool(prepared)&&Encode(prepared.candidate->explorer.model)==bytes,"combined model lost in real load preparation");ArtSame(now.art,prepared.candidate->explorer.art);
    Mistwatch::SavedGameCommitTargets targets{saved.session,saved.encounter,saved.evasion,controller,now,boar,orbit,solids,motion,framing,result,rendered};
    Mistwatch::SwapPreparedSavedGame(*prepared.candidate,targets);Need(Encode(now.model)==bytes,"combined model lost on real load commit");ArtSame(before.art,now.art);
    Mistwatch::SwapPreparedSavedGame(*prepared.candidate,targets);Need(Encode(now.model)==bytes,"combined model lost on rollback");ArtSame(before.art,now.art);State(before,now);
    auto bad=restored;bad.session.feet.y=-1;const auto rejected=Mistwatch::PrepareSavedGame(bad,{controller,now,boar,orbit},terrain);Need(!rejected&&Encode(now.model)==bytes,"unsafe load mutated model");ArtSame(before.art,now.art);State(before,now);
}
}
void Write(const std::filesystem::path& path,const std::vector<std::byte>& bytes){Budget();Need(!std::filesystem::exists(path),"new output cannot overwrite");std::ofstream out(path,std::ios::binary);Need(bool(out),"new artifact open");out.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));out.close();Need(bool(out),"new artifact complete write");}
void Text(const std::filesystem::path& path,const std::string& value){std::vector<std::byte> bytes(value.size());for(std::size_t i=0;i<value.size();++i)bytes[i]=std::byte(static_cast<unsigned char>(value[i]));Write(path,bytes);}
void Tick(void*){Budget();}
int wmain(int argc,wchar_t** argv){std::filesystem::path output;bool saved=false;std::string phase="fresh-artifacts";try{
    if(argc!=2)throw std::runtime_error("Usage: BackpackFitCpuTest.exe EXISTING_BUILD_ARTIFACT_ROOT");
    Budget();const std::filesystem::path parent=argv[1];
    Need(parent.is_absolute()&&std::filesystem::is_directory(parent),"existing absolute build artifact root required");
    const auto attr=GetFileAttributesW(parent.c_str());Need(attr!=INVALID_FILE_ATTRIBUTES&&(attr&FILE_ATTRIBUTE_REPARSE_POINT)==0,"ordinary output parent");
    const auto serial=std::chrono::steady_clock::now().time_since_epoch().count();
    output=parent/(L"backpack-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(serial));
    Need(!std::filesystem::exists(output),"new output cannot overwrite");
    Mistwatch::Explorer old;Mistwatch::ExplorerNapeHairBinding::Apply(old);const auto baseBytes=Encode(old.model);
    Need(baseBytes.size()==554724&&Hash(baseBytes)=="567A4CF59889C6E04446444A3A5C86889F3407931234F1702B852AA6FB89A1B7","actual current formal9225 whole canonical source");
    auto now=old;phase="one-fixed-source-convex-composition";const auto receipt=Mistwatch::BackpackFit1250::Apply(now,{&Tick,nullptr});State(old,now);
    Need(receipt.sourceRecords==9225&&receipt.records==9281&&receipt.sourceIndices==36792&&receipt.indices==37032&&receipt.appended==56,"finite receipt population, not author oracle");
    NonOwned(old.model,now.model);Ranges(old,now);const auto view=View(now.model);phase="independent-source-topology-normal-clearance-readback";
    Geometry(old.model,view);Normals(view);Inverse(old,now);Canonical(now.model);
    Need(std::filesystem::create_directory(output),"fresh output create failed");Write(output/L"current-nape.aeskin",baseBytes);const auto candidateBytes=Encode(now.model);Write(output/L"combined-backpack.aeskin",candidateBytes);saved=true;
    std::ostringstream initial;initial<<"{\"schema\":\"mistwatch.combined-backpack-wip.1250\",\"candidate_saved\":true,\"records\":9281,\"triangles\":12344,\"sha256\":\""<<Hash(candidateBytes)<<"\",\"geometry_qualified\":false,\"native\":false,\"art\":false,\"game\":false}";
    Text(output/L"saved-candidate-wip.json",initial.str());
    phase="original-ten-controls-plus-current-integration";
    auto bad=now.model;bad.bindMesh.vertices[0].x+=.01f;Reject([&]{NonOwned(old.model,bad);},"nonowned mutation accepted");
    bad=now.model;bad.bindMesh.indices[0]+=1;Reject([&]{NonOwned(old.model,bad);},"nonowned index mutation accepted");
    bad=now.model;bad.skeleton.joints[2].parent=0;Reject([&]{NonOwned(old.model,bad);},"rig mutation accepted");
    bad=now.model;bad.bindMesh.vertices[First].u=std::numeric_limits<float>::quiet_NaN();Reject([&]{Geometry(old.model,View(bad));},"owned NaN UV accepted");
    bad=now.model;bad.bindMesh.vertices[First].normal=0;Reject([&]{Topology(View(bad).bindMesh,First,First+98u,192);},"zero normal accepted");
    bad=now.model;for(std::size_t i=0;i<bad.bindMesh.indices.size();i+=3)if(CurrentOwned(bad.bindMesh.indices[i])){std::swap(bad.bindMesh.indices[i+1],bad.bindMesh.indices[i+2]);break;}
    Reject([&]{Topology(View(bad).bindMesh,First,First+98u,192);},"owned winding mutation accepted");
    bad=now.model;for(std::uint32_t i=First+98u;i<First+158u;++i)bad.bindMesh.vertices[Actual(i)].z+=.01f;Reject([&]{Geometry(old.model,View(bad));},"attachment support/clearance mutation accepted");
    bad=now.model;bad.bindMesh.vertices[6955].z+=.001f;Reject([&]{Containment(old.model,View(bad),false);},"retained real support mutation accepted");
    bad=now.model;bad.bindMesh.vertices[First+32u].x=.151f;Reject([&]{Containment(old.model,View(bad),false);},"stored point outside original-owned hull accepted");
    Reject([&]{Need(TetraCertificate(old.model.bindMesh,Precise(view.bindMesh.vertices[First+32u]),{First,First+1u,First+2u,First+3u},First+32u,false),"wrong-source certificate rejected");},"wrong original source IDs accepted");
    bad=now.model;bad.bindMesh.vertices[9000].u+=.1f;Reject([&]{NonOwned(old.model,bad);},"current short-hair raw mutation accepted");
    auto source=old;source.model.bindMesh.vertices[6931].u+=.1f;const auto rejectedBytes=Encode(source.model);Reject([&]{Mistwatch::BackpackFit1250::Apply(source,{&Tick,nullptr});},"wrong source162 UV accepted");Need(Encode(source.model)==rejectedBytes,"reject modified source model");
    source=now;Reject([&]{Mistwatch::BackpackFit1250::Apply(source,{&Tick,nullptr});},"double Apply accepted");Need(Encode(source.model)==candidateBytes,"double Apply reject altered candidate");
    auto badArt=now;badArt.art.eyeLensSpans[0].firstIndex-=240;Reject([&]{Ranges(old,badArt);},"lens offset mutation accepted");
    auto badInverse=now;badInverse.model.bindMesh.vertices[9000].color^=1u;Reject([&]{Inverse(old,badInverse);},"inverse concealed current nonowned drift");
    Containment(old.model,view,true);CertificateControls1250(old.model,view);ExactControls();phase="whole-new-owned-and-complete-protected-physical";Contacts(view);
    phase="all242-poses-and-save-commit-rollback";Poses(old,now);Save(now);
    Need(narrow==disjoint+allowed+forbidden+invalid,"everyexact call class accounting");
    std::ostringstream result;result<<"{\"schema\":\"mistwatch.combined-backpack-cpu.1250\",\"CPU_scope_passed\":true,\"candidate_saved\":true,\"records\":9281,\"triangles\":12344,\"checks\":"<<checks<<",\"negative_controls\":"<<negatives<<",\"narrow_pairs\":"<<narrow<<",\"native\":false,\"art\":false,\"game\":false,\"whole_character_contact_qualified\":false}";
    Text(output/L"CPU-receipt.json",result.str());std::cout<<"combined_backpack_CPU_scope=pass checks="<<checks<<" negatives="<<negatives<<" poses=242 narrow_pairs="<<narrow<<" exact_disjoint="<<disjoint<<" exact_allowed="<<allowed<<" exact_forbidden="<<forbidden<<" exact_invalid="<<invalid<<" original4_tests_separate=1 native=0 art=0 game=0 full_character_contact_qualified=0\n";return 0;
}catch(const std::exception& e){std::cerr<<"FIRST_DIVERGENCE phase="<<phase<<" reason="<<e.what()<<" checks="<<checks<<" negatives="<<negatives<<" pairs="<<narrow<<" candidate_saved="<<saved<<" native=0 art=0 game=0\n";
    if(!output.empty()&&std::filesystem::is_directory(output)&&!std::filesystem::exists(output/L"first-failure.txt")){try{Text(output/L"first-failure.txt","phase="+phase+"\nreason="+e.what()+"\ncandidate_saved="+(saved?"true":"false")+"\nnative=false\nart=false\ngame=false\n");}catch(...){}}
    return 1;}}
