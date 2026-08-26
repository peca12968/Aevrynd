#pragma once
namespace Rift {
class Engine;
class IGame {
public:
    virtual ~IGame()=default;
    virtual void OnStart(Engine&)=0;
    virtual void OnUpdate(Engine&,float fixedDelta)=0;
    virtual void OnRender(Engine&,float alpha)=0;
    virtual void OnStop(Engine&) {}
};
}
