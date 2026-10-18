#include "MistwatchSurveyMap.h"
#include <bit>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
using namespace Mistwatch;using namespace Aevrynd;
namespace Map=Mistwatch::SurveyMap;
unsigned cases{},checks{};
void Need(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
template<class Fn>void Case(const char* name,Fn body) {
    try{body();++cases;}
    catch(const std::exception& e){std::cerr<<"SURVEY_MAP_CPU_FIRST_DIVERGENCE case="<<name<<" reason="<<e.what()<<'\n';throw;}
}
bool Same(Vec3 a,Vec3 b){return std::bit_cast<unsigned>(a.x)==std::bit_cast<unsigned>(b.x)&&
    std::bit_cast<unsigned>(a.y)==std::bit_cast<unsigned>(b.y)&&std::bit_cast<unsigned>(a.z)==std::bit_cast<unsigned>(b.z);}
bool Same(const Session& a,const Session& b) {
    return a.collected==b.collected&&a.health==b.health&&a.bridgeRepaired==b.bridgeRepaired&&
      a.guardianDefeated==b.guardianDefeated&&a.beaconLit==b.beaconLit&&a.fuseCollected==b.fuseCollected&&
      a.powerRestored==b.powerRestored&&a.circuit==b.circuit&&a.repairMaterials==b.repairMaterials&&
      a.fuseClueRead==b.fuseClueRead&&a.fuseDials==b.fuseDials&&a.relayLinked==b.relayLinked&&
      a.relaySectors==b.relaySectors&&a.relayPatchA==b.relayPatchA&&a.relayPatchB==b.relayPatchB&&
      a.relayFieldTasks==b.relayFieldTasks&&a.extractionRoute==b.extractionRoute&&Same(a.feet,b.feet);
}
bool Same(const Camera3D& a,const Camera3D& b) {
    return Same(a.position,b.position)&&Same(a.target,b.target)&&Same(a.up,b.up)&&
      a.projection.mode==b.projection.mode&&a.projection.verticalFieldOfView==b.projection.verticalFieldOfView&&
      a.projection.orthographicHeight==b.projection.orthographicHeight&&a.projection.nearPlane==b.projection.nearPlane&&a.projection.farPlane==b.projection.farPlane;
}
Session State(Stage stage) {
    Session s;
    if(stage>=Stage::RepairMaterials)s.collected=7;
    if(stage>=Stage::Bridge)s.repairMaterials=7;
    if(stage>=Stage::Encounter)s.bridgeRepaired=true;
    if(stage>=Stage::Fuse)s.guardianDefeated=true;
    if(stage>=Stage::Generator){s.fuseClueRead=true;s.fuseDials=Session::solvedFuseDials;s.fuseCollected=true;}
    if(stage>=Stage::Relays){s.circuit=7;s.powerRestored=true;}
    if(stage>=Stage::Beacon) {
        s.relayPatchA=1;s.extractionRoute=1;s.relayFieldTasks=FieldRelayTask::AllTasks;s.relayLinked=7;
        for(unsigned i=0;i<3;++i)s.relaySectors|=Session::RelayTargetSector(i)<<(i*4);
    }
    if(stage==Stage::Complete)s.beaconLit=true;
    Need(s.Valid()&&s.CurrentStage()==stage,"fixture invalid stage");return s;
}
Camera3D Camera(){Camera3D c;c.position={0,3,-6};c.target={0,1,0};return c;}
ObjectiveGuide Guide(const Session& s,const Camera3D& camera,const HeightField3D& terrain) {
    const Encounter enemy;const Evasion evasion;const std::vector<BoxCollider3D> solids;
    return BuildObjectiveGuide(s,enemy,evasion,camera,false,true,solids,terrain);
}
std::vector<float> Heights(const HeightField3D& terrain) {
    std::vector<float> h;const auto& s=terrain.Settings();
    for(unsigned z=0;z<s.depth;++z)for(unsigned x=0;x<s.width;++x)h.push_back(terrain.Height(x,z));return h;
}
std::string Text(const Map::Layout& layout){std::string t;for(unsigned i=0;i<layout.count;++i)t+=layout.lines[i].text+'\n';return t;}
void Inventory(const Map::Data& d,Map::Kind kind,const std::vector<unsigned>& indices) {
    Need(d.valid&&d.markerCount==indices.size()+1,"stage inventory count");
    Need(d.markers[0].kind==Map::Kind::Bridge&&Same(d.markers[0].world,Session::bridge),"known bridge absent");
    for(unsigned i=0;i<indices.size();++i)Need(d.markers[i+1].kind==kind&&d.markers[i+1].index==indices[i],"stage inventory kind or index");
}
}
int main() {
  try {
    HeightField3D terrain({{-50,0,-20},201,241,.5f});
    for(unsigned z=0;z<241;++z)for(unsigned x=0;x<201;++x)Need(terrain.SetHeight(x,z,float(x)*.02f+float(z)*.03f),"terrain fixture");
    terrain.RecalculateRange();const auto camera=Camera();
    Case("visibility-all-16",[&]{for(unsigned mask=0;mask<16;++mask)
        Need(Map::ShouldShow(bool(mask&1),bool(mask&2),bool(mask&4),bool(mask&8))==(mask==7),"visibility changed controls or combat");});
    Case("all-nine-stage-inventory-and-source-immutability",[&]{
      for(unsigned i=0;i<9;++i) {
        const auto s=State(static_cast<Stage>(i)),before=s;const auto wire=s.Serialize();
        const auto c=camera,cb=c;const auto h=Heights(terrain);const auto settings=terrain.Settings();
        const auto guide=Guide(s,c,terrain);Need(guide.valid,"real guide invalid");
        const auto d=Map::Build(s,guide,c,terrain);
        Need(d.valid&&d.tileCount==750&&d.stage==s.CurrentStage(),"stage map missing");
        Need(Same(s,before)&&s.Serialize()==wire&&Same(c,cb)&&Heights(terrain)==h,"read-only source mutation");
        Need(terrain.Settings().width==settings.width&&terrain.Settings().depth==settings.depth&&
             terrain.Settings().cellSize==settings.cellSize&&Same(terrain.Settings().origin,settings.origin),"terrain settings mutation");
        Need(d.markers[0].completed==s.bridgeRepaired,"bridge completion drift");
        switch(s.CurrentStage()) {
        case Stage::Supplies:Inventory(d,Map::Kind::Supply,{0,1,2});for(unsigned n=0;n<3;++n)Need(Same(d.markers[n+1].world,Session::supplies[n]),"supply position");break;
        case Stage::RepairMaterials:Inventory(d,Map::Kind::Material,{0,1,2});for(unsigned n=0;n<3;++n)Need(Same(d.markers[n+1].world,Session::repairSites[n]),"material position");break;
        case Stage::Bridge:case Stage::Complete:Need(d.markerCount==1,"extra terminal/bridge marker");break;
        case Stage::Encounter:Inventory(d,Map::Kind::Encounter,{0});Need(Same(d.markers[1].world,guide.target),"dynamic enemy target replaced by arena");break;
        case Stage::Fuse:Inventory(d,Map::Kind::Journal,{0});Need(Same(d.markers[1].world,Session::rangerLog),"unread log replaced by cabinet");break;
        case Stage::Generator:Inventory(d,Map::Kind::Generator,{0});Need(Same(d.markers[1].world,Session::generator),"generator position");break;
        case Stage::Relays:Inventory(d,Map::Kind::Relay,{0,1,2});for(unsigned n=0;n<3;++n)Need(Same(d.markers[n+1].world,Session::relaySites[n]),"relay position");break;
        case Stage::Beacon:Inventory(d,Map::Kind::Beacon,{0});Need(Same(d.markers[1].world,Session::beacon),"beacon position");break;
        }
        DrawList draw;const auto layout=Map::MeasureLayout(draw,d);const auto text=Text(layout);
        Need(text.find("地形示意，不能判斷可通行")!=std::string::npos&&text.find("2 - 0 - 3")==std::string::npos&&
             text.find("sector")==std::string::npos&&text.find("100%") ==std::string::npos,"map answer/safety leak");
        for(unsigned n=0;n<layout.count;++n) {
          const auto& line=layout.lines[n];const auto measured=draw.MeasureSystemText(line.text,line.pixels);
          Need(line.pixels>=18.2f&&measured.x==line.size.x&&measured.y==line.size.y&&measured.x>0&&measured.y>0,
               "real Chinese font measurement missing");
          Need(line.position.x>=348&&line.position.x+measured.x<=1260&&line.position.y>=176&&line.position.y+measured.y<=552,"text containment");
        }
        const auto receipt=Map::Draw(draw,d);
        Need(receipt.drawn&&receipt.tiles==750&&receipt.markers==d.markerCount&&receipt.textLines==layout.count&&
             !draw.vertices.empty()&&draw.Stats2D().textGlyphs>0,"2D map did not submit real glyphs");
        Need(draw.worldVertices.empty()&&draw.worldIndices.empty()&&draw.meshInstances.empty()&&
             draw.skinnedMeshInstances.empty()&&draw.skinPaletteJoints.empty(),"map submitted 3D or skinning");
      }
    });
    Case("unfinished-items-and-unread-secrets",[&]{
      Session s;s.collected=2;auto d=Map::Build(s,Guide(s,camera,terrain),camera,terrain);Inventory(d,Map::Kind::Supply,{0,2});
      s=State(Stage::RepairMaterials);s.repairMaterials=5;d=Map::Build(s,Guide(s,camera,terrain),camera,terrain);Inventory(d,Map::Kind::Material,{1});
      s=State(Stage::Fuse);s.fuseDials=63;auto g=Guide(s,camera,terrain);g.context="2 - 0 - 3 secret sector 12";
      d=Map::Build(s,g,camera,terrain);Inventory(d,Map::Kind::Journal,{0});DrawList draw;
      Need(Text(Map::MeasureLayout(draw,d)).find("secret")==std::string::npos,"guide context leaked");
      s.fuseClueRead=true;d=Map::Build(s,Guide(s,camera,terrain),camera,terrain);Inventory(d,Map::Kind::Fuse,{0});
      Need(Same(d.markers[1].world,Session::fuse),"read clue cabinet missing");
      s=State(Stage::Relays);s.relayLinked=2;s.relaySectors=Session::RelayTargetSector(1)<<4;Need(s.Valid(),"linked fixture");
      d=Map::Build(s,Guide(s,camera,terrain),camera,terrain);Inventory(d,Map::Kind::Relay,{0,2});
      const auto before=Text(Map::MeasureLayout(draw,d));s.relaySectors|=3;Need(s.Valid(),"unsolved angle fixture");
      const auto after=Map::Build(s,Guide(s,camera,terrain),camera,terrain);
      Need(Text(Map::MeasureLayout(draw,after))==before,"antenna angle/answer leaked into map");
    });
    Case("750-current-height-samples-not-canonical-art",[&]{
      Session s;auto g=Guide(s,camera,terrain);const auto d=Map::Build(s,g,camera,terrain);
      for(unsigned row=0;row<30;++row)for(unsigned col=0;col<25;++col) {
        const auto& t=d.tiles[row*25+col];
        const unsigned sx=unsigned((double(col)+.5)/25*200),sz=unsigned((29.5-double(row))/30*240);
        Need(t.sourceX==sx&&t.sourceZ==sz&&t.height==terrain.Height(sx,sz),"not actual current grid height");
      }
      auto changed=terrain;const auto& t=d.tiles[0];Need(changed.SetHeight(t.sourceX,t.sourceZ,t.height+17),"change fixture");
      const auto next=Map::Build(s,g,camera,changed);
      Need(next.valid&&next.tiles[0].height==t.height+17&&d.tiles[0].height==terrain.Height(t.sourceX,t.sourceZ),"canonical copy ignored caller terrain");
    });
    Case("projection-north-negative-nonsquare-boundaries",[&]{
      Session s;const auto d=Map::Build(s,Guide(s,camera,terrain),camera,terrain);Vec2 p{};
      Need(d.bounds.minX==-50&&d.bounds.minZ==-20&&d.bounds.maxX==50&&d.bounds.maxZ==100,"settings extents drift");
      Need(Map::Project(d.bounds,{-50,0,100},p)&&p.x==0&&p.y==0,"northwest corner");
      Need(Map::Project(d.bounds,{50,0,-20},p)&&p.x==1&&p.y==1,"southeast corner");
      Need(Map::Project(d.bounds,{0,0,40},p)&&p.x==.5f&&p.y==.5f,"actual world center");
      Need(std::abs(d.plot.width/d.plot.height-100.f/120.f)<.00001f,"nonsquare world stretched");
      Need(!Map::Project(d.bounds,{-50.01f,0,0},p)&&!Map::Project(d.bounds,{0,0,100.01f},p),"outside location clamped");
      const double huge=(std::numeric_limits<double>::max)();
      Need(!Map::Project({-huge,-1,huge,1},{},p),"overflow projection span");
      auto edge=s;edge.feet={-50,0,-20};Need(Map::Build(edge,Guide(edge,camera,terrain),camera,terrain).valid,"negative boundary rejected");
      edge.feet.x=-50.001f;Need(!Map::Build(edge,Guide(edge,camera,terrain),camera,terrain).valid,"outside player faked");
      for(const auto direction:{Vec3{0,0,1},Vec3{1,0,0},Vec3{0,0,-1},Vec3{-1,0,0}}) {
        auto c=camera;c.target=c.position+direction;const auto map=Map::Build(s,Guide(s,c,terrain),c,terrain);
        Need(map.valid&&map.cameraForward.x==direction.x&&map.cameraForward.y==-direction.z,"camera heading is not rendered forward");
      }
      HeightField3D alternate({{-70,0,-40},71,91,2});alternate.Fill(3);
      const auto other=Map::Build(s,Guide(s,camera,alternate),camera,alternate);
      Need(other.valid&&other.bounds.minX==-70&&other.bounds.maxX==70&&other.bounds.maxZ==140&&
           std::abs(other.plot.width/other.plot.height-140.f/180.f)<.00001f,"alternate real settings ignored");
    });
    Case("invalid-inputs-and-empty-draw",[&]{
      const float nan=(std::numeric_limits<float>::quiet_NaN)(),inf=(std::numeric_limits<float>::infinity)();
      Session s;const auto g=Guide(s,camera,terrain);
      auto invalid=s;invalid.health=0;Need(!Map::Build(invalid,g,camera,terrain).valid,"dead map");
      invalid=s;invalid.feet.x=nan;Need(!Map::Build(invalid,g,camera,terrain).valid,"NaN player");
      auto badGuide=g;badGuide.valid=false;Need(!Map::Build(s,badGuide,camera,terrain).valid,"invalid guide");
      badGuide=g;badGuide.hasTarget=false;Need(!Map::Build(s,badGuide,camera,terrain).valid,"missing objective");
      badGuide=g;badGuide.target.z=inf;Need(!Map::Build(s,badGuide,camera,terrain).valid,"nonfinite target");
      badGuide=g;badGuide.target.x=500;Need(!Map::Build(s,badGuide,camera,terrain).valid,"out of map target");
      for(unsigned variant=0;variant<4;++variant){auto c=camera;
        if(variant==0)c.position.x=nan;if(variant==1)c.target=c.position;if(variant==2)c.up.y=inf;if(variant==3)c.projection.nearPlane=nan;
        Need(!Map::Build(s,g,c,terrain).valid,"invalid camera accepted");}
      auto badHeight=terrain;const auto good=Map::Build(s,g,camera,terrain);const auto t=good.tiles[0];
      Need(badHeight.SetHeight(t.sourceX,t.sourceZ,nan),"bad height fixture");
      Need(!Map::Build(s,g,camera,badHeight).valid,"NaN current sample");
      HeightField3D badOrigin({{inf,0,0},2,2,1});Need(!Map::Build(s,g,camera,badOrigin).valid,"infinite settings");
      HeightField3D overflow({{0,0,0},3,3,(std::numeric_limits<float>::max)()});
      Need(!Map::Build(s,g,camera,overflow).valid,"float extent overflow");
      DrawList draw;Need(!Map::Draw(draw,{}).drawn&&draw.vertices.empty()&&draw.worldVertices.empty(),"invalid data submitted geometry");
    });
    Case("forged-data-rejected-before-any-submission",[&]{
      Session s;const auto good=Map::Build(s,Guide(s,camera,terrain),camera,terrain);
      const float nan=(std::numeric_limits<float>::quiet_NaN)();
      for(unsigned variant=0;variant<12;++variant) {
        auto d=good;
        if(variant==0)d.player.x=nan;
        if(variant==1)d.player.y=1.01f;
        if(variant==2)d.cameraForward.x=nan;
        if(variant==3)d.cameraForward={0,0};
        if(variant==4)d.markerCount=Map::MarkerBudget+1;
        if(variant==5)d.markers[0].unit.x=nan;
        if(variant==6)d.markers[0].unit.y=-.01f;
        if(variant==7)d.markers[0].kind=static_cast<Map::Kind>(99);
        if(variant==8)d.markers[0].index=1;
        if(variant==9)d.markers[1].index=3;
        if(variant==10)d.markers[0].world.z=nan;
        if(variant==11)d.playerWorld.x=nan;
        DrawList draw;draw.Quad({2,2},{1,1},{1,1,1,1});
        std::vector<unsigned char> before(draw.vertices.size()*sizeof(Vertex));
        std::memcpy(before.data(),draw.vertices.data(),before.size());const auto stats=draw.Stats2D();
        bool rejected=false;try{(void)Map::Draw(draw,d);}catch(const std::runtime_error&){rejected=true;}
        Need(rejected,"forged public Data reached draw");
        Need(draw.vertices.size()*sizeof(Vertex)==before.size()&&std::memcmp(before.data(),draw.vertices.data(),before.size())==0&&
             draw.Stats2D().submittedTriangles==stats.submittedTriangles&&draw.Stats2D().textGlyphs==stats.textGlyphs&&
             draw.worldVertices.empty()&&draw.worldIndices.empty()&&draw.meshInstances.empty()&&draw.skinnedMeshInstances.empty(),
             "rejection modified DrawList");
      }
    });
    std::cout<<"SURVEY_MAP_CPU_PASS cases="<<cases<<" checks="<<checks<<" tile_budget=750 GPU=false\n";return 0;
  }catch(const std::exception&){return 1;}
}
