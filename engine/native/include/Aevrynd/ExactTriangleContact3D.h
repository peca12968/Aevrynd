#pragma once
#include <Aevrynd/Math.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace Aevrynd {
enum class ExactTriangleContactResult3D : std::uint8_t { Disjoint, AllowedSharedContact, ForbiddenContact, Invalid };

namespace ExactTriangleContactDetail3D {
inline void Require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
// 原 V809 有界算術補上完整約分；所有中間 Integer 仍受原 256 words 上限保護。
 // BEGIN_EXACT_RATIONAL_REDUCED_GCD_V1
struct Integer {
    bool negative{};std::vector<std::uint32_t> words;
    Integer()=default;
    explicit Integer(std::uint32_t value){if(value)words.push_back(value);}
    void Trim(){while(!words.empty()&&words.back()==0)words.pop_back();if(words.empty())negative=false;Require(words.size()<=256,"exact integer resource limit: unresolved");}
    int Sign()const{return words.empty()?0:(negative?-1:1);}
    static int Magnitude(const Integer& a,const Integer& b){if(a.words.size()!=b.words.size())return a.words.size()<b.words.size()?-1:1;for(std::size_t i=a.words.size();i>0;--i)if(a.words[i-1]!=b.words[i-1])return a.words[i-1]<b.words[i-1]?-1:1;return 0;}
    Integer Shift(unsigned count)const {
        if(!Sign())return{};Integer r;r.negative=negative;r.words.assign(count/32,0);
        std::uint64_t carry=0;for(auto w:words){const auto value=(std::uint64_t(w)<<(count%32))|carry;r.words.push_back(std::uint32_t(value));carry=value>>32;}
        if(carry)r.words.push_back(std::uint32_t(carry));r.Trim();return r;
    }
    unsigned Trailing()const{unsigned result=0;for(auto w:words){if(w)return result+unsigned(std::countr_zero(w));result+=32;}return result;}
    unsigned BitLength()const{return words.empty()?0u:unsigned((words.size()-1)*32)+32u-unsigned(std::countl_zero(words.back()));}
    bool One()const{return !negative&&words.size()==1&&words[0]==1;}
    bool PowerOfTwo()const{return Sign()>0&&Trailing()+1==BitLength();}
    void ShiftRight(unsigned count){const auto whole=count/32;Require(whole<=words.size(),"exact shift");words.erase(words.begin(),words.begin()+std::ptrdiff_t(whole));const auto bits=count%32;if(bits){std::uint32_t carry=0;for(std::size_t i=words.size();i>0;--i){const auto w=words[i-1];words[i-1]=(w>>bits)|carry;carry=w<<(32-bits);}}Trim();}
    friend Integer operator-(Integer a){if(a.Sign())a.negative=!a.negative;return a;}
    friend Integer operator+(const Integer& a,const Integer& b){
        if(!a.Sign())return b;if(!b.Sign())return a;Integer r;
        if(a.negative==b.negative){r.negative=a.negative;std::uint64_t carry=0;for(std::size_t i=0;i<(std::max)(a.words.size(),b.words.size());++i){const std::uint64_t value=carry+(i<a.words.size()?a.words[i]:0u)+(i<b.words.size()?b.words[i]:0u);r.words.push_back(std::uint32_t(value));carry=value>>32;}if(carry)r.words.push_back(std::uint32_t(carry));}
        else {const int order=Magnitude(a,b);if(!order)return{};const auto& hi=order>0?a:b;const auto& lo=order>0?b:a;r.negative=hi.negative;std::uint64_t borrow=0;for(std::size_t i=0;i<hi.words.size();++i){const std::uint64_t sub=borrow+(i<lo.words.size()?lo.words[i]:0u),value=hi.words[i];r.words.push_back(std::uint32_t(value-sub));borrow=value<sub?1u:0u;}Require(borrow==0,"exact subtraction carry");}
        r.Trim();return r;
    }
    friend Integer operator-(const Integer& a,const Integer& b){return a+(-b);}
    friend Integer operator*(const Integer& a,const Integer& b){
        if(!a.Sign()||!b.Sign())return{};Integer r;r.negative=a.negative!=b.negative;r.words.assign(a.words.size()+b.words.size(),0);
        for(std::size_t i=0;i<a.words.size();++i){std::uint64_t carry=0;for(std::size_t j=0;j<b.words.size();++j){const auto value=std::uint64_t(a.words[i])*b.words[j]+r.words[i+j]+carry;r.words[i+j]=std::uint32_t(value);carry=value>>32;}r.words[i+b.words.size()]=std::uint32_t(carry);}r.Trim();return r;
    }
    // Binary GCD 只做縮小、減法與右移，不產生超過輸入長度的暫存值。
    static Integer Gcd(Integer a,Integer b){
        a.Trim();b.Trim();a.negative=false;b.negative=false;if(!a.Sign())return b;if(!b.Sign())return a;
        const auto common=(std::min)(a.Trailing(),b.Trailing());a.ShiftRight(a.Trailing());b.ShiftRight(b.Trailing());
        for(;;){
            if(a.One()||b.One())return Integer(1).Shift(common);
            const int order=Magnitude(a,b);if(!order)return a.Shift(common);
            if(a.words.size()<=2&&b.words.size()<=2){
                const auto toUnsigned64=[](const Integer& n){return std::uint64_t(n.words[0])|(n.words.size()>1?std::uint64_t(n.words[1])<<32:0u);};
                const auto g=std::gcd(toUnsigned64(a),toUnsigned64(b));Integer result;result.words={std::uint32_t(g)};
                if(g>>32)result.words.push_back(std::uint32_t(g>>32));result.Trim();return result.Shift(common);}
            if(order>0){a=a-b;a.ShiftRight(a.Trailing());}else{b=b-a;b.ShiftRight(b.Trailing());}
        }
    }
    // 對齊除數後逐位相減；不可用可能多出第 8193 bit 的餘數左移。
    static Integer ExactDivide(Integer value,Integer divisor){
        value.Trim();divisor.Trim();Require(divisor.Sign()!=0,"exact divide by zero");
        const bool sign=value.negative!=divisor.negative;value.negative=false;divisor.negative=false;
        if(!value.Sign())return{};if(divisor.One()){value.negative=sign;return value;}
        if(divisor.PowerOfTwo()){const auto shift=divisor.Trailing();Require(value.Trailing()>=shift,"nonexact integer divide");
            value.ShiftRight(shift);value.negative=sign;return value;}
        if(value.words.size()<=2&&divisor.words.size()<=2){
            const auto toUnsigned64=[](const Integer& n){return std::uint64_t(n.words[0])|(n.words.size()>1?std::uint64_t(n.words[1])<<32:0u);};
            const auto n=toUnsigned64(value),d=toUnsigned64(divisor);Require(n%d==0,"nonexact integer divide");const auto q=n/d;
            Integer result;result.words={std::uint32_t(q)};if(q>>32)result.words.push_back(std::uint32_t(q>>32));result.negative=sign;result.Trim();return result;}
        Integer quotient;
        while(Magnitude(value,divisor)>=0){
            auto shift=value.BitLength()-divisor.BitLength();auto aligned=divisor.Shift(shift);
            if(Magnitude(aligned,value)>0){Require(shift>0,"exact division alignment");--shift;aligned.ShiftRight(1);}
            value=value-aligned;const auto word=std::size_t(shift/32);
            if(quotient.words.size()<=word)quotient.words.resize(word+1,0);
            const auto mask=std::uint32_t(1u)<<(shift%32);Require((quotient.words[word]&mask)==0,"exact division quotient bit");quotient.words[word]|=mask;
        }
        Require(!value.Sign(),"nonexact integer divide");quotient.negative=sign;quotient.Trim();return quotient;
    }
    double Mantissa(int& exponent)const {if(!Sign()){exponent=0;return 0;}const auto n=words.size();double value=words.back();if(n>1)value+=std::ldexp(double(words[n-2]),-32);exponent=int((n-1)*32);return negative?-value:value;}
};
struct Rational {
    Integer numerator,denominator{1};
private:
    static void ValidInputs(const Rational& a,const Rational& b){
        Require(a.denominator.Sign()>0&&b.denominator.Sign()>0,"invalid exact denominator");
        Require(a.numerator.words.size()<=256&&a.denominator.words.size()<=256&&b.numerator.words.size()<=256&&b.denominator.words.size()<=256,"exact integer resource limit: unresolved");
    }
    // 只供下方由正規化輸入推導出的互質結果；公開欄位供既有 receipt 讀取，不可直接改寫。
    static Rational Reduced(Integer n,Integer d){
        n.Trim();d.Trim();Require(d.Sign()!=0,"exact divide by zero");if(d.negative){n=-n;d=-d;}
        Rational out;if(!n.Sign())return out;out.numerator=std::move(n);out.denominator=std::move(d);return out;
    }
public:
    Rational()=default;
    explicit Rational(std::uint32_t value):numerator(value){}
    Rational(Integer a,Integer b):numerator(std::move(a)),denominator(std::move(b)){
        numerator.Trim();denominator.Trim();Require(denominator.Sign()!=0,"exact divide by zero");if(denominator.negative){denominator=-denominator;numerator=-numerator;}
        if(!numerator.Sign()){denominator=Integer(1);return;}
        const auto shift=(std::min)(numerator.Trailing(),denominator.Trailing());if(shift){numerator.ShiftRight(shift);denominator.ShiftRight(shift);}
        if(denominator.One()||denominator.PowerOfTwo())return;
        const auto g=Integer::Gcd(numerator,denominator);if(!g.One()){numerator=Integer::ExactDivide(numerator,g);denominator=Integer::ExactDivide(denominator,g);}
    }
    static Rational Float(float value){Require(std::isfinite(value),"nonfinite exact input");const auto bits=std::bit_cast<std::uint32_t>(value),code=(bits>>23)&255u,mantissa=(bits&0x7fffffu)|(code?0x800000u:0u);if(!mantissa)return{};Integer n(mantissa),d(1);const int exponent=code?int(code)-150:-149;if(exponent>=0)n=n.Shift(unsigned(exponent));else d=d.Shift(unsigned(-exponent));n.negative=(bits>>31)!=0;return{n,d};}
    int Sign()const{return numerator.Sign();}
    double Approximate()const {int a{},b{};const double n=numerator.Mantissa(a),d=denominator.Mantissa(b);return std::ldexp(n/d,a-b);}
    friend Rational operator-(Rational a){a.numerator=-a.numerator;return a;}
    friend Rational operator+(const Rational& a,const Rational& b){
        ValidInputs(a,b);
        if(!a.Sign())return b;if(!b.Sign())return a;
        if(Integer::Magnitude(a.denominator,b.denominator)==0)return{a.numerator+b.numerator,a.denominator};
        const auto g=Integer::Gcd(a.denominator,b.denominator),ad=Integer::ExactDivide(a.denominator,g),bd=Integer::ExactDivide(b.denominator,g);
        const auto n=a.numerator*bd+b.numerator*ad;if(!n.Sign())return{};
        const auto h=Integer::Gcd(n,g);return Reduced(Integer::ExactDivide(n,h),ad*Integer::ExactDivide(b.denominator,h));
    }
    friend Rational operator-(const Rational& a,const Rational& b){return a+(-b);}
    friend Rational operator*(const Rational& a,const Rational& b){
        ValidInputs(a,b);
        if(!a.Sign()||!b.Sign())return{};
        const auto g=Integer::Gcd(a.numerator,b.denominator),h=Integer::Gcd(b.numerator,a.denominator);
        return Reduced(Integer::ExactDivide(a.numerator,g)*Integer::ExactDivide(b.numerator,h),Integer::ExactDivide(a.denominator,h)*Integer::ExactDivide(b.denominator,g));
    }
    friend Rational operator/(const Rational& a,const Rational& b){
        ValidInputs(a,b);
        Require(b.Sign()!=0,"exact divide by zero");if(!a.Sign())return{};
        const auto g=Integer::Gcd(a.numerator,b.numerator),h=Integer::Gcd(a.denominator,b.denominator);
        return Reduced(Integer::ExactDivide(a.numerator,g)*Integer::ExactDivide(b.denominator,h),Integer::ExactDivide(a.denominator,h)*Integer::ExactDivide(b.numerator,g));
    }
    friend int Compare(const Rational& a,const Rational& b){
        ValidInputs(a,b);
        if(a.Sign()!=b.Sign())return a.Sign()<b.Sign()?-1:1;if(!a.Sign())return 0;
        if(Integer::Magnitude(a.denominator,b.denominator)==0)return(a.numerator-b.numerator).Sign();
        const auto g=Integer::Gcd(a.denominator,b.denominator),h=Integer::Gcd(a.numerator,b.numerator);
        return(Integer::ExactDivide(a.numerator,h)*Integer::ExactDivide(b.denominator,g)-Integer::ExactDivide(b.numerator,h)*Integer::ExactDivide(a.denominator,g)).Sign();
    }
};
// END_EXACT_RATIONAL_REDUCED_GCD_V1

using Point=std::array<Rational,3>;
using Triangle=std::array<Point,3>;
using Projected=std::array<Rational,2>;
inline Point ToExact(Vec3 v){return{{Rational::Float(v.x),Rational::Float(v.y),Rational::Float(v.z)}};}
inline Point Subtract(const Point& a,const Point& b){return{{a[0]-b[0],a[1]-b[1],a[2]-b[2]}};}
inline Point Cross3(const Point& a,const Point& b){return{{a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]}};}
inline Rational Dot3(const Point& a,const Point& b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
inline bool Nonzero(const Point& a){return a[0].Sign()!=0||a[1].Sign()!=0||a[2].Sign()!=0;}
inline bool Equal(const Point& a,const Point& b){for(unsigned k=0;k<3;++k)if(Compare(a[k],b[k])!=0)return false;return true;}
inline bool Between(const Rational& p,const Rational& a,const Rational& b){return(Compare(p,a)>=0&&Compare(p,b)<=0)||(Compare(p,b)>=0&&Compare(p,a)<=0);}
inline Point Interpolate(const Point& a,const Point& b,const Rational& t){return{{a[0]+(b[0]-a[0])*t,a[1]+(b[1]-a[1])*t,a[2]+(b[2]-a[2])*t}};}
inline void Append(std::vector<Point>& points,const Point& p){for(const auto& q:points)if(Equal(p,q))return;points.push_back(p);}
inline unsigned FirstNonzero(const Point& v){for(unsigned k=0;k<3;++k)if(v[k].Sign())return k;Require(false,"zero exact axis");return 0;}
inline bool SharedContains(const Point& p,std::span<const Point> shared){
    if(shared.empty())return false;if(shared.size()==1)return Equal(p,shared[0]);
    Require(shared.size()==2,"invalid declared simplex dimension");
    if(Nonzero(Cross3(Subtract(shared[1],shared[0]),Subtract(p,shared[0]))))return false;
    for(unsigned k=0;k<3;++k)if(!Between(p[k],shared[0][k],shared[1][k]))return false;return true;
}
inline Projected Project(const Point& p,unsigned drop){return{{p[(drop+1)%3],p[(drop+2)%3]}};}
inline Projected Subtract2(const Projected& a,const Projected& b){return{{a[0]-b[0],a[1]-b[1]}};}
inline Rational Cross2(const Projected& a,const Projected& b){return a[0]*b[1]-a[1]*b[0];}
inline Rational Orient(const Projected& a,const Projected& b,const Projected& c){return Cross2(Subtract2(b,a),Subtract2(c,a));}
inline bool OnSegment(const Projected& p,const Projected& a,const Projected& b){return !Orient(a,b,p).Sign()&&Between(p[0],a[0],b[0])&&Between(p[1],a[1],b[1]);}
inline bool Inside(const Projected& p,const std::array<Projected,3>& tri){
    const int area=Orient(tri[0],tri[1],tri[2]).Sign();Require(area!=0,"degenerate projected triangle");
    for(unsigned k=0;k<3;++k)if(Orient(tri[k],tri[(k+1)%3],p).Sign()*area<0)return false;return true;
}
// 完整切閉三角：plane-contained edge 的兩個端點皆保留，沒有 dp==dq 的跳過豁免。
inline std::vector<Point> Cut(const Triangle& tri,const Point& planeOrigin,const Point& normal){
    std::array<Rational,3> distance;std::vector<Point> result;
    for(unsigned k=0;k<3;++k){distance[k]=Dot3(normal,Subtract(tri[k],planeOrigin));if(!distance[k].Sign())Append(result,tri[k]);}
    for(unsigned k=0;k<3;++k){const unsigned next=(k+1)%3;if(distance[k].Sign()*distance[next].Sign()<0)Append(result,Interpolate(tri[k],tri[next],distance[k]/(distance[k]-distance[next])));}
    Require(result.size()<=2,"noncoplanar cut is not a point or segment");return result;
}
struct Interval {Point lower,upper;};
inline Interval Extrema(std::span<const Point> points,unsigned axis){
    Require(!points.empty(),"empty interval");Interval result{points[0],points[0]};
    for(const auto& p:points){if(Compare(p[axis],result.lower[axis])<0)result.lower=p;if(Compare(p[axis],result.upper[axis])>0)result.upper=p;}return result;
}
inline std::vector<Point> NoncoplanarIntersection(const Triangle& a,const Triangle& b,const Point& nA,const Point& nB,const Point& direction){
    const auto cutA=Cut(a,b[0],nB),cutB=Cut(b,a[0],nA);if(cutA.empty()||cutB.empty())return{};
    const unsigned axis=FirstNonzero(direction);const auto rangeA=Extrema(cutA,axis),rangeB=Extrema(cutB,axis);
    const auto& lower=Compare(rangeA.lower[axis],rangeB.lower[axis])>=0?rangeA.lower:rangeB.lower;
    const auto& upper=Compare(rangeA.upper[axis],rangeB.upper[axis])<=0?rangeA.upper:rangeB.upper;
    if(Compare(lower[axis],upper[axis])>0)return{};
    if(!Compare(lower[axis],upper[axis])){Require(Equal(lower,upper),"intersection line coordinate inconsistent");return{lower};}return{lower,upper};
}
// 共面凸交域全部生成點：兩邊的內含頂點 + 九組邊交點 + 共線交域端點。
// 這些點包含全部 extrema；不需浮點排序／面積或將 rational 交點 round 回 float。
inline std::vector<Point> CoplanarIntersection(const Triangle& a,const Triangle& b,const Point& nA){
    const unsigned drop=FirstNonzero(nA);std::array<Projected,3> pa,pb;std::vector<Point> result;
    for(unsigned k=0;k<3;++k){pa[k]=Project(a[k],drop);pb[k]=Project(b[k],drop);}
    for(unsigned k=0;k<3;++k){if(Inside(pa[k],pb))Append(result,a[k]);if(Inside(pb[k],pa))Append(result,b[k]);}
    const Rational zero,one(1);
    for(unsigned i=0;i<3;++i)for(unsigned j=0;j<3;++j){
        const unsigned nextI=(i+1)%3,nextJ=(j+1)%3;const auto r=Subtract2(pa[nextI],pa[i]),s=Subtract2(pb[nextJ],pb[j]),offset=Subtract2(pb[j],pa[i]);const auto den=Cross2(r,s);
        if(den.Sign()){
            const auto alpha=Cross2(offset,s)/den,beta=Cross2(offset,r)/den;
            if(Compare(alpha,zero)<0||Compare(alpha,one)>0||Compare(beta,zero)<0||Compare(beta,one)>0)continue;
            const auto point=Interpolate(a[i],a[nextI],alpha);Require(Equal(point,Interpolate(b[j],b[nextJ],beta)),"coplanar projected intersection inconsistent");Append(result,point);
        }else if(!Cross2(offset,r).Sign()){
            if(OnSegment(pa[i],pb[j],pb[nextJ]))Append(result,a[i]);
            if(OnSegment(pa[nextI],pb[j],pb[nextJ]))Append(result,a[nextI]);
            if(OnSegment(pb[j],pa[i],pa[nextI]))Append(result,b[j]);
            if(OnSegment(pb[nextJ],pa[i],pa[nextI]))Append(result,b[nextJ]);
        }
    }return result;
}
inline ExactTriangleContactResult3D Classify(const std::array<Vec3,3>& inputA,const std::array<Vec3,3>& inputB,std::span<const Vec3> inputShared){
    Require(inputShared.size()<=2,"declared shared simplex exceeds edge");
    Triangle a,b;for(unsigned k=0;k<3;++k){a[k]=ToExact(inputA[k]);b[k]=ToExact(inputB[k]);}
    const auto nA=Cross3(Subtract(a[1],a[0]),Subtract(a[2],a[0])),nB=Cross3(Subtract(b[1],b[0]),Subtract(b[2],b[0]));
    Require(Nonzero(nA)&&Nonzero(nB),"degenerate exact input triangle");
    std::array<Point,2> storedShared;
    for(std::size_t i=0;i<inputShared.size();++i){
        storedShared[i]=ToExact(inputShared[i]);bool existsA=false,existsB=false;
        for(unsigned k=0;k<3;++k){existsA=existsA||Equal(storedShared[i],a[k]);existsB=existsB||Equal(storedShared[i],b[k]);}
        Require(existsA&&existsB,"declared shared point is not an actual vertex of both triangles");
        if(i)Require(!Equal(storedShared[0],storedShared[1]),"duplicate shared points");
    }
    const auto direction=Cross3(nA,nB);std::vector<Point> intersection;
    if(Nonzero(direction))intersection=NoncoplanarIntersection(a,b,nA,nB,direction);
    else{
        if(Dot3(nA,Subtract(b[0],a[0])).Sign())return ExactTriangleContactResult3D::Disjoint;
        intersection=CoplanarIntersection(a,b,nA);
    }
    if(intersection.empty())return ExactTriangleContactResult3D::Disjoint;
    const std::span<const Point> shared(storedShared.data(),inputShared.size());
    for(const auto& point:intersection)if(!SharedContains(point,shared))return ExactTriangleContactResult3D::ForbiddenContact;
    return ExactTriangleContactResult3D::AllowedSharedContact;
}
template<class Work> inline ExactTriangleContactResult3D Guarded(Work&& work)noexcept{
    try{return std::forward<Work>(work)();}catch(...){return ExactTriangleContactResult3D::Invalid;}
}
}
// 只驗實存座標及閉交域；呼叫者仍須獨立核 source IDs／alias 政策，不能因位置巧合自授共享。
// 非有限、退化、虛假 simplex、8192-bit 算術超界或任何 unresolved 均 failclosed Invalid。
inline ExactTriangleContactResult3D ClassifyExactTriangleContact3D(const std::array<Vec3,3>& a,const std::array<Vec3,3>& b,std::span<const Vec3> shared={})noexcept{
    return ExactTriangleContactDetail3D::Guarded([&]{return ExactTriangleContactDetail3D::Classify(a,b,shared);});
}
}
