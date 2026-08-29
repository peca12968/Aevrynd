#include <Aevrynd/WorldPartitionTerrainVegetationAdapter3D.h>
#include <Aevrynd/Scene3D.h>
#include <filesystem>
#include <iostream>
#include <windows.h>
using namespace Aevrynd;

class TerrainVegetationD3D12Game final:public IGame {
public:
 explicit TerrainVegetationD3D12Game(int& result):result_(result){}
 void OnStart(Engine& engine)override{
  TerrainVegetationChunkSettings3D settings{};settings.chunkSize=16;settings.terrainCells=8;settings.vegetationPerChunk=8;settings.vegetationLods={Mesh3D::CreateUnitCone(12),Mesh3D::CreateUnitCone(8),Mesh3D::CreateUnitPyramid()};settings_=settings;target_=std::make_unique<TerrainVegetationWorldChunkActivationTarget3D>(engine,scene_,vegetation_,settings_);
  for(std::uint64_t index=0;index<4;++index){auto payload=std::make_shared<TerrainVegetationChunkPayload3D>();HeightField3D field({{float(index%2)*16,0,float(index/2)*16},9,9,2});field.Generate(std::uint32_t(91+index),3,.03f,3);payload->terrain=field.CreateMesh();for(std::uint32_t item=0;item<8;++item){Transform3D transform{};transform.position={float(index%2)*16+2+float(item%4)*3,1,float(index/2)*16+2+float(item/4)*5};transform.scale={1,3,1};payload->vegetation.push_back({index*100+item+1,0,transform.Matrix(),{{0,.5f,0},2,{.5f,1.5f,.5f}},{.7f,1,.7f,1}});}payload->cpuBytes=payload->terrain.vertices.size()*sizeof(Vertex3D)+payload->terrain.indices.size()*sizeof(uint32_t)+payload->vegetation.size()*sizeof(VegetationInstance3D);WorldChunkPayload3D opaque{payload,payload->cpuBytes,payload->cpuBytes};const auto estimate=target_->PhysicalAllocationBytes({{int(index%2),int(index/2)},std::uint32_t(index),1,index+1,1},opaque);auto activation=target_->Activate({{int(index%2),int(index/2)},std::uint32_t(index),1,index+1,1},opaque);if(!activation||activation.gpuBytes>estimate){result_=2;engine.Quit();return;}activations_.push_back(activation);}
  const auto stats=engine.GetRendererStats();if(vegetation_.Stats().chunks!=4||stats.meshUploads<7){result_=8;engine.Quit();return;}capture_=std::filesystem::temp_directory_path()/"aevrynd-world-partition-phase2.png";std::error_code error;std::filesystem::remove(capture_,error);
 }
 void OnUpdate(Engine& engine,float)override{
  if(result_)return;if(stage_==0&&++frames_>3){if(!engine.CaptureNextFrame3D(capture_)){result_=3;engine.Quit();return;}stage_=1;}
  else if(stage_==1&&++frames_>7){engine.WaitRendererIdle3D();if(!std::filesystem::exists(capture_)||!std::filesystem::file_size(capture_)||engine.GetRendererStats().lastSubmittedMeshInstances<5){result_=4;engine.Quit();return;}target_->BeginRetire(activations_[0]);if(vegetation_.Stats().chunks!=3){result_=5;engine.Quit();return;}stage_=2;}
  else if(stage_==2){engine.WaitRendererIdle3D();if(!target_->IsRetirementComplete(activations_[0]))return;for(std::size_t i=1;i<activations_.size();++i)target_->BeginRetire(activations_[i]);target_->WaitIdle();if(vegetation_.Stats().chunks||target_->PendingRetirements()||engine.GetRendererStats().pendingGpuReleaseBytes){result_=6;engine.Quit();return;}engine.Quit();}
 }
 void OnRender(Engine& engine,float)override{Scene3D scene;scene_.Collect(scene,engine.Draw());Camera3D camera{{16,18,-24},{16,0,16},{0,1,0},{}};scene.Submit(engine.Draw(),camera,{float(engine.Width()),float(engine.Height())});VegetationView3D view{camera,float(engine.Width())/float(engine.Height()),200,true,true};for(const auto& draw:vegetation_.Query(view))engine.Draw().MeshInstance(draw.mesh,draw.transform,draw.tint,{},MeshRenderLayer3D::Opaque,{},.25f,false,MaterialClass3D::Foliage);if(++renderFrames_>180){result_=7;engine.Quit();}}
 void OnStop(Engine& engine)override{target_.reset();engine.WaitRendererIdle3D();std::error_code error;std::filesystem::remove(capture_,error);}
private:int& result_;TerrainVegetationChunkSettings3D settings_{};SceneECSRegistry3D scene_{};ChunkedVegetationWorld3D vegetation_{};std::unique_ptr<TerrainVegetationWorldChunkActivationTarget3D> target_{};std::vector<WorldChunkActivation3D> activations_{};std::filesystem::path capture_{};int stage_{},frames_{},renderFrames_{};
};
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,wchar_t*,int){int result{};try{EngineConfig config{};config.title=L"Aevrynd World Partition Phase2";config.width=320;config.height=180;config.renderQuality=RenderQuality3D::Low;config.enableAutomaticMemoryTrim=false;Engine engine(config);engine.Run(std::make_unique<TerrainVegetationD3D12Game>(result),instance,SW_HIDE);}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 9;}std::cout<<"result="<<result<<'\n';return result;}
