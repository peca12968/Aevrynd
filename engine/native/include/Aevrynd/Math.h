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
struct Transform2D {
    Vec2 translation{};
    Vec2 scale{1.f,1.f};
    Vec2 pivot{};
    float rotation{};
};
struct Matrix4 {
    float values[4][4]{};
    static constexpr Matrix4 Identity(){Matrix4 result{};result.values[0][0]=result.values[1][1]=result.values[2][2]=result.values[3][3]=1.f;return result;}
    constexpr const float* operator[](std::size_t row)const{return values[row];}
    constexpr float* operator[](std::size_t row){return values[row];}
};
struct Color { float r{},g{},b{},a{1}; };
// 將線性色彩限制於有效範圍並打包成 GPU 可直接正規化的 RGBA8。
constexpr uint32_t PackColorRgba8(Color color){auto channel=[](float value){value=value<0.f?0.f:value>1.f?1.f:value;return uint32_t(value*255.f+.5f);};return channel(color.r)|(channel(color.g)<<8)|(channel(color.b)<<16)|(channel(color.a)<<24);}
// xyz 儲存單位法線，w 保留作為頂點是否接受光照的權重。
constexpr uint32_t PackNormalSnorm8(Vec3 normal,float lightingWeight=1.f){auto channel=[](float value){value=value<-1.f?-1.f:value>1.f?1.f:value;const int packed=int(value*127.f+(value>=0?.5f:-.5f));return uint32_t(uint8_t(int8_t(packed)));};return channel(normal.x)|(channel(normal.y)<<8)|(channel(normal.z)<<16)|(channel(lightingWeight)<<24);}
inline Vec4 UnpackNormalSnorm8(uint32_t packed){auto channel=[](uint32_t value){return(float)(int8_t(uint8_t(value)))/127.f;};return{channel(packed),channel(packed>>8),channel(packed>>16),channel(packed>>24)};}
inline float Length(Vec2 v){ return std::sqrt(v.x*v.x+v.y*v.y); }
inline float Distance(Vec2 a,Vec2 b){ return Length(a-b); }
inline Vec2 Normalize(Vec2 v){ float l=Length(v); return l>.0001f?v*(1.f/l):Vec2{}; }
inline Vec2 TransformPoint(const Transform2D& transform,Vec2 point){const float cosine=std::cos(transform.rotation),sine=std::sin(transform.rotation);const Vec2 local{(point.x-transform.pivot.x)*transform.scale.x,(point.y-transform.pivot.y)*transform.scale.y};return{local.x*cosine-local.y*sine+transform.pivot.x+transform.translation.x,local.x*sine+local.y*cosine+transform.pivot.y+transform.translation.y};}
constexpr float Dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
constexpr Vec3 Cross(Vec3 a,Vec3 b){return{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline float Length(Vec3 value){return std::sqrt(Dot(value,value));}
inline Vec3 Normalize(Vec3 value){float length=Length(value);return length>.0001f?value*(1.f/length):Vec3{};}
constexpr Matrix4 operator*(const Matrix4& left,const Matrix4& right){Matrix4 result{};for(std::size_t row=0;row<4;++row)for(std::size_t column=0;column<4;++column)for(std::size_t index=0;index<4;++index)result[row][column]+=left[row][index]*right[index][column];return result;}
constexpr Vec4 operator*(const Matrix4& matrix,Vec4 vector){return{matrix[0][0]*vector.x+matrix[0][1]*vector.y+matrix[0][2]*vector.z+matrix[0][3]*vector.w,matrix[1][0]*vector.x+matrix[1][1]*vector.y+matrix[1][2]*vector.z+matrix[1][3]*vector.w,matrix[2][0]*vector.x+matrix[2][1]*vector.y+matrix[2][2]*vector.z+matrix[2][3]*vector.w,matrix[3][0]*vector.x+matrix[3][1]*vector.y+matrix[3][2]*vector.z+matrix[3][3]*vector.w};}
constexpr Matrix4 CreateTranslationMatrix(Vec3 translation){Matrix4 result=Matrix4::Identity();result[0][3]=translation.x;result[1][3]=translation.y;result[2][3]=translation.z;return result;}
constexpr Matrix4 CreateScaleMatrix(Vec3 scale){Matrix4 result{};result[0][0]=scale.x;result[1][1]=scale.y;result[2][2]=scale.z;result[3][3]=1.f;return result;}
inline Matrix4 CreateRotationMatrix(Vec3 rotation){const float sx=std::sin(rotation.x),cx=std::cos(rotation.x),sy=std::sin(rotation.y),cy=std::cos(rotation.y),sz=std::sin(rotation.z),cz=std::cos(rotation.z);Matrix4 x=Matrix4::Identity(),y=Matrix4::Identity(),z=Matrix4::Identity();x[1][1]=cx;x[1][2]=-sx;x[2][1]=sx;x[2][2]=cx;y[0][0]=cy;y[0][2]=sy;y[2][0]=-sy;y[2][2]=cy;z[0][0]=cz;z[0][1]=-sz;z[1][0]=sz;z[1][1]=cz;return y*x*z;}
inline Matrix4 CreateTransformMatrix(Vec3 position,Vec3 rotation,Vec3 scale){return CreateTranslationMatrix(position)*CreateRotationMatrix(rotation)*CreateScaleMatrix(scale);}
// 法線使用模型矩陣左上 3x3 的反矩陣轉置，非等比縮放後仍維持正確方向。
inline Matrix4 CreateNormalMatrix(const Matrix4& transform){const float a=transform[0][0],b=transform[0][1],c=transform[0][2],d=transform[1][0],e=transform[1][1],f=transform[1][2],g=transform[2][0],h=transform[2][1],i=transform[2][2];const float determinant=a*(e*i-f*h)-b*(d*i-f*g)+c*(d*h-e*g);if(std::abs(determinant)<.000001f)return Matrix4::Identity();const float inverse=1.f/determinant;Matrix4 result{};result[0][0]=(e*i-f*h)*inverse;result[0][1]=(f*g-d*i)*inverse;result[0][2]=(d*h-e*g)*inverse;result[1][0]=(c*h-b*i)*inverse;result[1][1]=(a*i-c*g)*inverse;result[1][2]=(b*g-a*h)*inverse;result[2][0]=(b*f-c*e)*inverse;result[2][1]=(c*d-a*f)*inverse;result[2][2]=(a*e-b*d)*inverse;result[3][3]=1;return result;}
constexpr Vec3 TransformPoint(const Matrix4& matrix,Vec3 point){Vec4 value=matrix*Vec4{point.x,point.y,point.z,1};return std::abs(value.w)>.00001f?Vec3{value.x/value.w,value.y/value.w,value.z/value.w}:Vec3{value.x,value.y,value.z};}
inline Vec3 TransformNormal(const Matrix4& normalMatrix,Vec3 normal){Vec4 value=normalMatrix*Vec4{normal.x,normal.y,normal.z,0};return Normalize({value.x,value.y,value.z});}
inline Matrix4 CreateDirectionalShadowMatrix(Vec3 direction,float planeHeight){direction=Normalize(direction);if(std::abs(direction.y)<.0001f)return Matrix4::Identity();const float inverseY=1.f/direction.y;Matrix4 result=Matrix4::Identity();result[0][1]=-direction.x*inverseY;result[0][3]=direction.x*planeHeight*inverseY;result[1][1]=0;result[1][3]=planeHeight;result[2][1]=-direction.z*inverseY;result[2][3]=direction.z*planeHeight*inverseY;return result;}
inline float Clamp(float v,float a,float b){ return v<a?a:(v>b?b:v); }
}
