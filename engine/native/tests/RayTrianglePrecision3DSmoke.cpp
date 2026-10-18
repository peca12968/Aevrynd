#include <Aevrynd/Geometry3D.h>
#include <array>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <limits>
#include <stdexcept>
namespace {
using namespace Aevrynd;
using Triangle=std::array<Vec3,3>;
std::size_t checks{};
void Need(bool condition,const char* message){++checks;if(!condition)throw std::runtime_error(message);}
bool Near(float value,float expected,float tolerance=.000001f){return std::isfinite(value)&&std::abs(value-expected)<=tolerance;}
void Frame(const RayTriangleHit3D& hit,const Ray3D& ray){
    Need(bool(hit),"Expected true finite hit");
    Need(Near(Length(hit.normal),1.f,.000001f),"True hit must have a unit normal");
    Need(Dot(hit.normal,ray.direction)<=0.f,"Normal must face the input ray");
    Need(Near(hit.barycentric.x+hit.barycentric.y+hit.barycentric.z,1.f),"Barycentric sum");
    Need(hit.barycentric.x>=0.f&&hit.barycentric.y>=0.f&&hit.barycentric.z>=0.f,"Strict barycentric bounds");
}
}
int main(){
    try{
        // 固定有效薄斜面及其後方內襯，使用真失敗輸入的原 float 頂點；不挪ray或加邊界epsilon。
        const std::array<Triangle,3> layers{{
            {{{.0162113904953003f,3.8072988986969f,2.99699997901917f},{2.49621152877808f,2.95729899406433f,2.99699997901917f},{2.48324227333069f,2.9194598197937f,2.50300002098083f}}},
            {{{.0162113904953003f,3.8072988986969f,2.99699997901917f},{2.48324227333069f,2.9194598197937f,2.50300002098083f},{.00324225425720215f,3.76945972442627f,2.50300002098083f}}},
            {{{-.0110236406326294f,3.72783660888672f,-2.99399995803833f},{-.0110236406326294f,3.72783660888672f,2.99399995803833f},{2.46897649765015f,2.87783670425415f,2.99399995803833f}}}
        }};
        const Vec3 from{1.2886340618133545f,3.4768970012664795f,2.75f};
        const Vec3 to{1.2270309925079346f,3.2971608638763428f,2.75f};
        const auto delta=to-from;
        std::size_t sharedCases{};
        for(const bool normalized:std::array{false,true})for(const bool cull:std::array{false,true}){
            const Ray3D ray=normalized?MakeRay3D(from,delta):Ray3D{from,delta};
            const float maximum=normalized?Length(delta):1.f;
            std::size_t nearest=layers.size();RayTriangleHit3D closest{};
            for(std::size_t i=0;i<layers.size();++i){
                const auto& t=layers[i];const auto hit=RayIntersectsTriangle(ray,t[0],t[1],t[2],maximum,cull);
                if(hit&&hit.distance<closest.distance){closest=hit;nearest=i;}
            }
            const float expected=normalized?.1200001108324361f:.6315794016432965f;
            if(nearest>=2||!Near(closest.distance,expected)){
                std::cerr<<"RAY_SHARED_EDGE_EXPECTED_SURFACE_MISS case="<<sharedCases<<" normalized="<<normalized<<" cull="<<cull<<" observed_distance="<<closest.distance<<'\n';
                return 21;
            }
            Frame(closest,ray);++sharedCases;
        }
        Need(sharedCases==4,"Four fixed shared-edge cases");
        const Triangle front{{{-1,-1,5},{0,1,5},{1,-1,5}}};
        const Ray3D ray{{0,0,0},{0,0,1}};
        auto query=[&](const Ray3D& input,float maximum=std::numeric_limits<float>::infinity(),bool cull=false){return RayIntersectsTriangle(input,front[0],front[1],front[2],maximum,cull);};
        const auto hit=query(ray);Frame(hit,ray);Need(Near(hit.distance,5.f),"Original front hit distance");
        Need(Near(hit.barycentric.x,.25f)&&Near(hit.barycentric.y,.5f)&&Near(hit.barycentric.z,.25f),"Original front barycentrics");
        Need(bool(query(ray,5.f))&&!query(ray,std::nextafter(5.f,0.f)),"Original inclusive maximum distance");
        Need(bool(query(ray,100.f,true))&&!RayIntersectsTriangle(ray,front[2],front[1],front[0],100.f,true),"Original backface culling");
        Need(!query({{0,0,6},{0,0,1}}),"Behind the origin cannot hit");
        Need(!query({{0,0,0},{1,0,0}})&&!query({{0,0,0},{0,0,0}}),"Parallel and zero rays reject");
        const auto endpoint=query({{0,0,5},{0,0,1}},0.f);Frame(endpoint,{{0,0,5},{0,0,1}});Need(endpoint.distance==0.f,"Origin on surface remains inclusive");
        const auto scaled=query({{0,0,0},{0,0,2}},2.5f);Frame(scaled,{{0,0,0},{0,0,2}});Need(Near(scaled.distance,2.5f),"Unnormalized parametric distance");
        Need(!query({{0,0,0},{0,0,2}},std::nextafter(2.5f,0.f)),"Unnormalized maximum distance");
        for(const Vec3 outside:std::array{Vec3{1.1f,-1,0},Vec3{0,1.1f,0},Vec3{-1.1f,-1,0}})
            Need(!query({outside,{0,0,1}}),"Outside barycentrics never gain tolerance");
        Need(!RayIntersectsTriangle(ray,{0,0,5},{1,0,5},{2,0,5})&&!RayIntersectsTriangle(ray,front[0],front[0],front[2]),"Degenerate triangles reject");
        const float gate=.000001f;
        for(const float height:std::array{std::nextafter(gate,0.f),gate,std::nextafter(gate,std::numeric_limits<float>::infinity())}){
            const Ray3D smallRay{{.25f,height*.25f,0},{0,0,1}};
            const auto small=RayIntersectsTriangle(smallRay,{0,0,1},{0,height,1},{1,0,1},2.f,true);
            Need(bool(small)==(height>gate),"Original determinant <= gate is unchanged");
            if(small)Frame(small,smallRay);
        }
        const Ray3D tinyRay{{.00025f,.0025f,0},{0,0,1}};
        const auto tiny=RayIntersectsTriangle(tinyRay,{0,0,1},{0,.01f,1},{.001f,0,1});
        Frame(tiny,tinyRay);Need(Near(tiny.distance,1.f),"Valid small-area face keeps a true normal");
        const auto large=RayIntersectsTriangle(ray,{-1.e30f,-1.e30f,1},{0,1.e30f,1},{1.e30f,-1.e30f,1});
        Frame(large,ray);Need(Near(large.distance,1.f),"Finite large edges do not overflow float subtraction/cross");
        const float nan=std::numeric_limits<float>::quiet_NaN(),inf=std::numeric_limits<float>::infinity();
        for(const float invalid:std::array{nan,inf,-inf}){
            Need(!query({{invalid,0,0},{0,0,1}}),"Nonfinite origin rejects");
            Need(!query({{0,0,0},{0,0,invalid}}),"Nonfinite direction rejects");
            Need(!RayIntersectsTriangle(ray,{invalid,-1,5},front[1],front[2]),"Nonfinite vertex A rejects");
            Need(!RayIntersectsTriangle(ray,front[0],{0,invalid,5},front[2]),"Nonfinite vertex B rejects");
            Need(!RayIntersectsTriangle(ray,front[0],front[1],{1,-1,invalid}),"Nonfinite vertex C rejects");
        }
        Need(!query(ray,nan)&&!query(ray,-1.f)&&!query(ray,-inf),"Invalid distance range rejects");
        Need(bool(query(ray,inf)),"Positive infinity remains valid unlimited range");
        const Camera3D camera=MakeFirstPersonCamera({0,0,0},0,0);
        const auto screenRay=ScreenPointToRay(camera,{1280,720},{640,360});
        const auto screenHit=query(screenRay);Frame(screenHit,screenRay);Need(Near(screenHit.distance,5.f),"Actual camera screen ray still works");
        std::cout<<"RAY_TRIANGLE_PRECISION_PASS shared_edge_cases="<<sharedCases<<" contract_checks="<<checks<<" original_epsilon_preserved=1 input_shift=0 watertight_claim=0\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<"RAY_TRIANGLE_PRECISION_FAILURE: "<<error.what()<<'\n';return 1;}
}
