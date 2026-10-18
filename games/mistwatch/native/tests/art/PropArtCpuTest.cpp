#include "MistwatchGeneratorDetail.h"
#include "MistwatchRangerHut.h"
// 歷史木屋專屬命名空間；現行世界、發電機與遊戲來源仍使用正式版本。
namespace OriginalHutAsset {using namespace Mistwatch;}
#define Mistwatch OriginalHutAsset
#include "fixtures/hut-before-whole/MistwatchRangerHut.h"
#undef Mistwatch
namespace OldHut=OriginalHutAsset;
#include "MistwatchSession.h"
#include <Aevrynd/Scene3D.h>
#include <algorithm>
#include <bit>
#include <cctype>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>

std::vector<Aevrynd::BoxCollider3D> FormalPropWorldSnapshot(bool,Mistwatch::FirLayoutSelector);
namespace {
using namespace Aevrynd;
std::size_t checks{};
void Check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
bool Same(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
bool Same(Vec3 a,Vec3 b){return Same(a.x,b.x)&&Same(a.y,b.y)&&Same(a.z,b.z);}
bool Same(Color a,Color b){return Same(a.r,b.r)&&Same(a.g,b.g)&&Same(a.b,b.b)&&Same(a.a,b.a);}
double DotDouble(Vec3 a,Vec3 b){return double(a.x)*b.x+double(a.y)*b.y+double(a.z)*b.z;}
Vec3 Position(const Vertex3D& v){return {v.x,v.y,v.z};}
std::string Read(const std::string& path){std::ifstream in(path,std::ios::binary);Check(bool(in),"Source audit file missing");return {std::istreambuf_iterator<char>{in},std::istreambuf_iterator<char>{}};}
// 只擷取發電機擁有的完整陳述與區塊，不把相鄰新增資源清理納入歷史逐字比較。
// 這是有界來源契約，不冒稱 C++ 編譯器或 GPU 生命週期驗證。
using Tokens=std::vector<std::string>;
Tokens Tokenize(const std::string& source){
    Tokens result;
    for(std::size_t i=0;i<source.size();){const auto c=static_cast<unsigned char>(source[i]);
        if(std::isspace(c)){++i;continue;}
        if(source.compare(i,2,"//")==0){const auto end=source.find('\n',i+2);i=end==std::string::npos?source.size():end+1;continue;}
        if(source.compare(i,2,"/*")==0){const auto end=source.find("*/",i+2);if(end==std::string::npos)throw std::runtime_error("Unclosed source comment");i=end+2;continue;}
        const auto begin=i;
        if(source[i]=='\"'||source[i]=='\''){
            const char quote=source[i++];bool closed=false;
            while(i<source.size()){if(source[i]=='\\'){i+=2;continue;}if(source[i++]==quote){closed=true;break;}}
            if(!closed||i>source.size())throw std::runtime_error("Unclosed source literal");
        }else if(std::isalnum(c)||source[i]=='_'){
            while(i<source.size()&&(std::isalnum(static_cast<unsigned char>(source[i]))||source[i]=='_'))++i;
        }else ++i;
        result.push_back(source.substr(begin,i-begin));
    }
    return result;
}
std::size_t Unique(const Tokens& tokens,const std::string& marker){
    const auto needle=Tokenize(marker);if(needle.empty())throw std::runtime_error("Empty lifecycle marker");
    std::size_t found=tokens.size();
    for(std::size_t i=0;i+needle.size()<=tokens.size();++i)
        if(std::equal(needle.begin(),needle.end(),tokens.begin()+static_cast<std::ptrdiff_t>(i))){
            if(found!=tokens.size())throw std::runtime_error("Duplicate lifecycle marker");found=i;
        }
    if(found==tokens.size())throw std::runtime_error("Missing lifecycle marker");return found;
}
std::size_t BalancedEnd(const Tokens& tokens,std::size_t first,const char* open,const char* close){
    if(first>=tokens.size()||tokens[first]!=open)throw std::runtime_error("Lifecycle block opener missing");
    unsigned depth=0;
    for(std::size_t i=first;i<tokens.size();++i){if(tokens[i]==open)++depth;else if(tokens[i]==close&&--depth==0)return i+1;}
    throw std::runtime_error("Unclosed lifecycle block");
}
std::size_t StatementEnd(const Tokens& tokens,std::size_t first){
    for(std::size_t i=first;i<tokens.size();){
        if(tokens[i]==";")return i+1;
        if(tokens[i]=="(")i=BalancedEnd(tokens,i,"(",")");
        else if(tokens[i]=="{")i=BalancedEnd(tokens,i,"{","}");
        else if(tokens[i]=="[")i=BalancedEnd(tokens,i,"[","]");
        else ++i;
    }
    throw std::runtime_error("Unterminated lifecycle statement");
}
std::size_t ControlledBlockEnd(const Tokens& tokens,std::size_t first,const char* keyword){
    if(first>=tokens.size()||tokens[first]!=keyword)throw std::runtime_error("Lifecycle control missing");
    return BalancedEnd(tokens,BalancedEnd(tokens,first+1,"(",")"),"{","}");
}
Tokens Range(const Tokens& tokens,std::size_t first,std::size_t end){
    if(first>=end||end>tokens.size())throw std::runtime_error("Invalid lifecycle range");
    return {tokens.begin()+static_cast<std::ptrdiff_t>(first),tokens.begin()+static_cast<std::ptrdiff_t>(end)};
}
struct GeneratorLifecycle {
    Tokens upload,destroy,receipt;
    std::array<std::size_t,4> stopReferences{};
};
GeneratorLifecycle Lifecycle(const std::string& source){
    const auto tokens=Tokenize(source);
    GeneratorLifecycle out;
    const auto upload=Unique(tokens,"const auto generatorDetail=");auto end=StatementEnd(tokens,upload);
    if(end>=tokens.size()||tokens[end]!="try")throw std::runtime_error("Generator upload try missing");
    end=BalancedEnd(tokens,end+1,"{","}");end=ControlledBlockEnd(tokens,end,"catch");
    out.upload=Range(tokens,upload,end);
    const auto stop=Unique(tokens,"void OnStop(Engine& e) override {");
    const auto open=stop+Tokenize("void OnStop(Engine& e) override").size();
    const auto body=Range(tokens,open+1,BalancedEnd(tokens,open,"{","}")-1);
    const auto destroy=Unique(body,"unsigned generatorDestroyed");
    end=ControlledBlockEnd(body,StatementEnd(body,destroy),"for");out.destroy=Range(body,destroy,end);
    const auto receipt=Unique(body,"const auto generatorCleared=");
    end=StatementEnd(body,receipt);end=StatementEnd(body,end);
    end=ControlledBlockEnd(body,end,"if");
    if(end>=body.size()||body[end]!="if")throw std::runtime_error("Generator final failure gate missing");
    const auto gateBody=BalancedEnd(body,end+1,"(",")");
    if(gateBody>=body.size()||body[gateBody]!="throw")throw std::runtime_error("Generator final failure not propagated");
    end=StatementEnd(body,gateBody);out.receipt=Range(body,receipt,end);
    constexpr std::array identifiers{"generatorDetailMeshes","generatorDestroyed","generatorCleared","generatorCleanupOk"};
    for(std::size_t i=0;i<identifiers.size();++i)
        out.stopReferences[i]=static_cast<std::size_t>(std::count(body.begin(),body.end(),identifiers[i]));
    return out;
}
bool SameLifecycle(const GeneratorLifecycle& expected,const std::string& source){
    try{const auto actual=Lifecycle(source);return expected.upload==actual.upload&&expected.destroy==actual.destroy&&
        expected.receipt==actual.receipt&&expected.stopReferences==actual.stopReferences;
    }catch(const std::exception&){return false;}
}
std::string ReplaceOnce(std::string source,const std::string& before,const std::string& after){
    const auto at=source.find(before);Check(at!=std::string::npos&&source.find(before,at+before.size())==std::string::npos,"Mutation marker not unique");
    source.replace(at,before.size(),after);return source;
}
unsigned LifecycleNegatives(const GeneratorLifecycle& expected,const std::string& candidate){
    unsigned negatives=0;
    const auto reject=[&](const std::string& before,const std::string& after,const char* reason){
        ++negatives;Check(!SameLifecycle(expected,ReplaceOnce(candidate,before,after)),reason);
    };
    const std::string loop="for(auto& mesh:generatorDetailMeshes){if(mesh){e.DestroyMesh3D(mesh.handle);++generatorDestroyed;}mesh={};}";
    reject(loop,"for(auto& mesh:generatorDetailMeshes){if(mesh){++generatorDestroyed;}mesh={};}","Missing actual destroy rejected");
    reject(loop,"for(auto& mesh:generatorDetailMeshes){if(mesh){e.DestroyMesh3D(mesh.handle);e.DestroyMesh3D(mesh.handle);++generatorDestroyed;}mesh={};}","Duplicate actual destroy rejected");
    reject(loop,"for(auto& mesh:generatorDetailMeshes){if(mesh){e.DestroyMesh3D(box.handle);++generatorDestroyed;}mesh={};}","Wrong owned handle rejected");
    reject(loop,"for(auto& mesh:generatorDetailMeshes){if(mesh){e.DestroyMesh3D(mesh.handle);++generatorDestroyed;}}","Missing handle clear rejected");
    reject(loop,loop+"\ne.DestroyMesh3D(generatorDetailMeshes[0].handle);","Duplicate outside owned loop rejected");
    reject(loop,loop+"\n"+loop,"Duplicate owned loop rejected");
    reject("generatorDestroyed==Mistwatch::GeneratorDetailGroups&&","generatorDestroyed<=Mistwatch::GeneratorDetailGroups&&","Weakened destroy count rejected");
    reject("generatorCleared==Mistwatch::GeneratorDetailGroups;","true;","Ignored cleared resource failure rejected");
    reject("if(!generatorCleanupOk)throw std::runtime_error(\"Generator detail cleanup incomplete\");","if(!generatorCleanupOk){}","Swallowed cleanup failure rejected");
    reject("if(!out)throw std::runtime_error(\"Generator cleanup receipt failed\");","if(!out){}","Ignored receipt write failure rejected");
    reject("(generatorCleanupOk?\"success\":\"failed\")","(\"success\")","False successful receipt rejected");
    reject("for(auto& mesh:generatorDetailMeshes){if(mesh)e.DestroyMesh3D(mesh.handle);mesh={};}",
        "for(auto& mesh:generatorDetailMeshes){mesh={};}","Missing failed upload cleanup rejected");
    reject("if(!generatorDetailMeshes[group])throw std::runtime_error(\"Generator detail GPU upload failed\");",
        "if(!generatorDetailMeshes[group]){}","Ignored upload failure rejected");
    Check(SameLifecycle(expected,ReplaceOnce(candidate,loop,loop+
        "\nunsigned unrelatedDestroyed{};for(auto& mesh:unrelatedMeshes){if(mesh){e.DestroyMesh3D(mesh.handle);++unrelatedDestroyed;}mesh={};}\n")),
        "Adjacent unrelated cleanup must not contaminate generator contract");
    Check(SameLifecycle(expected,ReplaceOnce(candidate,loop,"/* } generatorDestroyed generatorDetailMeshes */\n"+loop)),
        "Comments must not become executable lifecycle evidence");
    return negatives;
}
void WorldEqual(const std::vector<BoxCollider3D>& a,const std::vector<BoxCollider3D>& b){Check(a.size()==b.size(),"Formal WorldSolids count");for(std::size_t i=0;i<a.size();++i)Check(Same(a[i].center,b[i].center)&&Same(a[i].extents,b[i].extents),"Formal WorldSolids exact centre/extents");}
void MeshChecks(const Mesh3D& mesh,Vec3 minimum,Vec3 maximum){
    Check(mesh.Valid(),"Real Mesh3D validity");
    for(const auto& v:mesh.vertices){Check(std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z)&&std::isfinite(v.u)&&std::isfinite(v.v),"Real finite vertex/UV");Check(v.x>=minimum.x&&v.y>=minimum.y&&v.z>=minimum.z&&v.x<=maximum.x&&v.y<=maximum.y&&v.z<=maximum.z,"Real mesh bounds");}
    for(std::size_t i=0;i<mesh.indices.size();i+=3){const auto& va=mesh.vertices.at(mesh.indices.at(i));const auto& vb=mesh.vertices.at(mesh.indices.at(i+1));const auto& vc=mesh.vertices.at(mesh.indices.at(i+2));const auto a=Position(va),b=Position(vb),c=Position(vc);
        // double 精度獨立叉積；不使用作者自己的 Triangle 或 Normalize 判斷自己。
        const double ux=double(b.x)-a.x,uy=double(b.y)-a.y,uz=double(b.z)-a.z,vx=double(c.x)-a.x,vy=double(c.y)-a.y,vz=double(c.z)-a.z;
        const double nx=uy*vz-uz*vy,ny=uz*vx-ux*vz,nz=ux*vy-uy*vx,length=std::sqrt(nx*nx+ny*ny+nz*nz);
        Check(std::isfinite(length)&&length*.5>1.e-8,"Real nondegenerate triangle");
        for(const auto* v:{&va,&vb,&vc}){const auto unpacked=UnpackNormalSnorm8(v->normal);const Vec3 n{unpacked.x,unpacked.y,unpacked.z};const double normalLength=std::sqrt(DotDouble(n,n));Check(normalLength>.98&&normalLength<1.02,"Real packed normal length");Check((nx*n.x+ny*n.y+nz*n.z)/(length*normalLength)>.999,"Real outward winding normal agreement");}
    }
}
bool Hits(const Mesh3D& mesh,Vec3 from,Vec3 to){const auto direction=to-from;for(std::size_t i=0;i<mesh.indices.size();i+=3){const auto a=Position(mesh.vertices.at(mesh.indices.at(i))),b=Position(mesh.vertices.at(mesh.indices.at(i+1))),c=Position(mesh.vertices.at(mesh.indices.at(i+2)));const auto edge=b-a,other=c-a,h=Cross(direction,other);const float det=Dot(edge,h);if(std::abs(det)<.000001f)continue;const float inverse=1.f/det;const auto s=from-a;const float u=inverse*Dot(s,h);if(u<0||u>1)continue;const auto q=Cross(s,edge);const float v=inverse*Dot(direction,q),t=inverse*Dot(other,q);if(v>=0&&u+v<=1&&t>.0001f&&t<.9999f)return true;}return false;}
Mistwatch::Session SessionFor(unsigned circuit){Mistwatch::Session s;s.collected=7;s.repairMaterials=7;s.bridgeRepaired=true;s.guardianDefeated=true;s.fuseClueRead=true;s.fuseDials=Mistwatch::Session::solvedFuseDials;s.fuseCollected=true;s.circuit=circuit;s.feet=Mistwatch::Session::generator+Vec3{-1.6f,0,0};Check(s.Valid(),"Actual Session fixture valid");return s;}
}
int main(){try{
    using namespace Aevrynd;
    constexpr std::array groupTriangles{std::size_t{376},std::size_t{192},std::size_t{660}};
    const auto geometry=Mistwatch::CreateGeneratorDetail(),repeat=Mistwatch::CreateGeneratorDetail();
    Check(geometry.meshes.size()==3&&Mistwatch::GeneratorDetailGroups==3,"Exactly original three mesh groups");Check(geometry.parts.size()==77,"Exact authored part count");
    std::size_t triangles{};std::array<std::size_t,3> coverage{};
    Scene3D scene;
    const auto materials=Mistwatch::GeneratorDetailMaterials();
    for(std::size_t group=0;group<3;++group){const auto& mesh=geometry.meshes[group];MeshChecks(mesh,{-.85f,0,-.6f},{.85f,1.2f,.6f});Check(mesh.indices.size()/3==groupTriangles[group],"Each real material mesh triangle budget");Check(mesh.indices==repeat.meshes[group].indices&&mesh.vertices.size()==repeat.meshes[group].vertices.size(),"Repeat actual topology");triangles+=mesh.indices.size()/3;
        for(std::size_t i=0;i<mesh.vertices.size();++i){const auto& a=mesh.vertices[i];const auto& b=repeat.meshes[group].vertices[i];Check(Same(Position(a),Position(b))&&a.normal==b.normal&&a.color==b.color&&Same(a.u,b.u)&&Same(a.v,b.v),"Repeat exact actual vertices");}
        const auto& material=materials[group];Check(material.surface.roughness>=.3f&&material.surface.roughness<=.8f&&material.surface.metallic>=0&&material.surface.metallic<=1&&material.surface.emissive==0&&material.surface.occlusion==1&&material.castsShadow&&material.tint.a==1&&!material.doubleSided&&!material.textures&&material.alphaMode==MaterialAlphaMode3D::Opaque,"Original bounded opaque real PBR material");
        const MeshResource3D descriptor{MeshHandle3D{static_cast<std::uint32_t>(group+1u)},mesh.Bounds(),mesh.vertices.size(),mesh.indices.size(),std::make_shared<const Mesh3D>(mesh)};Check(scene.Mesh(descriptor,{Mistwatch::Session::generator,{},{1,1,1}},material)!=0,"Three actual CPU Scene3D mesh submissions");
    }
    for(const auto& part:geometry.parts){Check(part.group<3&&part.firstIndex==coverage[part.group]&&part.indexCount%3==0&&part.indexCount>0,"Contiguous real part index coverage");coverage[part.group]+=part.indexCount;const auto& mesh=geometry.meshes[part.group];Check(coverage[part.group]<=mesh.indices.size(),"Real part ends in group");for(std::size_t i=part.firstIndex;i<coverage[part.group];i+=3){const auto a=Position(mesh.vertices.at(mesh.indices.at(i))),b=Position(mesh.vertices.at(mesh.indices.at(i+1))),c=Position(mesh.vertices.at(mesh.indices.at(i+2)));const auto normal=Cross(b-a,c-a),centroid=(a+b+c)*(1.f/3.f)-part.centre;Check(DotDouble(normal,centroid)/std::sqrt(DotDouble(normal,normal))>1.e-6,"Actual convex component outward winding");}}
    for(std::size_t group=0;group<3;++group)Check(coverage[group]==geometry.meshes[group].indices.size(),"All real indices covered");Check(triangles==1228&&triangles<=Mistwatch::GeneratorDetailTriangleBudget,"Exact real generator total and retained original budget");Check(scene.MeshCount()==3&&scene.TriangleCount()==1228,"Actual static Scene3D membership");
    std::size_t lensTriangles{};for(unsigned lamp=0;lamp<3;++lamp){const auto lens=Mistwatch::GeneratorIndicatorLens(lamp);Check(lens.size()==8,"Exactly eight actual lens triangles");const auto centre=Mistwatch::GeneratorIndicatorCentre(lamp);for(const auto& t:lens){for(const auto& p:t){Check(Mistwatch::Session::Finite(p)&&p.x>=-.85f&&p.x<=.85f&&p.y>=0&&p.y<=1.2f&&p.z>=-.6f&&p.z<=.6f,"Actual embedded lens collision envelope");Check(std::hypot(p.x-centre.x,p.y-centre.y)<=.022001f&&Same(p.z,centre.z),"Actual 4.4 centimetre lens diameter/plane");}const auto cross=Cross(t[1]-t[0],t[2]-t[0]);Check(Length(cross)>.0000001f&&cross.z<0&&std::abs(cross.x)<1.e-7f&&std::abs(cross.y)<1.e-7f,"Actual nondegenerate front-facing lens");scene.Triangle(Mistwatch::Session::generator+t[0],Mistwatch::Session::generator+t[1],Mistwatch::Session::generator+t[2],Mistwatch::GeneratorIndicatorColour(0,lamp));++lensTriangles;}}
    Check(lensTriangles==24&&scene.MeshCount()==3&&scene.TriangleCount()==1252,"Actual three meshes plus 24 state lens triangles");
    bool rejected=false;try{(void)Mistwatch::GeneratorIndicatorLens(3);}catch(const std::out_of_range&){rejected=true;}Check(rejected,"Invalid lens index rejected");
    for(unsigned circuit=0;circuit<8;++circuit){auto session=SessionFor(circuit);const auto saved=session.Serialize();for(unsigned lamp=0;lamp<3;++lamp){const Color expected=(circuit&(1u<<lamp))?Color{.2f,1,.3f,1}:Color{1,.15f,.1f,1};Check(Same(Mistwatch::GeneratorIndicatorColour(session.circuit,lamp),expected),"Actual 0..7 Session state maps to exact original lamp colour");}Check(session.Serialize()==saved,"Actual visual helper preserves Session save bytes");constexpr std::array masks{3u,6u,4u};for(unsigned lever=0;lever<3;++lever){auto changed=session;Check(changed.ToggleCircuit(lever)&&changed.circuit==(circuit^masks[lever]),"Actual existing XOR circuit rule");Check(changed.RepairGenerator()==(changed.circuit==7),"Actual original repair rule");}}
    const auto hut=OldHut::CreateRangerHut();MeshChecks(hut,{-2.5001f,-.00001f,-3.0001f},{2.5001f,3.8501f,3.0001f});Check(hut.indices.size()/3==1708&&hut.vertices.size()==3432,"Exact actual hut triangles and indexed vertex budget");
    for(const auto& v:hut.vertices){Check(!(v.x<-2.2999f&&v.y<2.5499f&&std::abs(v.z)<2.7999f),"Original west entrance vertex exclusion");Check(!(std::abs(v.z)>2.7999f&&v.y>.9001f&&v.y<2.1999f&&((v.x>-2.1499f&&v.x<-.1501f)||(v.x>.1501f&&v.x<2.1499f))),"Original four strict window vertex exclusions");}
    unsigned windowRays{},entryRays{};for(const float end:{-1.f,1.f})for(const float x:{-1.15f,1.15f})for(const float y:{1.1f,1.55f,2.f}){Check(!Hits(hut,{x,y,end*3.05f},{x,y,end*2.7f}),"Actual hut open window segment");++windowRays;}for(const float z:{-2.f,0.f,2.f})for(const float y:{.4f,1.3f,2.3f}){Check(!Hits(hut,{-2.6f,y,z},{-2.2f,y,z}),"Actual hut open west entry segment");++entryRays;}
    for(const bool repaired:{false,true})for(const auto selector:{Mistwatch::FirLayoutSelector::Legacy,Mistwatch::FirLayoutSelector::ForestWood})WorldEqual(FormalPropWorldSnapshot(repaired,selector),Mistwatch::WorldSolids(repaired,selector));
    const auto collider=Mistwatch::GeneratorSolid();Check(Same(collider.center,{7,.6f,65})&&Same(collider.extents,{.85f,.6f,.6f}),"Exact original generator interaction collider");
    const auto formal=Read(std::string(PROP_ART_FORMAL_ROOT)+"/games/mistwatch/native/src/main.cpp"),candidate=Read(std::string(PROP_ART_CANDIDATE_ROOT)+"/games/mistwatch/native/src/main.cpp");
    const auto lifecycle=Lifecycle(formal);
    Check(SameLifecycle(lifecycle,candidate),"Exact owned generator upload destroy receipt and propagated failure source");
    const auto lifecycleNegatives=LifecycleNegatives(lifecycle,candidate);
    Check(candidate.find("GeneratorIndicatorColour(session.circuit,lamp)")!=std::string::npos&&candidate.find("GeneratorIndicatorLens(lamp)")!=std::string::npos&&candidate.find("Vec3{-.5f+float(lamp)*.5f,1.3f,0}")==std::string::npos,"Actual main source uses tested lens/state helpers and removes huge boxes");
    std::cout<<"PROP_ART_CPU_PASS hut_scope=historical generator_world_scope=current checks="<<checks<<" generator_groups=3 parts=77 triangles=1228 lens_triangles="<<lensTriangles<<" hut_triangles=1708 hut_vertices=3432 window_rays="<<windowRays<<" entry_rays="<<entryRays<<" session_states=8 world_exact=1 cleanup_owned_tokens_exact=1 lifecycle_negative_controls="<<lifecycleNegatives<<" gpu_executed=0\n";return 0;
}catch(const std::exception& error){std::cerr<<"FIRST_DIVERGENCE checks="<<checks<<" error="<<error.what()<<'\n';return 1;}}
