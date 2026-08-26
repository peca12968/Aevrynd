#pragma once
#include <cmath>

namespace Rift {
struct Vec2 {
    float x{}, y{};
    Vec2 operator+(Vec2 b) const { return {x+b.x,y+b.y}; }
    Vec2 operator-(Vec2 b) const { return {x-b.x,y-b.y}; }
    Vec2 operator*(float s) const { return {x*s,y*s}; }
};
struct Color { float r{},g{},b{},a{1}; };
inline float Length(Vec2 v){ return std::sqrt(v.x*v.x+v.y*v.y); }
inline float Distance(Vec2 a,Vec2 b){ return Length(a-b); }
inline Vec2 Normalize(Vec2 v){ float l=Length(v); return l>.0001f?v*(1.f/l):Vec2{}; }
inline float Clamp(float v,float a,float b){ return v<a?a:(v>b?b:v); }
}
