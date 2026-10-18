#include <Aevrynd/Camera.h>
#include <Aevrynd/Camera2D.h>
#include <Aevrynd/Engine.h>
#include <Aevrynd/Geometry3D.h>
#include <Aevrynd/Input.h>
#include <Aevrynd/Viewport.h>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace Aevrynd;

constexpr bool Near(float left,float right,float tolerance=.001f){return left>=right-tolerance&&left<=right+tolerance;}
bool Finite(const Matrix4& matrix){for(const auto& row:matrix.values)for(float value:row)if(!std::isfinite(value))return false;return true;}

constexpr Matrix4 Identity=Matrix4::Identity();
static_assert(Identity[0][0]==1.f&&Identity[3][3]==1.f);
static_assert((Identity*Vec4{2,3,4,1}).x==2.f);
static_assert(Dot({1,2,3},{4,5,6})==32.f);
static_assert(Cross({1,0,0},{0,1,0}).z==1.f);

namespace LeftMouseDragContractDetail {
enum class EventType {Move,LeftPress,LeftRelease,RightPress,RightRelease,Consume,Begin,FocusLoss,FocusGain};
struct Event {EventType type;int x{},y{};bool inside{true};};
constexpr Event Move(int x,int y,bool inside=true){return {EventType::Move,x,y,inside};}
constexpr Event Action(EventType type){return {type};}
struct Reference {
    int x{},y{},rawX{},rawY{},dragX{},dragY{};
    bool focused{true},anchored{},inside{true},left{},right{},leftPressed{},rightPressed{};
};
// 從每個事件前綴重新解譯；整數座標精確相減，不讀Input狀態來產生expected。
// raw段只需前後在畫布內；drag段另須事件當時left=true，不能用batch最後的按鍵狀態倒推。
Reference Replay(const std::vector<Event>& events){
    Reference r;
    for(const auto& event:events){
        if(event.type==EventType::Consume){r.rawX=r.rawY=r.dragX=r.dragY=0;r.leftPressed=r.rightPressed=false;continue;}
        if(event.type==EventType::Begin)continue;
        if(event.type==EventType::FocusLoss||event.type==EventType::FocusGain){
            const bool next=event.type==EventType::FocusGain;
            if(next!=r.focused){
                r.focused=next;r.anchored=false;r.inside=false;r.left=r.right=r.leftPressed=r.rightPressed=false;
                r.rawX=r.rawY=r.dragX=r.dragY=0;
            }
            continue;
        }
        if(!r.focused)continue;
        if(event.type==EventType::Move){
            const bool segment=r.anchored&&r.inside&&event.inside;
            const int dx=event.x-r.x,dy=event.y-r.y;
            if(segment){r.rawX+=dx;r.rawY+=dy;if(r.left){r.dragX+=dx;r.dragY+=dy;}}
            r.x=event.x;r.y=event.y;r.inside=event.inside;r.anchored=true;
        }else if(event.type==EventType::LeftPress){if(!r.left)r.leftPressed=true;r.left=true;}
        else if(event.type==EventType::LeftRelease)r.left=false;
        else if(event.type==EventType::RightPress){if(!r.right)r.rightPressed=true;r.right=true;}
        else if(event.type==EventType::RightRelease)r.right=false;
    }
    return r;
}
void Apply(Input& input,const Event& event){
    switch(event.type){
    case EventType::Move:input.SetMouse({static_cast<float>(event.x),static_cast<float>(event.y)},event.inside);break;
    case EventType::LeftPress:input.SetMouseButton(true);break;
    case EventType::LeftRelease:input.SetMouseButton(false);break;
    case EventType::RightPress:input.SetRightMouseButton(true);break;
    case EventType::RightRelease:input.SetRightMouseButton(false);break;
    case EventType::Consume:input.ConsumePresses();break;
    case EventType::Begin:input.BeginFrame();break;
    case EventType::FocusLoss:input.SetFocused(false);break;
    case EventType::FocusGain:input.SetFocused(true);break;
    }
}
struct Counts {std::size_t scenarios{},prefixes{},assertions{};};
void Run(const char* name,const std::vector<Event>& events,Counts& counts){
    Input input;std::vector<Event> history;history.reserve(events.size());std::size_t index{};
    const auto need=[&](bool condition,const char* reason){
        ++counts.assertions;
        if(!condition)throw std::runtime_error(std::string(name)+" event="+std::to_string(index)+" "+reason);
    };
    const auto vector=[&](Vec2 actual,int x,int y,const char* reason){
        ++counts.assertions;
        if(actual.x!=static_cast<float>(x)||actual.y!=static_cast<float>(y))throw std::runtime_error(
            std::string(name)+" event="+std::to_string(index)+" "+reason+" actual="+std::to_string(actual.x)+","+
            std::to_string(actual.y)+" expected="+std::to_string(x)+","+std::to_string(y));
    };
    for(const auto& event:events){
        Apply(input,event);history.push_back(event);const auto expected=Replay(history);
        vector(input.LeftMouseDragDelta(),expected.dragX,expected.dragY,"left drag differs from event-time oracle");
        vector(input.MouseDelta(),expected.rawX,expected.rawY,"raw mouse delta semantics changed");
        vector(input.Mouse(),expected.x,expected.y,"mouse anchor position changed");
        need(input.MouseInsideCanvas()==expected.inside,"inside-canvas state changed");
        need(input.MouseDown()==expected.left,"left held state changed");
        need(input.MousePressed()==expected.leftPressed,"left press edge changed");
        need(input.RightMousePressed()==expected.rightPressed,"right press edge changed");
        need(input.Focused()==expected.focused,"focus state changed");
        ++counts.prefixes;++index;
    }
    ++counts.scenarios;
}
}
bool LeftMouseDragDeltaContract(){
    using namespace LeftMouseDragContractDetail;
    try{
        Counts counts;std::size_t named{},bounded{};
        const auto run=[&](const char* name,const std::vector<Event>& events){Run(name,events,counts);++named;};
        const auto press=Action(EventType::LeftPress),release=Action(EventType::LeftRelease),
            rightPress=Action(EventType::RightPress),rightRelease=Action(EventType::RightRelease),
            consume=Action(EventType::Consume),begin=Action(EventType::Begin),
            lose=Action(EventType::FocusLoss),gain=Action(EventType::FocusGain);
        run("move-before-press-and-real-same-batch-drag",{Move(10,10),Move(16,7),press,Move(21,11),release,Move(50,20)});
        run("release-preserves-final-held-segment",{Move(20,30),press,Move(15,38),Move(8,31),release,Move(-20,50),begin});
        run("multiple-press-release-in-one-batch",{Move(0,0),Move(10,-5),press,Move(13,-9),release,Move(20,20),
            press,Move(15,27),release,Move(-30,-20),press,Move(-37,-16),release});
        run("zero-update-begin-retains-unconsumed-drag",{Move(5,5),press,Move(12,-3),begin,begin,Move(9,-8),begin,
            consume,begin,begin,Move(4,-1),begin,release,begin,consume,begin});
        run("focus-loss-and-regain-first-point",{Move(10,10),press,Move(14,6),lose,Move(1000,-1000),press,rightPress,begin,
            gain,Move(100,100),Move(103,95),press,Move(101,91),gain,Move(98,93),release});
        run("letterbox-exit-reentry-with-left-held",{Move(10,10),press,Move(13,7),Move(30,-20,false),Move(40,-30,false),
            Move(50,50),Move(45,57),release,Move(60,60),consume,Move(70,70,false),Move(80,80),Move(82,77)});
        run("right-button-is-not-left-drag",{Move(0,0),rightPress,Move(12,-3),rightRelease,Move(14,0),begin,consume});
        run("left-and-right-overlap-independent",{Move(0,0),rightPress,press,Move(5,-7),release,Move(12,-9),
            press,Move(8,-3),rightRelease,Move(2,1),release});
        run("press-before-first-mouse-anchor",{press,Move(100,100),Move(102,97),release,Move(110,110),consume});
        run("press-outside-then-reenter",{Move(-10,-10,false),press,Move(-20,-30,false),Move(10,10),Move(14,6),
            Move(50,-20,false),release,Move(60,60),Move(62,65)});
        run("consume-keeps-held-but-clears-only-accumulation",{Move(2,4),press,Move(10,1),consume,begin,Move(7,-6),
            consume,consume,Move(12,-1),release,Move(30,40),begin});
        run("button-only-taps-never-replay-old-motion",{Move(4,4),Move(12,-3),press,release,press,release,rightPress,
            rightRelease,begin,consume,press,release,Move(20,20)});
        run("redundant-focus-state-is-noop",{Move(0,0),press,Move(4,-6),gain,Move(3,-2),lose,lose,Move(99,99),
            gain,gain,Move(-10,10),Move(-7,8),press,Move(-9,12)});
        // 11種事件、全部長度4序列，每例先有有效點；固定上界與帶符號整數避免隨機漏測或float容差放行。
        const std::array<Event,11> alphabet{{Move(4,-3),Move(-5,7),Move(20,-11,false),press,release,
            rightPress,rightRelease,consume,begin,lose,gain}};
        constexpr std::size_t Depth=4;
        std::size_t combinations=1;for(std::size_t i=0;i<Depth;++i)combinations*=alphabet.size();
        for(std::size_t sequence=0;sequence<combinations;++sequence){
            std::vector<Event> events;events.reserve(Depth+1);events.push_back(Move(0,0));
            auto code=sequence;for(std::size_t i=0;i<Depth;++i){events.push_back(alphabet[code%alphabet.size()]);code/=alphabet.size();}
            const auto label="bounded-"+std::to_string(sequence);Run(label.c_str(),events,counts);++bounded;
        }
        if(named!=13||bounded!=14641||counts.scenarios!=named+bounded||counts.assertions!=counts.prefixes*8)
            throw std::runtime_error("left drag contract case accounting differs");
        std::cout<<"LEFT_MOUSE_DRAG_DELTA_CPU_PASS named_sequences="<<named<<" bounded_sequences="<<bounded
            <<" event_prefixes="<<counts.prefixes<<" exact_assertions="<<counts.assertions
            <<" raw_delta_unchanged=true GPU=false\n";return true;
    }catch(const std::exception& error){std::cerr<<"LEFT_MOUSE_DRAG_DELTA_FIRST_DIVERGENCE "<<error.what()<<'\n';return false;}
}

bool LeftMouseButtonAtContract(){
    std::string active="setup";std::size_t cases{},checkpoints{},assertions{};
    try{
        const auto expect=[&](const Input& input,Vec2 drag,Vec2 raw,Vec2 position,bool inside,bool down,bool pressed,bool focused=true){
            const auto need=[&](bool value,const char* reason){++assertions;if(!value)throw std::runtime_error(reason);};
            const auto same=[](Vec2 a,Vec2 b){return a.x==b.x&&a.y==b.y;};
            need(same(input.LeftMouseDragDelta(),drag),"button-position drag differs");
            need(same(input.MouseDelta(),raw),"button-position raw delta differs");
            need(same(input.Mouse(),position),"button-position anchor differs");
            need(input.MouseInsideCanvas()==inside,"button-position canvas state differs");
            need(input.MouseDown()==down,"button-position held state differs");
            need(input.MousePressed()==pressed,"button-position press edge differs");
            need(input.Focused()==focused,"button-position focus differs");
            need(!input.RightMousePressed(),"left button-position event changed right button");++checkpoints;
        };
        // 按下座標先校正raw anchor，再切換左鍵狀態；不能把cached100到down200算成drag。
        {
            active="cached100-down200-move210-release220";Input input;input.SetMouse({100,100});
            expect(input,{},{},{100,100},true,false,false);
            input.SetMouseButtonAt({200,200},true);expect(input,{},{100,100},{200,200},true,true,true);
            input.SetMouse({210,210});expect(input,{10,10},{110,110},{210,210},true,true,true);
            input.SetMouseButtonAt({220,220},false);expect(input,{20,20},{120,120},{220,220},true,false,true);
            input.SetMouse({230,230});expect(input,{20,20},{130,130},{230,230},true,false,true);++cases;
        }
        {
            active="actual-outside-press-cached-inside";Input input;input.SetMouse({100,100});
            input.SetMouseButtonAt({10,20},true,false);expect(input,{},{},{10,20},false,false,false);
            input.SetMouse({200,200});expect(input,{},{},{200,200},true,false,false);
            input.SetMouse({205,197});expect(input,{},{5,-3},{205,197},true,false,false);++cases;
        }
        {
            active="actual-inside-press-cached-outside";Input input;input.SetMouse({500,500},false);
            input.SetMouseButtonAt({200,200},true,true);expect(input,{},{},{200,200},true,true,true);
            input.SetMouse({190,210});expect(input,{-10,10},{-10,10},{190,210},true,true,true);
            input.SetMouseButtonAt({180,220},false);expect(input,{-20,20},{-20,20},{180,220},true,false,true);++cases;
        }
        {
            active="outside-release-still-clears-held-state";Input input;input.SetMouse({100,100});
            input.SetMouseButtonAt({100,100},true);input.SetMouse({110,105});
            expect(input,{10,5},{10,5},{110,105},true,true,true);
            input.SetMouseButtonAt({200,200},false,false);expect(input,{10,5},{10,5},{200,200},false,false,true);
            input.SetMouse({300,300});expect(input,{10,5},{10,5},{300,300},true,false,true);
            input.SetMouse({303,298});expect(input,{10,5},{13,3},{303,298},true,false,true);++cases;
        }
        {
            active="unfocused-button-position-events-ignored";Input input;input.SetMouse({10,10});
            input.SetMouseButtonAt({10,10},true);input.SetMouse({20,5});expect(input,{10,-5},{10,-5},{20,5},true,true,true);
            input.SetFocused(false);input.SetMouseButtonAt({100,100},true,true);
            expect(input,{},{},{20,5},false,false,false,false);
            input.SetMouseButtonAt({200,200},false,false);expect(input,{},{},{20,5},false,false,false,false);
            input.SetFocused(true);input.SetMouseButtonAt({300,300},true,true);expect(input,{},{},{300,300},true,true,true);
            input.SetMouse({310,297});expect(input,{10,-3},{10,-3},{310,297},true,true,true);++cases;
        }
        {
            active="same-point-button-events-never-invent-motion";Input input;input.SetMouse({100,100});input.SetMouse({105,98});
            input.SetMouseButtonAt({105,98},true);expect(input,{},{5,-2},{105,98},true,true,true);
            input.SetMouseButtonAt({105,98},false);expect(input,{},{5,-2},{105,98},true,false,true);
            input.SetMouseButtonAt({105,98},true);expect(input,{},{5,-2},{105,98},true,true,true);
            input.SetMouseButtonAt({105,98},true);expect(input,{},{5,-2},{105,98},true,true,true);++cases;
        }
        {
            active="same-batch-positioned-button-cycles-preserve-prior-drag";Input input;input.SetMouse({0,0});
            input.SetMouseButtonAt({10,-10},true);expect(input,{},{10,-10},{10,-10},true,true,true);
            input.SetMouse({15,-15});input.SetMouseButtonAt({20,-20},false);
            expect(input,{10,-10},{20,-20},{20,-20},true,false,true);
            input.SetMouseButtonAt({30,-30},true);expect(input,{10,-10},{30,-30},{30,-30},true,true,true);
            input.SetMouse({25,-25});input.SetMouseButtonAt({15,-15},false);
            expect(input,{-5,5},{15,-15},{15,-15},true,false,true);
            input.SetMouse({18,-18});expect(input,{-5,5},{18,-18},{18,-18},true,false,true);++cases;
        }
        {
            active="inside-release-from-cached-outside-and-consume";Input input;input.SetMouse({0,0});
            input.SetMouseButtonAt({0,0},true);input.SetMouse({5,-5});input.SetMouse({100,100},false);
            input.SetMouseButtonAt({10,-10},false,true);expect(input,{5,-5},{5,-5},{10,-10},true,false,true);
            input.SetMouse({12,-13});expect(input,{5,-5},{7,-8},{12,-13},true,false,true);
            input.ConsumePresses();input.SetMouseButtonAt({20,-20},true);
            expect(input,{},{8,-7},{20,-20},true,true,true);
            input.SetMouseButtonAt({25,-25},false);expect(input,{5,-5},{13,-12},{25,-25},true,false,true);++cases;
        }
        if(cases!=8||checkpoints!=33||assertions!=264)throw std::runtime_error("button-position case accounting differs");
        std::cout<<"LEFT_MOUSE_BUTTON_AT_CPU_PASS named_cases="<<cases<<" checkpoints="<<checkpoints
            <<" exact_assertions="<<assertions<<" original_drag_oracle_unchanged=true GPU=false\n";return true;
    }catch(const std::exception& error){std::cerr<<"LEFT_MOUSE_BUTTON_AT_FIRST_DIVERGENCE case="<<active
        <<" checkpoint="<<checkpoints<<" reason="<<error.what()<<'\n';return false;}
}


template<class T> void CancelLeftDragForContract(T& input) {
    if constexpr(requires{input.CancelLeftMouseDrag();})input.CancelLeftMouseDrag();
    else throw std::runtime_error("active-left-drag cancellation entrypoint missing");
}

bool LeftMouseCaptureCancellationContract(){
    std::string active="setup";std::size_t cases{},assertions{};
    try{
        const auto require=[&](bool value,const char* reason){++assertions;if(!value)throw std::runtime_error(reason);};
        const auto same=[](Vec2 a,Vec2 b){return a.x==b.x&&a.y==b.y;};
        {
            active="cancel-active-preserves-unrelated-input";Input input;
            input.SetKey('W',true);input.SetKey('E',true);input.SetKey('E',false);
            input.SetMouse({10,20});input.SetMouseButton(true);input.SetRightMouseButton(true);
            input.SetMouse({16,12});input.AddMouseWheel(2);
            const auto raw=input.MouseDelta(),point=input.Mouse();
            const bool connected=input.GamepadConnected();const float lx=input.GamepadLeftX(),rx=input.GamepadRightX();
            require(input.MouseDown()&&input.MousePressed()&&same(input.LeftMouseDragDelta(),{6,-8}),"active fixture missing");
            CancelLeftDragForContract(input);
            require(!input.MouseDown()&&!input.MousePressed()&&same(input.LeftMouseDragDelta(),{}),"cancel retains active left drag");
            require(input.Down('W')&&input.Pressed('W')&&!input.Down('E')&&input.Pressed('E'),"cancel changed keyboard");
            require(input.Focused()&&!input.FocusLost(),"cancel changed focus");
            require(input.RightMousePressed()&&input.MouseWheel()==2,"cancel changed right button or wheel");
            require(same(input.MouseDelta(),raw)&&same(input.Mouse(),point),"cancel changed raw delta or position");
            require(input.GamepadConnected()==connected&&input.GamepadLeftX()==lx&&input.GamepadRightX()==rx,"cancel changed gamepad");
            input.SetMouse({500,600});
            require(same(input.MouseDelta(),raw)&&same(input.LeftMouseDragDelta(),{}),"first post-cancel point created a jump");
            input.SetMouse({504,595});
            require(same(input.MouseDelta(),raw+Vec2{4,-5})&&same(input.LeftMouseDragDelta(),{}),"unpressed post-cancel move became drag");
            input.ConsumePresses();input.SetRightMouseButton(true);
            require(!input.RightMousePressed(),"cancel changed right-button held state");
            ++cases;
        }
        {
            active="repeated-cancel-does-not-reset-new-anchor";Input input;
            input.SetMouse({0,0});input.SetMouseButton(true);CancelLeftDragForContract(input);
            input.SetMouse({100,200});CancelLeftDragForContract(input);input.SetMouse({103,204});
            require(same(input.MouseDelta(),{3,4})&&same(input.LeftMouseDragDelta(),{}),"idle cancellation reset an established anchor");
            require(!input.MouseDown()&&!input.MousePressed(),"idle cancellation rearmed left mouse");++cases;
        }
        {
            active="fresh-left-down-after-cancel";Input input;
            input.SetMouse({10,10});input.SetMouseButton(true);CancelLeftDragForContract(input);
            input.SetMouse({100,100});input.SetMouseButtonAt({110,110},true);input.SetMouse({117,107});
            require(input.MouseDown()&&input.MousePressed(),"fresh press after cancel did not arm");
            require(same(input.LeftMouseDragDelta(),{7,-3}),"fresh drag includes cancelled movement");
            input.SetMouseButtonAt({120,105},false);
            require(!input.MouseDown()&&same(input.LeftMouseDragDelta(),{10,-5}),"fresh release tail missing");++cases;
        }
        {
            active="normal-up-capturechanged-preserves-tail";Input input;
            input.SetMouse({100,100});input.SetMouseButtonAt({200,200},true);
            input.SetMouse({210,210});input.SetMouseButtonAt({220,220},false);
            const auto raw=input.MouseDelta(),point=input.Mouse();
            require(!input.MouseDown()&&input.MousePressed()&&same(input.LeftMouseDragDelta(),{20,20}),"normal release fixture missing");
            CancelLeftDragForContract(input);CancelLeftDragForContract(input);
            require(input.MousePressed()&&same(input.LeftMouseDragDelta(),{20,20}),"capturechanged after normal up erased valid tail");
            require(same(input.MouseDelta(),raw)&&same(input.Mouse(),point),"idle cancel changed raw input");
            input.SetMouse({225,227});
            require(same(input.MouseDelta(),raw+Vec2{5,7})&&same(input.LeftMouseDragDelta(),{20,20}),"idle cancel reset normal-up anchor");++cases;
        }
        {
            active="unfocused-cancel-preserves-focus-loss-latch";Input input;
            input.SetMouse({1,2});input.SetMouseButton(true);input.SetFocused(false);
            CancelLeftDragForContract(input);
            require(!input.Focused()&&input.FocusLost()&&!input.MouseDown(),"cancel cleared focus-loss latch");
            input.SetFocused(true);input.SetMouse({700,800});
            require(input.Focused()&&input.FocusLost()&&same(input.LeftMouseDragDelta(),{}),"focus restore regained stale drag");++cases;
        }
        require(cases==5,"capture cancellation case accounting differs");
        std::cout<<"LEFT_MOUSE_CAPTURE_CANCELLATION_CPU_PASS cases="<<cases<<" assertions="<<assertions<<" GPU=false\n";return true;
    }catch(const std::exception& error){
        std::cerr<<"LEFT_MOUSE_CAPTURE_CANCELLATION_FIRST_DIVERGENCE case="<<active<<" assertions="<<assertions<<" reason="<<error.what()<<'\n';return false;
    }
}

int main(){
    if(!LeftMouseCaptureCancellationContract())return 35;
    if(!LeftMouseDragDeltaContract())return 33;
    if(!LeftMouseButtonAtContract())return 34;
    if(!CommandLineHasOption(L"--renderer dx12 --dlss",L"--dlss")||!CommandLineHasOption(L"--renderer \"--dlss\"",L"--dlss")||CommandLineHasOption(L"--renderer --dlss-disabled",L"--dlss")||CommandLineHasOption(L"--renderer dx12",L"--dlss")){std::cerr<<"Exact launch option parsing failed\n";return 30;}
    CameraRig2D rig2D;const Vec2 identityScreen=rig2D.WorldToScreen({100,50});const Bounds2D paddedBounds=rig2D.WorldBounds(100);if(!Near(identityScreen.x,100)||!Near(identityScreen.y,50)){std::cerr<<"Default 2D camera identity failed\n";return 26;}if(!Near(paddedBounds.minimum.x,-100)||!Near(paddedBounds.minimum.y,-100)||!Near(paddedBounds.maximum.x,1380)||!Near(paddedBounds.maximum.y,820)){std::cerr<<"Default 2D camera world bounds failed\n";return 31;}
    rig2D.SetCamera({{740,410},{1280,720},1.5f,.3f});const Vec2 worldPoint{812,477},screenPoint=rig2D.WorldToScreen(worldPoint),roundTrip=rig2D.ScreenToWorld(screenPoint),transformedPoint=TransformPoint(rig2D.DrawTransform(),worldPoint);if(!Near(roundTrip.x,worldPoint.x,.01f)||!Near(roundTrip.y,worldPoint.y,.01f)||!Near(transformedPoint.x,screenPoint.x,.01f)||!Near(transformedPoint.y,screenPoint.y,.01f)){std::cerr<<"2D camera coordinate round trip failed\n";return 27;}
    const Vec2 parallaxPoint=rig2D.WorldToScreen({640,360},.25f);if(parallaxPoint.x>=640||parallaxPoint.y>=360){std::cerr<<"2D camera parallax failed\n";return 28;}const Bounds2D rotatedBounds=rig2D.WorldBounds();for(const Vec2 screen:{Vec2{0,0},Vec2{1280,0},Vec2{1280,720},Vec2{0,720}}){const Vec2 corner=rig2D.ScreenToWorld(screen);if(corner.x<rotatedBounds.minimum.x-.01f||corner.x>rotatedBounds.maximum.x+.01f||corner.y<rotatedBounds.minimum.y-.01f||corner.y>rotatedBounds.maximum.y+.01f){std::cerr<<"Rotated 2D camera world bounds failed\n";return 32;}}
    rig2D.Reset({1280,720});for(int step=0;step<120;++step)rig2D.Update({740,410},5.f,6.1f,.1f);if(!Near(rig2D.Camera().position.x,740,.01f)||!Near(rig2D.Camera().position.y,410,.01f)||!Near(rig2D.Camera().zoom,2.f,.01f)||std::abs(rig2D.Camera().rotation)>.3f){std::cerr<<"2D camera smoothing or clamping failed\n";return 29;}
    const CanvasViewport wide=CalculateCanvasViewport({1280,720},{1024,768}),fit=CalculateCanvasViewport({1280,720},{853,480});
    if(!Near(wide.x,0)||!Near(wide.y,96)||!Near(wide.width,1024)||!Near(wide.height,576)||!Near(fit.x,0)||!Near(fit.y,.09375f)){std::cerr<<"Canvas letterbox calculation failed\n";return 21;}
    const Vec2 mapped=MapClientToCanvas({512,384},wide);if(!Near(mapped.x,640)||!Near(mapped.y,360)||!wide.Contains({512,384})||wide.Contains({512,32})){std::cerr<<"Canvas input mapping failed\n";return 22;}
    Input input;input.SetMouse({10,10});input.SetMouse({15,7});input.AddMouseWheel(2);
    if(!Near(input.MouseDelta().x,5)||!Near(input.MouseDelta().y,-3)||!Near(input.MouseWheel(),2)){std::cerr<<"Mouse motion accumulation failed\n";return 11;}input.SetMouse({20,20},false);input.SetMouse({30,30},true);if(!Near(input.MouseDelta().x,5)||input.MouseInsideCanvas()!=true){std::cerr<<"Letterbox mouse filtering failed\n";return 23;}input.ConsumePresses();if(!Near(input.MouseDelta().x,0)||!Near(input.MouseWheel(),0)){std::cerr<<"Mouse motion consumption failed\n";return 12;}
    CameraRig3D rig;rig.Reset(CameraViewMode3D::FirstPerson,{0,2,0});Camera3D controlled=rig.Update({{0,0,1},{1,0},0},1.f);if(rig.Mode()!=CameraViewMode3D::FirstPerson||!Near(Length(rig.Anchor()-Vec3{0,2,0}),1.4f)||!Near(rig.Yaw(),.18f)||controlled.projection.mode!=ProjectionMode::Perspective){std::cerr<<"First-person camera rig failed\n";return 13;}
    rig.Reset(CameraViewMode3D::ThirdPerson,{0,1,10});controlled=rig.Update({{}, {},1},.1f);if(!Near(rig.Distance(),36.2f)||!Near(Length(controlled.position-controlled.target),rig.Distance(),.01f)){std::cerr<<"Third-person camera rig failed\n";return 14;}
    rig.Reset(CameraViewMode3D::GodView,{5,0,7});controlled=rig.Update({{1,0,1},{1,0},1},.1f);if(controlled.projection.mode!=ProjectionMode::Orthographic||controlled.position.y<=controlled.target.y||!Finite(CreateViewProjectionMatrix(controlled,16.f/9.f))){std::cerr<<"God-view camera rig failed\n";return 15;}
    rig.Reset(CameraViewMode3D::Isometric,{0,0,0});controlled=rig.Update({{}, {-1,1},-1},.1f);if(controlled.projection.mode!=ProjectionMode::Orthographic||!Near(Length(controlled.position-controlled.target),rig.Distance(),.01f)||!Finite(CreateViewProjectionMatrix(controlled,16.f/9.f))){std::cerr<<"Isometric camera rig failed\n";return 16;}
    const Matrix4 model=CreateTransformMatrix({3,4,5},{.3f,.7f,.2f},{2,4,.5f}),normalMatrix=CreateNormalMatrix(model);
    const Vec3 localNormal=Normalize({1,1,0}),localTangent=Normalize({1,-1,0});
    const Vec3 worldNormal=TransformNormal(normalMatrix,localNormal),worldTangent=Normalize(TransformPoint(model,localTangent)-TransformPoint(model,{}));
    if(!Near(Length(worldNormal),1)||!Near(Dot(worldNormal,worldTangent),0,.002f)){std::cerr<<"Inverse-transpose normal transform failed\n";return 10;}
    Camera3D firstPerson=MakeFirstPersonCamera({0,0,0},0,0);
    const Ray3D centerRay=ScreenPointToRay(firstPerson,{1280,720},{640,360}),rightRay=ScreenPointToRay(firstPerson,{1280,720},{1280,360});if(!Near(centerRay.origin.z,0)||!Near(centerRay.direction.x,0)||!Near(centerRay.direction.z,1)||rightRay.direction.x<=0){std::cerr<<"Perspective screen ray failed\n";return 17;}
    float entryDistance{};if(!RayIntersectsAabb(centerRay,{0,0,5},{1,1,1},100,&entryDistance)||!Near(entryDistance,4)||RayIntersectsAabb(centerRay,{5,0,5},{1,1,1},100)){std::cerr<<"Ray AABB intersection failed\n";return 18;}const RayTriangleHit3D triangleHit=RayIntersectsTriangle(centerRay,{-1,-1,5},{0,1,5},{1,-1,5});if(!triangleHit||!Near(triangleHit.distance,5)||!Near(triangleHit.normal.z,-1)||!Near(triangleHit.barycentric.x+triangleHit.barycentric.y+triangleHit.barycentric.z,1)||!RayIntersectsTriangle(centerRay,{-1,-1,5},{0,1,5},{1,-1,5},100,true)||RayIntersectsTriangle(centerRay,{1,-1,5},{0,1,5},{-1,-1,5},100,true)){std::cerr<<"Ray triangle intersection failed\n";return 19;}
    ProjectedPoint center=ProjectToViewport({0,0,10},firstPerson,{1280,720});
    if(!center.visible||!Near(center.position.x,640)||!Near(center.position.y,360)||center.depth<=0||center.depth>=1){std::cerr<<"First-person projection failed\n";return 1;}
    if(ProjectToViewport({0,0,-10},firstPerson,{1280,720}).visible){std::cerr<<"Point behind camera must be hidden\n";return 2;}
    if(!Finite(CreateViewMatrix(MakeFirstPersonCamera({0,0,0},0,1.5707963f)))){std::cerr<<"Vertical first-person view must remain finite\n";return 7;}
    if(!SphereIntersectsFrustum({0,0,10},1,firstPerson,16.f/9.f)||SphereIntersectsFrustum({100,0,10},1,firstPerson,16.f/9.f)||SphereIntersectsFrustum({0,0,-10},1,firstPerson,16.f/9.f)){std::cerr<<"Perspective sphere frustum test failed\n";return 8;}
    if(!Near(ProjectedSphereRadiusPixels({0,0,10},1,firstPerson,{1280,720}),62.3538f,.01f)||ProjectedSphereRadiusPixels({0,0,-10},1,firstPerson,{1280,720})!=0){std::cerr<<"Perspective projected radius failed\n";return 24;}

    Camera3D thirdPerson=MakeThirdPersonCamera({0,1,0},0,0,10);
    if(!Near(thirdPerson.position.x,0)||!Near(thirdPerson.position.y,1)||!Near(thirdPerson.position.z,-10)){std::cerr<<"Third-person orbit failed\n";return 3;}
    if(!ProjectToViewport(thirdPerson.target,thirdPerson,{1280,720}).visible){std::cerr<<"Third-person target must be visible\n";return 4;}

    Camera3D overhead=MakeOverheadCamera({5,0,7},20,true);
    const Ray3D overheadCenterRay=ScreenPointToRay(overhead,{800,600},{400,300}),overheadCornerRay=ScreenPointToRay(overhead,{800,600},{800,0});if(!Near(overheadCenterRay.direction.y,-1)||!Near(overheadCenterRay.origin.x,5)||Near(overheadCornerRay.origin.x,5)||!Near(Dot(overheadCenterRay.direction,overheadCornerRay.direction),1)){std::cerr<<"Orthographic screen ray failed\n";return 20;}
    ProjectedPoint overheadCenter=ProjectToViewport(overhead.target,overhead,{800,600});
    if(overhead.projection.mode!=ProjectionMode::Orthographic||!overheadCenter.visible||!Near(overheadCenter.position.x,400)||!Near(overheadCenter.position.y,300)){std::cerr<<"Overhead projection failed\n";return 5;}
    if(!SphereIntersectsFrustum(overhead.target,1,overhead,4.f/3.f)||SphereIntersectsFrustum({100,0,7},1,overhead,4.f/3.f)){std::cerr<<"Orthographic sphere frustum test failed\n";return 9;}
    if(!Near(ProjectedSphereRadiusPixels(overhead.target,1,overhead,{800,600}),20)){std::cerr<<"Orthographic projected radius failed\n";return 25;}

    Camera3D isometric=MakeIsometricCamera({0,0,0},30,24);
    if(isometric.projection.mode!=ProjectionMode::Orthographic||!Near(Length(isometric.position-isometric.target),30,.01f)||!ProjectToViewport(isometric.target,isometric,{1280,720}).visible){std::cerr<<"Isometric projection failed\n";return 6;}
    return 0;
}
