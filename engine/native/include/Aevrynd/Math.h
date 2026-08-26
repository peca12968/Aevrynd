#pragma once
#include <cmath>
#include <cstdint>
#include <cstddef>

namespace Aevrynd {
struct Vec2 {
    float x{}, y{};
    constexpr Vec2 operator+(Vec2 b) const { return {x+b.x,y+b.y}; }
    constexpr Vec2 operator-(Vec2 b) const { return {x-b.x,y-b.y}; }
    constexpr Vec2 operator*(float s) const { return {x*s,y*s}; }
};
struct Vec3 {
    float x{},y{},z{};
    constexpr Vec3 operator+(Vec3 value)const{return{x+value.x,y+value.y,z+value.z};}
    constexpr Vec3 operator-(Vec3 value)const{return{x-value.x,y-value.y,z-value.z};}
    constexpr Vec3 operator*(float scale)const{return{x*scale,y*scale,z*scale};}
};
struct Vec4 {float x{},y{},z{},w{};};
struct Matrix4 {
    float values[4][4]{};
    static constexpr Matrix4 Identity(){Matrix4 result{};result.values[0][0]=result.values[1][1]=result.values[2][2]=result.values[3][3]=1.f;return result;}
    constexpr const float* operator[](std::size_t row)const{return values[row];}
    constexpr float* operator[](std::size_t row){return values[row];}
};
struct Color { float r{},g{},b{},a{1}; };
// 將線性色彩限制於有效範圍並打包成 GPU 可直接正規化的 RGBA8。
constexpr uint32_t PackColorRgba8(Color color){auto channel=[](float value){value=value<0.f?0.f:value>1.f?1.f:value;return uint32_t(value*255.f+.5f);};return channel(color.r)|(channel(color.g)<<8)|(channel(color.b)<<16)|(channel(color.a)<<24);}
inline float Length(Vec2 v){ return std::sqrt(v.x*v.x+v.y*v.y); }
inline float Distance(Vec2 a,Vec2 b){ return Length(a-b); }
inline Vec2 Normalize(Vec2 v){ float l=Length(v); return l>.0001f?v*(1.f/l):Vec2{}; }
constexpr float Dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
constexpr Vec3 Cross(Vec3 a,Vec3 b){return{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline float Length(Vec3 value){return std::sqrt(Dot(value,value));}
inline Vec3 Normalize(Vec3 value){float length=Length(value);return length>.0001f?value*(1.f/length):Vec3{};}
constexpr Matrix4 operator*(const Matrix4& left,const Matrix4& right){Matrix4 result{};for(std::size_t row=0;row<4;++row)for(std::size_t column=0;column<4;++column)for(std::size_t index=0;index<4;++index)result[row][column]+=left[row][index]*right[index][column];return result;}
constexpr Vec4 operator*(const Matrix4& matrix,Vec4 vector){return{matrix[0][0]*vector.x+matrix[0][1]*vector.y+matrix[0][2]*vector.z+matrix[0][3]*vector.w,matrix[1][0]*vector.x+matrix[1][1]*vector.y+matrix[1][2]*vector.z+matrix[1][3]*vector.w,matrix[2][0]*vector.x+matrix[2][1]*vector.y+matrix[2][2]*vector.z+matrix[2][3]*vector.w,matrix[3][0]*vector.x+matrix[3][1]*vector.y+matrix[3][2]*vector.z+matrix[3][3]*vector.w};}
inline float Clamp(float v,float a,float b){ return v<a?a:(v>b?b:v); }
}
