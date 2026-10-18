#include "../src/CharacterController3D.cpp"
#include <iostream>
#include <stdexcept>

using namespace Aevrynd;
unsigned checks{};
void Need(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
std::vector<BoxCollider3D> Boxes(unsigned count,bool nearby=false){
    std::vector<BoxCollider3D> boxes;boxes.reserve(count);
    for(unsigned i=0;i<count;++i)boxes.push_back({{nearby?2.f:100.f+float(i),1,0},{.2f,.5f,.2f}});
    return boxes;
}
bool Identity(std::span<const BoxCollider3D> a,std::span<const BoxCollider3D> b){return a.data()==b.data()&&a.size()==b.size();}
// 同一份生產控制器在本測試 TU 編譯，以直接驗證私有窗口與巢狀生命期。
int main()try{
    const Vec3 e{.5f,1,.5f},c{0,1,0};
    auto boxes=Boxes(80);boxes[0]={{2,1,0},{.2f,.5f,.2f}};
    const std::span<const BoxCollider3D> all=boxes;
    Need(activeStepQueryWindow==nullptr,"INITIAL_TLS_EMPTY");
    {
        StepQueryWindowScope scope;scope.Prepare(all,{},e,1.f/60);
        const auto selected=StepQueryBoxes(c,e,{},all);
        Need(selected.size()==1&&!Identity(selected,all),"ONE_NEAR_BOX_SELECTED");
        Need(selected[0].center.x==2,"SOURCE_ORDER_AND_VALUE");
        Need(!Identity(StepQueryBoxes({4,1,0},e,{},all),all),"INNER_EQUAL_ENDPOINT_RETAINED");
        Need(Identity(StepQueryBoxes({std::nextafter(4.f,5.f),1,0},e,{},all),all),"INNER_NEXT_FLOAT_OUT_FALLBACK");
        Need(!Identity(StepQueryBoxes(c,e,{4,0,0},all),all),"END_EQUAL_BOUNDARY_RETAINED");
        Need(Identity(StepQueryBoxes(c,e,{std::nextafter(4.f,5.f),0,0},all),all),"END_OUTSIDE_FALLBACK");
        auto different=e;different.x=std::nextafter(e.x,1.f);
        Need(Identity(StepQueryBoxes(c,different,{},all),all),"EXTENT_BITS_FALLBACK");
        auto copied=boxes;Need(Identity(StepQueryBoxes(c,e,{},copied),copied),"DIFFERENT_SPAN_SAME_VALUES_FALLBACK");
        const auto shorter=all.first(70);Need(Identity(StepQueryBoxes(c,e,{},shorter),shorter),"SAME_POINTER_DIFFERENT_SIZE_FALLBACK");
        Need(Identity(StepQueryBoxes(c,e,{std::numeric_limits<float>::infinity(),0,0},all),all),"NONFINITE_DISPLACEMENT_FALLBACK");
        Need(Identity(StepQueryBoxes({std::numeric_limits<float>::quiet_NaN(),1,0},e,{},all),all),"NONFINITE_CENTER_FALLBACK");
        Need(!Identity(StepQueryBoxes({0,1.e30f,0},e,{},all),all),"NO_Y_CULLING_OR_QUERY_Y_CAP");
        {
            StepQueryWindowScope nested;
            Need(Identity(StepQueryBoxes(c,e,{},all),all),"UNPREPARED_NESTED_MASKS_PARENT");
            nested.Prepare(all.first(3),{},e,1.f/60);
            Need(Identity(StepQueryBoxes(c,e,{},all),all),"INAPPLICABLE_NESTED_CANNOT_BORROW_PARENT");
        }
        Need(!Identity(StepQueryBoxes(c,e,{},all),all),"PARENT_RESTORED_AFTER_NESTED");
    }
    Need(activeStepQueryWindow==nullptr,"OUTERMOST_SCOPE_RESTORES_TLS");
    for(unsigned count:{65u,256u,257u}){
        auto dense=Boxes(count,true);StepQueryWindowScope scope;scope.Prepare(dense,{},e,1.f/60);
        const auto result=StepQueryBoxes(c,e,{},dense);
        Need(result.size()==count,"DENSE_COUNT_NOT_TRUNCATED");
        Need(Identity(result,dense)==(count>256),"DENSE_CAPACITY_OR_OVERFLOW_FULL_FALLBACK");
    }
    {
        auto distant=Boxes(65);StepQueryWindowScope scope;scope.Prepare(distant,{},e,1.f/60);
        const auto result=StepQueryBoxes(c,e,{},distant);Need(result.empty()&&!Identity(result,distant),"ZERO_LOCAL_CANDIDATES_ARE_LEGAL");
    }
    const auto unchangedPointer=boxes.data();
    boxes[1]={{3,1,0},{.2f,.5f,.2f}};
    {
        StepQueryWindowScope scope;scope.Prepare(all,{},e,1.f/60);
        Need(boxes.data()==unchangedPointer&&StepQueryBoxes(c,e,{},all).size()==2,"SAME_ADDRESS_WORLD_REVISION_REBUILT");
    }
    for(float dt:{0.f,-.01f,std::numeric_limits<float>::quiet_NaN()}){
        StepQueryWindowScope scope;scope.Prepare(all,{},e,dt);Need(Identity(StepQueryBoxes(c,e,{},all),all),"INVALID_OR_ZERO_DT_NO_WINDOW");
    }
    for(float x:{100000.f,-100000.f,std::nextafter(100000.f,std::numeric_limits<float>::infinity())}){
        auto remote=Boxes(80);for(auto& box:remote)box.center.x+=x;
        const Vec3 origin{x,0,0};StepQueryWindowScope scope;scope.Prepare(remote,origin,e,1.f/60);
        const bool shouldPrepare=std::abs(x)<=100000.f;
        Need(Identity(StepQueryBoxes({x,1,0},e,{},remote),remote)!=shouldPrepare,"COORDINATE_APPLICABILITY_BOUNDARY");
    }
    {
        const float tiny=std::numeric_limits<float>::denorm_min();auto tinyWorld=Boxes(80);
        tinyWorld[0]={{tiny*2,0,0},{0,1,1}};const Vec3 tinyExtents{tiny,.5f,tiny};
        StepQueryWindowScope scope;scope.Prepare(tinyWorld,{-1,0,0},tinyExtents,1.f/60);
        const auto selected=StepQueryBoxes({-1,0,0},tinyExtents,{1,0,0},tinyWorld);
        Need(selected.size()==1,"DENORMAL_ROUNDING_COUNTEREXAMPLE_BOX_RETAINED");
        const auto original=SweepAabb({-1,0,0},tinyExtents,{1,0,0},tinyWorld[0]);
        const auto candidate=SweepAabb({-1,0,0},tinyExtents,{1,0,0},selected[0]);
        Need(original.fraction==1&&original.normalMask==1&&original.fraction==candidate.fraction&&original.normalMask==candidate.normalMask,"ORIGINAL_SLAB_DENORMAL_RESULT_PRESERVED");
    }
    {
        // 外 scope 列表在新 Step 前已失效；新入口必 mask，完整驗證不能借舊列表吞掉遠端非法尾盒。
        auto world=Boxes(80);world[0]={{2,1,0},{.2f,.5f,.2f}};
        StepQueryWindowScope outer;outer.Prepare(world,{},e,1.f/60);
        Need(StepQueryBoxes(c,e,{},world).size()==1,"OUTER_FIXTURE_PREPARED");
        world.back().extents.x=std::numeric_limits<float>::quiet_NaN();
        CharacterControllerSettings3D settings;settings.radius=.5f;settings.height=2;settings.maximumTerrainStandingGrade=.7f;
        CharacterController3D controller(settings);controller.Reset();const auto before=controller.StateHash();
        HeightField3D terrain;terrain.Fill(0);
        Need(!controller.Step({{1,0,0}},world,{},terrain,1.f/60),"NESTED_STEP_FULL_ENTRY_REJECTS_INVALID_FAR_BOX");
        Need(controller.StateHash()==before,"FAILED_NESTED_STEP_STATE_UNCHANGED");
    }
    Need(activeStepQueryWindow==nullptr,"FINAL_TLS_EMPTY");
    Need(bool(std::cout),"STDOUT_STREAM_READY");
    std::cout<<"QUERY_SELECTOR_COMPLETE checks="<<checks<<'\n';return 0;
}catch(const std::exception& error){std::cerr<<"FIRST_QUERY_SELECTOR_DIVERGENCE="<<error.what()<<" checks="<<checks<<'\n';return 1;}
