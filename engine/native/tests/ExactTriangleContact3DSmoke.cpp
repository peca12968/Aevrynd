#include <Aevrynd/ExactTriangleContact3D.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string_view>

namespace {
using Aevrynd::Vec3;
using Triangle=std::array<Vec3,3>;
using Result=Aevrynd::ExactTriangleContactResult3D;
namespace Exact=Aevrynd::ExactTriangleContactDetail3D;
std::size_t checkCount{},negativeCount{},failureCount{};
void Check(bool value,std::string_view name){++checkCount;if(!value){++failureCount;std::cerr<<"FAIL "<<name<<'\n';}}
inline constexpr std::array<std::array<unsigned,3>,6> Permutations{{{{0,1,2}},{{0,2,1}},{{1,0,2}},{{1,2,0}},{{2,0,1}},{{2,1,0}}}};
Triangle Permute(const Triangle& t,const std::array<unsigned,3>& p){return{{t[p[0]],t[p[1]],t[p[2]]}};}
bool IntegerMatches(const Exact::Integer& actual,std::int64_t expected){
    const auto magnitude=std::uint64_t(expected<0?-expected:expected);
    const auto size=magnitude?(magnitude>>32?2u:1u):0u;
    return actual.negative==(expected<0)&&actual.words.size()==size&&
        (!size||actual.words[0]==std::uint32_t(magnitude))&&(size<2||actual.words[1]==std::uint32_t(magnitude>>32));
}
// 獨立小整數 oracle：不用待測 Rational 四則或 Compare 推導預期值。
bool FractionMatches(const Exact::Rational& actual,std::int64_t n,std::int64_t d){
    if(d<0){n=-n;d=-d;}const auto g=std::gcd(n<0?-n:n,d);n/=g;d/=g;
    return IntegerMatches(actual.numerator,n)&&IntegerMatches(actual.denominator,d);
}
Exact::Rational SmallFraction(std::int64_t n,std::uint32_t d){
    auto numerator=Exact::Integer(std::uint32_t(n<0?-n:n));if(n<0)numerator=-numerator;
    return{numerator,Exact::Integer(d)};
}
std::string IntegerHex(const Exact::Integer& value){
    if(!value.Sign())return "0x0";std::ostringstream out;if(value.negative)out<<'-';out<<"0x"<<std::hex;
    for(std::size_t i=value.words.size();i>0;--i){if(i!=value.words.size())out<<std::setfill('0')<<std::setw(8);out<<value.words[i-1];}return out.str();
}
std::string FractionHex(const Exact::Rational& value){return IntegerHex(value.numerator)+"/"+IntegerHex(value.denominator);}
void EmitOracle(const Exact::Rational& a,const Exact::Rational& b){
    std::cout<<"NORMALIZATION_ORACLE a="<<FractionHex(a)<<" b="<<FractionHex(b)<<" sum="<<FractionHex(a+b)<<" difference="<<FractionHex(a-b)
        <<" product="<<FractionHex(a*b)<<" quotient="<<FractionHex(a/b)<<" compare="<<Compare(a,b)<<'\n';
}
void NormalizationControls(){
    using Exact::Integer;using Exact::Rational;
    auto doubled=Rational(1)/Rational(3);for(unsigned i=0;i<20;++i){doubled=doubled+doubled;
        Check(FractionMatches(doubled,std::int64_t(1)<<unsigned(i+1),3),"odd denominator repeated doubling stays canonical");}
    const auto odd=Integer(1).Shift(96)+Integer(1),threeOdd=odd*Integer(3),fiveOdd=odd*Integer(5);
    Check(IntegerMatches(Integer::Gcd(threeOdd,fiveOdd)-odd,0),"multiword odd gcd retained exactly");
    Check(FractionMatches(Rational(threeOdd,fiveOdd),3,5),"multiword odd common factor canonicalized");
    Check(FractionMatches(Rational(-threeOdd,-fiveOdd),3,5),"two negative integer factors normalize sign");
    Check(FractionMatches(Rational(threeOdd,-fiveOdd),-3,5),"negative denominator normalizes sign");
    Check(FractionMatches(Rational(Integer(3).Shift(200),Integer(5).Shift(200)),3,5),"large dyadic common factor canonicalized");
    Check(IntegerMatches(Integer::Gcd(Integer{},-threeOdd)-threeOdd,0),"gcd zero and negative input magnitude");
    Check(IntegerMatches(Integer::Gcd(Integer{},Integer{}),0),"gcd zero zero remains zero");
    Check(IntegerMatches(Integer::ExactDivide(-fiveOdd,odd),-5),"signed exact multiword quotient");
    Check(IntegerMatches(Integer::ExactDivide(fiveOdd,-odd),-5),"negative exact divisor sign");
    Check(IntegerMatches(Integer::ExactDivide(odd*odd,odd)-odd,0),"multiword exact quotient bits retained");
    Check(IntegerMatches(Integer::ExactDivide(Integer{},odd),0),"zero exact numerator");
    Check(Exact::Guarded([&]{(void)Integer::ExactDivide(fiveOdd+Integer(1),odd);return Result::Disjoint;})==Result::Invalid,"multiword nonexact remainder failclosed");
    Check(Exact::Guarded([]{(void)Integer::ExactDivide(Integer(21),Integer(4));return Result::Disjoint;})==Result::Invalid,"nonexact dyadic integer divide failclosed");
    Check(Exact::Guarded([]{(void)Integer::ExactDivide(Integer(25),Integer(7));return Result::Disjoint;})==Result::Invalid,"nonexact odd integer divide failclosed");
    Check(Exact::Guarded([]{(void)Integer::ExactDivide(Integer(1),Integer{});return Result::Disjoint;})==Result::Invalid,"integer zero divide failclosed");
    Check(Exact::Guarded([]{Rational invalid;invalid.denominator=Integer{};(void)(Rational{}*invalid);return Result::Disjoint;})==Result::Invalid,"zero multiplier cannot conceal invalid denominator");
    const auto boundary=Integer(1).Shift(8191)+Integer(3);const Rational large(boundary,Integer(3)),inverse(Integer(3),boundary);
    Check(FractionMatches(large*inverse,1,1),"8192 bit factors cancel before multiplication");
    Check(FractionMatches(large/large,1,1),"8192 bit factors cancel before division");
    Check(FractionMatches(large+(-large),0,1),"8192 bit opposite fractions cancel to canonical zero");
    Check(Compare(large,large)==0&&Compare(-large,-large)==0,"8192 bit exact comparison avoids unneeded products");
    Check(Exact::Guarded([&]{(void)(Rational(boundary,Integer(1))*Rational(2));return Result::Disjoint;})==Result::Invalid,"genuine rational numerator overflow remains failclosed");
    Check(Exact::Guarded([&]{(void)(inverse*inverse);return Result::Disjoint;})==Result::Invalid,"genuine rational denominator overflow remains failclosed");
    unsigned pairs{};
    for(std::int64_t an=-8;an<=8;++an)for(std::uint32_t ad=1;ad<=9;++ad)
        for(std::int64_t bn=-8;bn<=8;++bn)for(std::uint32_t bd=1;bd<=9;++bd){
            const auto a=SmallFraction(an,ad),b=SmallFraction(bn,bd);const auto left=an*std::int64_t(bd),right=bn*std::int64_t(ad);
            Check(FractionMatches(a,an,ad)&&FractionMatches(b,bn,bd),"exhaustive independent canonical constructor");
            Check(FractionMatches(a+b,left+right,std::int64_t(ad)*bd),"exhaustive independent fraction addition");
            Check(FractionMatches(a-b,left-right,std::int64_t(ad)*bd),"exhaustive independent fraction subtraction");
            Check(FractionMatches(a*b,an*bn,std::int64_t(ad)*bd),"exhaustive independent fraction multiplication");
            if(bn)Check(FractionMatches(a/b,left,std::int64_t(ad)*bn),"exhaustive independent fraction division");
            Check(Compare(a,b)==(left<right?-1:left>right?1:0),"exhaustive independent exact comparison");++pairs;
        }
    for(unsigned i=0;i<16;++i)EmitOracle(SmallFraction(std::int64_t(i)-8,i+3),SmallFraction(i%2?-std::int64_t(i+1):std::int64_t(i+1),i+5));
    EmitOracle(Rational(-odd,Integer(3)),Rational(Integer(5),odd));
    std::cout<<"EXACT_NORMALIZATION_CONTROLS small_pairs="<<pairs<<" repeated_odd_doubling=20 word_limit=256 fraction_representation=reduced-gcd-v1 storage_canonicalization_changed=true native_qualified=false art_qualified=false\n";
}
void Case(std::string_view name,const Triangle& a,const Triangle& b,std::span<const Vec3> shared,Result expected){
    for(const auto& pa:Permutations)for(const auto& pb:Permutations){const auto x=Permute(a,pa),y=Permute(b,pb);
        for(bool reverse:{false,true}){const auto actual=reverse?Aevrynd::ClassifyExactTriangleContact3D(y,x,shared):Aevrynd::ClassifyExactTriangleContact3D(x,y,shared);Check(actual==expected,name);if(expected==Result::ForbiddenContact||expected==Result::Invalid)++negativeCount;
            if(shared.size()==2){const std::array<Vec3,2> opposite{{shared[1],shared[0]}};Check((reverse?Aevrynd::ClassifyExactTriangleContact3D(y,x,opposite):Aevrynd::ClassifyExactTriangleContact3D(x,y,opposite))==expected,name);if(expected==Result::ForbiddenContact||expected==Result::Invalid)++negativeCount;}
        }
    }
}
}
int main(){
    static_assert(sizeof(float)==4&&std::numeric_limits<float>::is_iec559);
    using namespace Aevrynd;
    // m1120 actual saved f32 readback，不用舊 binary64 dp/alpha 當 oracle。
    // IDs A=8242/8244/8243；B=8698/8699/8700。十七位原 readback 小數以 f32 literal 固定。
    const Triangle actualA{{{0.016533726826310158f,1.6058217287063599f,-0.07709205150604248f},{0.016915725544095039f,1.6109670400619507f,-0.079248659312725067f},{1.3720788274440565e-06f,1.6058249473571777f,-0.078635327517986298f}}};
    const Triangle actualB{{{1.6019552049328922e-06f,1.6056292057037354f,-0.07909538596868515f},actualA[0],actualA[2]}};
    const std::array<std::array<double,3>,6> rawReadback{{{{0.016533726826310158,1.6058217287063599,-0.07709205150604248}},{{0.016915725544095039,1.6109670400619507,-0.079248659312725067}},{{1.3720788274440565e-06,1.6058249473571777,-0.078635327517986298}},{{1.6019552049328922e-06,1.6056292057037354,-0.07909538596868515}},{{0.016533726826310158,1.6058217287063599,-0.07709205150604248}},{{1.3720788274440565e-06,1.6058249473571777,-0.078635327517986298}}}};
    for(unsigned record=0;record<6;++record){const auto p=record<3?actualA[record]:actualB[record-3];Check(double(p.x)==rawReadback[record][0]&&double(p.y)==rawReadback[record][1]&&double(p.z)==rawReadback[record][2],"all six f32 records exact m1120 promoted readback");}
    const std::array<Vec3,2> actualShared{{actualA[0],actualA[2]}};const std::array<Vec3,1> actualOne{{actualA[0]}};
    Case("m1120 actual lawful full shared edge",actualA,actualB,actualShared,Result::AllowedSharedContact);
    Case("m1120 same full edge without alias is forbidden",actualA,actualB,{},Result::ForbiddenContact);
    Case("m1120 point declaration cannot hide full edge",actualA,actualB,actualOne,Result::ForbiddenContact);
    // 實存兩個 endpoint 的 exact dyadic 值另由原 readback 保護，不 round 新交點。
    Check(Compare(Exact::Rational::Float(actualA[0].x),Exact::Rational(8876477u)/Exact::Rational(536870912u))==0,"actual8242 stored X dyadic");
    Check(Compare(Exact::Rational::Float(actualA[0].y),Exact::Rational(13470609u)/Exact::Rational(8388608u))==0,"actual8242 stored Y dyadic");
    Check(Compare(Exact::Rational::Float(actualA[0].z),-(Exact::Rational(646695u)/Exact::Rational(8388608u)))==0,"actual8242 stored Z dyadic");
    const Exact::Rational denominator8243(Exact::Integer(1),Exact::Integer(1).Shift(43));
    Check(Compare(Exact::Rational::Float(actualA[2].x),Exact::Rational(12068933u)*denominator8243)==0,"actual8243 stored X dyadic");
    Check(Compare(Exact::Rational::Float(actualA[2].y),Exact::Rational(3367659u)/Exact::Rational(2097152u))==0,"actual8243 stored Y dyadic");
    Check(Compare(Exact::Rational::Float(actualA[2].z),-(Exact::Rational(10554255u)/Exact::Rational(134217728u)))==0,"actual8243 stored Z dyadic");

    const Triangle a{{{0,0,0},{2,0,0},{0,2,0}}};const std::array<Vec3,2> edge{{a[0],a[1]}};const std::array<Vec3,1> point{{a[0]}};
    const Triangle adjacent{{a[1],a[0],{1,-2,0}}},fold{{a[0],a[1],{1,1,0}}},hinge{{a[1],a[0],{1,0,1}}};
    Case("coplanar adjacent only shared edge",a,adjacent,edge,Result::AllowedSharedContact);
    Case("coplanar same-side fold has area beyond shared edge",a,fold,edge,Result::ForbiddenContact);
    Case("noncoplanar zero-plane full edge retained",a,hinge,edge,Result::AllowedSharedContact);
    Case("noncoplanar same full edge no declaration",a,hinge,{},Result::ForbiddenContact);
    const Triangle pointOnly{{a[0],{-1,0,1},{0,-1,1}}},coplanarPoint{{a[0],{-2,0,0},{0,-2,0}}};
    Case("noncoplanar legitimate point",a,pointOnly,point,Result::AllowedSharedContact);
    Case("coplanar legitimate point",a,coplanarPoint,point,Result::AllowedSharedContact);
    Case("physical position coincidence is not automatic alias",a,pointOnly,{},Result::ForbiddenContact);
    const Triangle extended{{a[0],{1,0,0},{.5f,0,1}}},interiorEdge{{{.5f,0,0},{1.5f,0,0},{1,0,1}}};
    Case("shared point but contact extends along segment",a,extended,point,Result::ForbiddenContact);
    Case("plane-contained nonshared edge contact",a,interiorEdge,{},Result::ForbiddenContact);
    const Triangle crossing{{{.5f,.5f,-1},{.5f,.5f,1},{1.5f,.5f,0}}},parallel{{{0,0,1},{2,0,1},{0,2,1}}},away{{{3,3,-1},{3,3,1},{4,3,0}}};
    Case("proper noncoplanar crossing",a,crossing,{},Result::ForbiddenContact);
    Case("parallel different planes disjoint",a,parallel,{},Result::Disjoint);
    Case("nonparallel empty plane cut disjoint",a,away,{},Result::Disjoint);
    const Triangle inset{{{.25f,.25f,0},{1,.25f,0},{.25f,1,0}}},separate{{{3,0,0},{4,0,0},{3,1,0}}};
    Case("coplanar positive area containment",a,inset,{},Result::ForbiddenContact);
    Case("coplanar disjoint",a,separate,{},Result::Disjoint);
    const Triangle endpoint{{a[1],{3,-1,0},{3,0,0}}};const std::array<Vec3,1> point2{{a[1]}};
    Case("closed collinear edge intervals share endpoint",a,endpoint,point2,Result::AllowedSharedContact);
    Case("identical triangles no automatic exemption",a,a,{},Result::ForbiddenContact);
    Case("two shared points cannot whitelist triangle area",a,a,edge,Result::ForbiddenContact);
    Case("three shared points invalid",a,a,a,Result::Invalid);
    const std::array<Vec3,1> fake{{a[1]}},interior{{{.5f,.5f,0}}};const std::array<Vec3,2> duplicate{{a[0],a[0]}};
    Case("false shared ID position absent from second triangle",a,pointOnly,fake,Result::Invalid);
    Case("interior point is not actual vertex",a,crossing,interior,Result::Invalid);
    Case("duplicate shared points invalid",a,hinge,duplicate,Result::Invalid);
    Case("false shared metadata invalid even if pair disjoint",a,parallel,point,Result::Invalid);

    const float tiny=std::bit_cast<float>(1u),negativeTiny=std::bit_cast<float>(0x80000001u);
    const Triangle tinyFold{{a[0],a[1],{1,tiny,0}}},tinyAdjacent{{a[0],a[1],{1,negativeTiny,0}}},tinyParallel{{{0,0,tiny},{2,0,tiny},{0,2,tiny}}};
    Case("one-f32-ULP coplanar area escape forbidden",a,tinyFold,edge,Result::ForbiddenContact);
    Case("one-f32-ULP opposite coplanar adjacent lawful",a,tinyAdjacent,edge,Result::AllowedSharedContact);
    Case("one-f32-ULP parallel plane separation disjoint",a,tinyParallel,{},Result::Disjoint);
    const Triangle tinyCross{{{.5f,.5f,1},{1,.5f,1},{.5f,1,negativeTiny}}},tinyAbove{{{.5f,.5f,1},{1,.5f,1},{.5f,1,tiny}}};
    Case("one-f32-ULP true crossing is not lost",a,tinyCross,{},Result::ForbiddenContact);
    Case("one-f32-ULP side change eliminates crossing",a,tinyAbove,{},Result::Disjoint);
    const std::array<Vec3,1> falseUlp{{{std::nextafter(2.f,3.f),0,0}}};Case("one-ULP false shared position invalid",a,hinge,falseUlp,Result::Invalid);
    const Triangle line{{{0,0,0},{1,0,0},{2,0,0}}},collapsed{{a[0],a[0],a[1]}};
    Case("collinear exact triangle invalid",a,line,{},Result::Invalid);Case("duplicate triangle vertex invalid",a,collapsed,{},Result::Invalid);
    auto nonfinite=a;nonfinite[0].x=std::bit_cast<float>(0x7fc00000u);Case("NaN invalid",a,nonfinite,{},Result::Invalid);nonfinite=a;nonfinite[0].y=std::bit_cast<float>(0x7f800000u);Case("Infinity invalid",a,nonfinite,{},Result::Invalid);
    const std::array<Vec3,1> nanShared{{{std::bit_cast<float>(0x7fc00000u),0,0}}};Case("NaN declared simplex invalid",a,hinge,nanShared,Result::Invalid);
    const std::array<Vec3,1> signedZero{{{std::bit_cast<float>(0x80000000u),0,0}}};Case("negative zero is physical zero point",a,pointOnly,signedZero,Result::AllowedSharedContact);

    using Exact::Integer;using Exact::Rational;
    const Rational maxWord(0xffffffffu);Check(Compare((maxWord*maxWord)/maxWord,maxWord)==0,"mechanical integer multiword carry");Check(Rational::Float(tiny).Sign()>0&&Rational::Float(negativeTiny).Sign()<0,"f32 subnormal bits retained exactly");
    Check(Compare(Rational::Float(-0.f),Rational{})==0,"signed zero exact arithmetic");
    Check(Integer(0xffffffffu).Shift(8160).words.size()==256,"original 8192-bit word boundary retained");
    Check(Exact::Guarded([]{(void)(Rational(1)/Rational{});return Result::Disjoint;})==Result::Invalid,"zero divide failclosed Invalid");
    Check(Exact::Guarded([]{(void)Integer(1).Shift(8192);return Result::Disjoint;})==Result::Invalid,"8193-bit integer growth failclosed Invalid");
    Check(Exact::Guarded([]{Integer full;full.words.assign(256,0xffffffffu);full.Trim();(void)(full+Integer(1));return Result::Disjoint;})==Result::Invalid,"carry beyond256words failclosed Invalid");
    NormalizationControls();
    std::cout<<"SUMMARY checks="<<checkCount<<" forbidden_or_invalid_controls="<<negativeCount<<" failures="<<failureCount<<" all6_per_triangle_permutations=true symmetric=true actual_pair_ids=8242,8244,8243/8698,8699,8700 source_geometry_changed=false hair_qualified=false native_qualified=false art_qualified=false\n";
    return failureCount?1:0;
}
