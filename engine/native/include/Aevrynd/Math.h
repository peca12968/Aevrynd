#pragma once
#include <cmath>
#include <cstdint>

namespace Aevrynd {
struct Vec2 {
    float x{}, y{};
    constexpr Vec2 operator+(Vec2 b) const { return {x+b.x,y+b.y}; }
    constexpr Vec2 operator-(Vec2 b) const { return {x-b.x,y-b.y}; }
    constexpr Vec2 operator*(float s) const { return {x*s,y*s}; }
};
struct Color { float r{},g{},b{},a{1}; };
// 將線性色彩限制於有效範圍並打包成 GPU 可直接正規化的 RGBA8。
constexpr uint32_t PackColorRgba8(Color color){auto channel=[](float value){value=value<0.f?0.f:value>1.f?1.f:value;return uint32_t(value*255.f+.5f);};return channel(color.r)|(channel(color.g)<<8)|(channel(color.b)<<16)|(channel(color.a)<<24);}
inline float Length(Vec2 v){ return std::sqrt(v.x*v.x+v.y*v.y); }
inline float Distance(Vec2 a,Vec2 b){ return Length(a-b); }
inline Vec2 Normalize(Vec2 v){ float l=Length(v); return l>.0001f?v*(1.f/l):Vec2{}; }
inline float Clamp(float v,float a,float b){ return v<a?a:(v>b?b:v); }
}
