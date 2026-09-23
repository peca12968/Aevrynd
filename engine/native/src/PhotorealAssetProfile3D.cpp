#include "Aevrynd/PhotorealAssetProfile3D.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <fstream>
#include <limits>
#include <new>
#include <unordered_set>
#include <utility>

namespace Aevrynd {
namespace {

constexpr std::size_t MaximumCatalogJsonBytes3D=16u*1024u*1024u;
constexpr std::size_t MaximumReceiptJsonBytes3D=1024u*1024u;

enum class JsonKind : std::uint8_t {Null,Boolean,Number,String,Array,Object};
struct JsonValue {
    JsonKind kind{JsonKind::Null};
    bool boolean{};
    std::uint64_t number{};
    double real{};
    bool integerNumber{};
    std::string string;
    std::vector<JsonValue> array;
    std::vector<std::pair<std::string,JsonValue>> object;
};

bool ValidUtf8(std::string_view value){
    for(std::size_t index=0;index<value.size();){
        const auto first=static_cast<unsigned char>(value[index++]);if(first<=0x7f)continue;
        std::uint32_t codepoint{};std::size_t continuation{};
        if(first>=0xc2&&first<=0xdf){codepoint=first&0x1f;continuation=1;}
        else if(first>=0xe0&&first<=0xef){codepoint=first&0x0f;continuation=2;}
        else if(first>=0xf0&&first<=0xf4){codepoint=first&0x07;continuation=3;}
        else return false;
        if(index+continuation>value.size())return false;
        for(std::size_t byte=0;byte<continuation;++byte){const auto next=static_cast<unsigned char>(value[index++]);if((next&0xc0)!=0x80)return false;codepoint=(codepoint<<6)|(next&0x3f);}
        if(continuation==2&&(codepoint<0x800||(codepoint>=0xd800&&codepoint<=0xdfff))||continuation==3&&(codepoint<0x10000||codepoint>0x10ffff))return false;
    }
    return true;
}

void SetError(PhotorealAssetError3D* error,PhotorealAssetErrorCode3D code,std::size_t offset,std::string message){
    if(error)*error={code,offset,std::move(message)};
}

class JsonParser {
public:
    JsonParser(std::string_view text,std::size_t maximumStringBytes):text_(text),maximumStringBytes_(maximumStringBytes){}
    bool Parse(JsonValue& output,PhotorealAssetError3D* error){
        SkipWhitespace();
        if(!Value(output,0,error))return false;
        SkipWhitespace();
        if(position_!=text_.size()){SetError(error,PhotorealAssetErrorCode3D::InvalidJson,position_,"Trailing JSON data");return false;}
        return true;
    }
private:
    static bool Hex(char value,std::uint32_t& digit){
        if(value>='0'&&value<='9'){digit=std::uint32_t(value-'0');return true;}
        if(value>='a'&&value<='f'){digit=std::uint32_t(value-'a'+10);return true;}
        if(value>='A'&&value<='F'){digit=std::uint32_t(value-'A'+10);return true;}
        return false;
    }
    static void AppendUtf8(std::string& output,std::uint32_t codepoint){
        if(codepoint<=0x7f)output.push_back(char(codepoint));
        else if(codepoint<=0x7ff){output.push_back(char(0xc0|(codepoint>>6)));output.push_back(char(0x80|(codepoint&0x3f)));}
        else if(codepoint<=0xffff){output.push_back(char(0xe0|(codepoint>>12)));output.push_back(char(0x80|((codepoint>>6)&0x3f)));output.push_back(char(0x80|(codepoint&0x3f)));}
        else{output.push_back(char(0xf0|(codepoint>>18)));output.push_back(char(0x80|((codepoint>>12)&0x3f)));output.push_back(char(0x80|((codepoint>>6)&0x3f)));output.push_back(char(0x80|(codepoint&0x3f)));}
    }
    void SkipWhitespace(){while(position_<text_.size()&&(text_[position_]==' '||text_[position_]=='\t'||text_[position_]=='\r'||text_[position_]=='\n'))++position_;}
    bool String(std::string& output,PhotorealAssetError3D* error){
        if(position_>=text_.size()||text_[position_]!='"'){SetError(error,PhotorealAssetErrorCode3D::InvalidJson,position_,"Expected JSON string");return false;}
        ++position_;output.clear();
        while(position_<text_.size()){
            const unsigned char value=static_cast<unsigned char>(text_[position_++]);
            if(value=='"'){if(ValidUtf8(output))return true;SetError(error,PhotorealAssetErrorCode3D::InvalidJson,position_-1,"Invalid UTF-8 in JSON string");return false;}
            if(value<0x20){SetError(error,PhotorealAssetErrorCode3D::InvalidJson,position_-1,"Control character in JSON string");return false;}
            if(value!='\\')output.push_back(char(value));
            else{
                if(position_>=text_.size()){SetError(error,PhotorealAssetErrorCode3D::InvalidJson,position_,"Truncated JSON escape");return false;}
                const char escape=text_[position_++];
                switch(escape){
                case '"':output.push_back('"');break;case '\\':output.push_back('\\');break;case '/':output.push_back('/');break;
                case 'b':output.push_back('\b');break;case 'f':output.push_back('\f');break;case 'n':output.push_back('\n');break;
                case 'r':output.push_back('\r');break;case 't':output.push_back('\t');break;
                case 'u':{
                    std::uint32_t codepoint{};
                    for(int index=0;index<4;++index){std::uint32_t digit{};if(position_>=text_.size()||!Hex(text_[position_++],digit)){SetError(error,PhotorealAssetErrorCode3D::InvalidJson,position_,"Invalid Unicode escape");return false;}codepoint=(codepoint<<4)|digit;}
                    if(codepoint>=0xd800&&codepoint<=0xdbff){
                        if(position_+6>text_.size()||text_[position_]!='\\'||text_[position_+1]!='u'){SetError(error,PhotorealAssetErrorCode3D::InvalidJson,position_,"Missing low surrogate");return false;}
                        position_+=2;std::uint32_t low{};for(int index=0;index<4;++index){std::uint32_t digit{};if(!Hex(text_[position_++],digit)){SetError(error,PhotorealAssetErrorCode3D::InvalidJson,position_,"Invalid low surrogate");return false;}low=(low<<4)|digit;}
                        if(low<0xdc00||low>0xdfff){SetError(error,PhotorealAssetErrorCode3D::InvalidJson,position_,"Invalid low surrogate");return false;}
                        codepoint=0x10000+((codepoint-0xd800)<<10)+(low-0xdc00);
                    }else if(codepoint>=0xdc00&&codepoint<=0xdfff){SetError(error,PhotorealAssetErrorCode3D::InvalidJson,position_,"Unexpected low surrogate");return false;}
                    AppendUtf8(output,codepoint);break;
                }
                default:SetError(error,PhotorealAssetErrorCode3D::InvalidJson,position_-1,"Invalid JSON escape");return false;
                }
            }
            if(output.size()>maximumStringBytes_){SetError(error,PhotorealAssetErrorCode3D::InputLimitExceeded,position_,"JSON string exceeds limit");return false;}
        }
        SetError(error,PhotorealAssetErrorCode3D::InvalidJson,position_,"Unterminated JSON string");return false;
    }
    bool Number(JsonValue& output,PhotorealAssetError3D* error){
        const std::size_t start=position_;const bool negative=position_<text_.size()&&text_[position_]=='-';if(negative)++position_;
        if(position_>=text_.size()||text_[position_]<'0'||text_[position_]>'9'){SetError(error,PhotorealAssetErrorCode3D::InvalidJson,position_,"Invalid JSON number");return false;}if(text_[position_]=='0')++position_;else while(position_<text_.size()&&text_[position_]>='0'&&text_[position_]<='9')++position_;
        bool fractional{};if(position_<text_.size()&&text_[position_]=='.'){fractional=true;++position_;const std::size_t fraction=position_;while(position_<text_.size()&&text_[position_]>='0'&&text_[position_]<='9')++position_;if(position_==fraction){SetError(error,PhotorealAssetErrorCode3D::InvalidJson,position_,"Invalid JSON fraction");return false;}}
        bool exponential{};if(position_<text_.size()&&(text_[position_]=='e'||text_[position_]=='E')){exponential=true;++position_;if(position_<text_.size()&&(text_[position_]=='+'||text_[position_]=='-'))++position_;const std::size_t exponent=position_;while(position_<text_.size()&&text_[position_]>='0'&&text_[position_]<='9')++position_;if(position_==exponent){SetError(error,PhotorealAssetErrorCode3D::InvalidJson,position_,"Invalid JSON exponent");return false;}}
        double parsed{};const auto conversion=std::from_chars(text_.data()+start,text_.data()+position_,parsed);if(conversion.ec!=std::errc{}||conversion.ptr!=text_.data()+position_||!std::isfinite(parsed)){SetError(error,PhotorealAssetErrorCode3D::InvalidJson,start,"JSON number is not finite");return false;}output.kind=JsonKind::Number;output.real=parsed;if(!negative&&!fractional&&!exponential){std::uint64_t exact{};const auto integerConversion=std::from_chars(text_.data()+start,text_.data()+position_,exact);if(integerConversion.ec!=std::errc{}||integerConversion.ptr!=text_.data()+position_){SetError(error,PhotorealAssetErrorCode3D::InvalidJson,start,"JSON integer overflow");return false;}output.number=exact;output.integerNumber=true;}else if(parsed>=0.&&parsed<=9007199254740991.&&std::floor(parsed)==parsed){output.number=static_cast<std::uint64_t>(parsed);output.integerNumber=true;}return true;
    }
    bool Value(JsonValue& output,std::size_t depth,PhotorealAssetError3D* error){
        if(depth>64||++nodes_>100000){SetError(error,PhotorealAssetErrorCode3D::InputLimitExceeded,position_,"JSON complexity exceeds limit");return false;}
        SkipWhitespace();if(position_>=text_.size()){SetError(error,PhotorealAssetErrorCode3D::InvalidJson,position_,"Unexpected end of JSON");return false;}
        const char value=text_[position_];
        if(value=='"'){output.kind=JsonKind::String;return String(output.string,error);}
        if(value>='0'&&value<='9'||value=='-')return Number(output,error);
        if(text_.substr(position_,4)=="true"){position_+=4;output.kind=JsonKind::Boolean;output.boolean=true;return true;}
        if(text_.substr(position_,5)=="false"){position_+=5;output.kind=JsonKind::Boolean;output.boolean=false;return true;}
        if(text_.substr(position_,4)=="null"){position_+=4;output.kind=JsonKind::Null;return true;}
        if(value=='['){
            ++position_;output.kind=JsonKind::Array;SkipWhitespace();if(position_<text_.size()&&text_[position_]==']'){++position_;return true;}
            while(true){output.array.emplace_back();if(!Value(output.array.back(),depth+1,error))return false;SkipWhitespace();if(position_>=text_.size()){SetError(error,PhotorealAssetErrorCode3D::InvalidJson,position_,"Unterminated array");return false;}const char separator=text_[position_++];if(separator==']')return true;if(separator!=','){SetError(error,PhotorealAssetErrorCode3D::InvalidJson,position_-1,"Expected array separator");return false;}}
        }
        if(value=='{'){
            ++position_;output.kind=JsonKind::Object;SkipWhitespace();if(position_<text_.size()&&text_[position_]=='}'){++position_;return true;}std::unordered_set<std::string> keys;
            while(true){SkipWhitespace();std::string key;if(!String(key,error))return false;if(!keys.insert(key).second){SetError(error,PhotorealAssetErrorCode3D::InvalidJson,position_,"Duplicate JSON key");return false;}SkipWhitespace();if(position_>=text_.size()||text_[position_++]!=':'){SetError(error,PhotorealAssetErrorCode3D::InvalidJson,position_,"Expected object colon");return false;}output.object.emplace_back(std::move(key),JsonValue{});if(!Value(output.object.back().second,depth+1,error))return false;SkipWhitespace();if(position_>=text_.size()){SetError(error,PhotorealAssetErrorCode3D::InvalidJson,position_,"Unterminated object");return false;}const char separator=text_[position_++];if(separator=='}')return true;if(separator!=','){SetError(error,PhotorealAssetErrorCode3D::InvalidJson,position_-1,"Expected object separator");return false;}}
        }
        SetError(error,PhotorealAssetErrorCode3D::InvalidJson,position_,"Invalid JSON token");return false;
    }
    std::string_view text_;
    std::size_t maximumStringBytes_{};
    std::size_t position_{};
    std::size_t nodes_{};
};

const JsonValue* Field(const JsonValue& object,std::string_view name){
    if(object.kind!=JsonKind::Object)return nullptr;
    const auto found=std::find_if(object.object.begin(),object.object.end(),[&](const auto& item){return item.first==name;});
    return found==object.object.end()?nullptr:&found->second;
}
bool GetString(const JsonValue& object,std::string_view name,std::string& output){const JsonValue* value=Field(object,name);if(!value||value->kind!=JsonKind::String)return false;output=value->string;return true;}
bool GetNumber(const JsonValue& object,std::string_view name,std::uint64_t& output){const JsonValue* value=Field(object,name);if(!value||value->kind!=JsonKind::Number||!value->integerNumber)return false;output=value->number;return true;}
bool GetBoolean(const JsonValue& object,std::string_view name,bool& output){const JsonValue* value=Field(object,name);if(!value||value->kind!=JsonKind::Boolean)return false;output=value->boolean;return true;}

std::string Lower(std::string value){for(char& character:value)if(character>='A'&&character<='Z')character=char(character-'A'+'a');return value;}
bool ValidIdentifier(std::string_view value){if(value.empty()||value.size()>128)return false;for(unsigned char character:value)if(!(character>='a'&&character<='z'||character>='A'&&character<='Z'||character>='0'&&character<='9'||character=='-'||character=='_'||character=='.'))return false;return true;}
bool ValidSha256(std::string_view value){if(value.size()!=64)return false;for(char character:value)if(!(character>='0'&&character<='9'||character>='a'&&character<='f'||character>='A'&&character<='F'))return false;return true;}
bool ValidRuntimeAssetPath(std::string_view value){
    if(value.empty()||value.size()>4096||value.front()=='/'||value.front()=='\\'||value.find(':')!=std::string_view::npos)return false;
    std::size_t segmentStart{};for(std::size_t index=0;index<=value.size();++index){
        if(index<value.size()&&static_cast<unsigned char>(value[index])<0x20)return false;
        if(index==value.size()||value[index]=='/'||value[index]=='\\'){
            const std::string_view segment=value.substr(segmentStart,index-segmentStart);if(segment.empty()||segment=="."||segment=="..")return false;segmentStart=index+1;
        }
    }
    return true;
}
std::string UrlHost(std::string_view value){
    constexpr std::string_view prefix="https://";if(value.substr(0,prefix.size())!=prefix)return{};value.remove_prefix(prefix.size());const std::size_t end=value.find_first_of("/:?#");std::string host=Lower(std::string(value.substr(0,end)));if(host.empty()||host.find('@')!=std::string::npos)return{};return host;
}
bool HostMatches(std::string_view host,std::string_view official){return host==official||host.size()>official.size()&&host.ends_with(official)&&host[host.size()-official.size()-1]=='.';}
bool HostsShareOfficialRoot(std::string_view left,std::string_view right){return HostMatches(left,right)||HostMatches(right,left);}
PhotorealAssetKind3D ParseKind(std::string_view kind){if(kind=="pbr-material")return PhotorealAssetKind3D::PbrMaterial;if(kind=="mesh-pbr")return PhotorealAssetKind3D::MeshPbr;if(kind=="hdri")return PhotorealAssetKind3D::Hdri;return PhotorealAssetKind3D::Unknown;}
PhotorealMaterialChannel3D ParseChannel(std::string value){
    value=Lower(std::move(value));if(value=="base-color"||value=="color"||value=="diffuse")return PhotorealMaterialChannel3D::BaseColor;
    if(value=="normal-dx"||value=="normaldx")return PhotorealMaterialChannel3D::NormalDx;if(value=="normal-gl"||value=="normalgl")return PhotorealMaterialChannel3D::NormalGl;
    if(value=="roughness")return PhotorealMaterialChannel3D::Roughness;if(value=="metallic")return PhotorealMaterialChannel3D::Metallic;if(value=="ambient-occlusion"||value=="ao")return PhotorealMaterialChannel3D::AmbientOcclusion;
    if(value=="displacement"||value=="height")return PhotorealMaterialChannel3D::Displacement;if(value=="emissive")return PhotorealMaterialChannel3D::Emissive;if(value=="opacity"||value=="alpha")return PhotorealMaterialChannel3D::Opacity;if(value=="arm"||value=="orm"||value=="packed-arm"||value=="packed-orm")return PhotorealMaterialChannel3D::PackedArm;return PhotorealMaterialChannel3D::None;
}
bool ParseReceiptObject(const JsonValue& input,PhotorealImportReceipt3D& output,PhotorealAssetError3D* error){
    PhotorealImportReceipt3D receipt{};std::string result;
    if(!GetString(input,"assetId",receipt.assetId)||!GetString(input,"source",receipt.source)||!GetString(input,"sha256",receipt.sha256)||!GetNumber(input,"bytes",receipt.bytes)||!GetNumber(input,"uncompressedBytes",receipt.uncompressedBytes)||!GetString(input,"result",result)){
        SetError(error,PhotorealAssetErrorCode3D::InvalidReceipt,0,"Import receipt is incomplete");return false;
    }
    if(!ValidIdentifier(receipt.assetId)||!ValidIdentifier(receipt.source)||!ValidSha256(receipt.sha256)||!receipt.bytes||!receipt.uncompressedBytes||receipt.uncompressedBytes>64ull*1024ull*1024ull*1024ull){SetError(error,PhotorealAssetErrorCode3D::InvalidReceipt,0,"Import receipt fields are invalid");return false;}
    const JsonValue* maps=Field(input,"maps");if(maps){if(maps->kind!=JsonKind::Array){SetError(error,PhotorealAssetErrorCode3D::InvalidReceipt,0,"Import receipt maps must be an array");return false;}for(const JsonValue& map:maps->array){if(map.kind!=JsonKind::String){SetError(error,PhotorealAssetErrorCode3D::InvalidReceipt,0,"Import receipt map name is invalid");return false;}receipt.channels=receipt.channels|ParseChannel(map.string);}}
    receipt.passed=result=="pass";if(!receipt.passed){SetError(error,PhotorealAssetErrorCode3D::InvalidReceipt,0,"Import receipt did not pass");return false;}output=std::move(receipt);return true;
}
bool ReadBounded(const std::filesystem::path& path,std::size_t maximumBytes,std::string& output,PhotorealAssetError3D* error){
    std::error_code ec;const std::uintmax_t bytes=std::filesystem::file_size(path,ec);if(ec){SetError(error,PhotorealAssetErrorCode3D::IoFailure,0,"Unable to query JSON file");return false;}if(bytes>maximumBytes){SetError(error,PhotorealAssetErrorCode3D::InputLimitExceeded,0,"JSON file exceeds limit");return false;}
    std::ifstream stream(path,std::ios::binary);if(!stream){SetError(error,PhotorealAssetErrorCode3D::IoFailure,0,"Unable to open JSON file");return false;}try{output.resize(static_cast<std::size_t>(bytes));}catch(const std::bad_alloc&){SetError(error,PhotorealAssetErrorCode3D::InputLimitExceeded,0,"Unable to allocate bounded JSON buffer");return false;}if(bytes&&(!stream.read(output.data(),static_cast<std::streamsize>(bytes))||stream.gcount()!=static_cast<std::streamsize>(bytes))){SetError(error,PhotorealAssetErrorCode3D::IoFailure,0,"Unable to read complete JSON file");return false;}char extra{};if(stream.get(extra)){SetError(error,PhotorealAssetErrorCode3D::InputLimitExceeded,0,"JSON file changed while reading");return false;}return true;
}
void HashBytes(std::uint64_t& hash,std::string_view value){for(unsigned char byte:value){hash^=byte;hash*=1099511628211ull;}hash^=0xff;hash*=1099511628211ull;}
void HashNumber(std::uint64_t& hash,std::uint64_t value){for(unsigned index=0;index<8;++index){hash^=std::uint8_t(value>>(index*8));hash*=1099511628211ull;}}
std::uint32_t MipCount(std::uint32_t dimension){std::uint32_t count=1;while(dimension>1){dimension>>=1;++count;}return count;}
std::uint32_t TierDimension(PhotorealTextureTier3D tier){switch(tier){case PhotorealTextureTier3D::Far512:return 512;case PhotorealTextureTier3D::General1K:return 1024;case PhotorealTextureTier3D::Near2K:return 2048;case PhotorealTextureTier3D::Hero4K:return 4096;}return 1024;}
std::uint64_t SaturatingAdd(std::uint64_t left,std::uint64_t right){return right>(std::numeric_limits<std::uint64_t>::max)()-left?(std::numeric_limits<std::uint64_t>::max)():left+right;}

}

void PhotorealAssetCatalog3D::Clear()noexcept{assets_.clear();preflight_.reset();stats_={};}

bool PhotorealAssetCatalog3D::LoadFile(const std::filesystem::path& path,PhotorealAssetError3D* error,const PhotorealCatalogLimits3D& limits){
    if(limits.maximumJsonBytes>MaximumCatalogJsonBytes3D){SetError(error,PhotorealAssetErrorCode3D::InputLimitExceeded,0,"Catalog file limit exceeds hard cap");return false;}std::string json;if(!ReadBounded(path,limits.maximumJsonBytes,json,error))return false;return LoadJson(json,error,limits);
}

bool PhotorealAssetCatalog3D::LoadJson(std::string_view json,PhotorealAssetError3D* error,const PhotorealCatalogLimits3D& limits){
    if(error)*error={};if(limits.maximumJsonBytes>MaximumCatalogJsonBytes3D||json.size()>limits.maximumJsonBytes||!limits.maximumAssets||!limits.maximumStringBytes||!limits.maximumDownloadBudgetBytes){SetError(error,PhotorealAssetErrorCode3D::InputLimitExceeded,0,"Catalog limits rejected input");return false;}
    JsonValue root;if(!JsonParser(json,limits.maximumStringBytes).Parse(root,error))return false;if(root.kind!=JsonKind::Object){SetError(error,PhotorealAssetErrorCode3D::InvalidSchema,0,"Catalog root must be an object");return false;}
    std::uint64_t schema{},downloadBudget{};if(!GetNumber(root,"schemaVersion",schema)||schema!=1||!GetNumber(root,"downloadBudgetBytes",downloadBudget)||!downloadBudget||downloadBudget>limits.maximumDownloadBudgetBytes){SetError(error,PhotorealAssetErrorCode3D::InvalidSchema,0,"Unsupported catalog schema or budget");return false;}
    const JsonValue* sources=Field(root,"sources");const JsonValue* assets=Field(root,"assets");if(!sources||sources->kind!=JsonKind::Object||!assets||assets->kind!=JsonKind::Array||assets->array.empty()||assets->array.size()>limits.maximumAssets){SetError(error,PhotorealAssetErrorCode3D::InvalidSchema,0,"Catalog sources or assets are invalid");return false;}
    struct SourcePolicy{std::string id;std::string host;};std::vector<SourcePolicy> sourcePolicies;sourcePolicies.reserve(sources->object.size());
    for(const auto& [id,value]:sources->object){std::string license,licenseUrl;if(!ValidIdentifier(id)||!GetString(value,"license",license)||license!="CC0-1.0"||!GetString(value,"licenseUrl",licenseUrl)){SetError(error,PhotorealAssetErrorCode3D::LicenseRejected,0,"Catalog source is not explicitly CC0-1.0");return false;}const std::string host=UrlHost(licenseUrl);if(host.empty()){SetError(error,PhotorealAssetErrorCode3D::InvalidCatalog,0,"Catalog source license URL is invalid");return false;}sourcePolicies.push_back({id,host});}
    std::vector<PhotorealCatalogAsset3D> parsed;parsed.reserve(assets->array.size());std::unordered_set<std::string> identifiers;std::uint64_t firstBatchBytes{};std::size_t firstBatchCount{};
    for(const JsonValue& value:assets->array){
        PhotorealCatalogAsset3D asset{};std::string kind;if(value.kind!=JsonKind::Object||!GetString(value,"id",asset.id)||!GetString(value,"source",asset.source)||!GetString(value,"category",asset.category)||!GetString(value,"kind",kind)||!GetString(value,"officialPage",asset.officialPage)||!GetBoolean(value,"firstBatch",asset.firstBatch)||!GetString(value,"lodAtlas",asset.optimizationHint)){
            SetError(error,PhotorealAssetErrorCode3D::InvalidCatalog,0,"Catalog asset is incomplete");return false;
        }
        asset.kind=ParseKind(kind);const JsonValue* preferred=Field(value,"preferred");if(!preferred||!GetString(*preferred,"resolution",asset.preferredResolution)||!GetString(*preferred,"format",asset.preferredFormat)||!GetNumber(*preferred,"estimatedBytes",asset.estimatedBytes)||!asset.estimatedBytes||asset.kind==PhotorealAssetKind3D::Unknown||!ValidIdentifier(asset.id)||!ValidIdentifier(asset.source)||!ValidIdentifier(asset.category)){
            SetError(error,PhotorealAssetErrorCode3D::InvalidCatalog,0,"Catalog asset metadata is invalid");return false;
        }
        const auto policy=std::find_if(sourcePolicies.begin(),sourcePolicies.end(),[&](const SourcePolicy& item){return item.id==asset.source;});const std::string pageHost=UrlHost(asset.officialPage);if(policy==sourcePolicies.end()||pageHost.empty()||!HostsShareOfficialRoot(pageHost,policy->host)){SetError(error,PhotorealAssetErrorCode3D::LicenseRejected,0,"Asset page does not match its official CC0 source");return false;}
        if(!identifiers.insert(asset.id).second){SetError(error,PhotorealAssetErrorCode3D::DuplicateAsset,0,"Catalog contains a duplicate asset identifier");return false;}
        const std::string hint=Lower(asset.optimizationHint);asset.atlasRecommended=hint.find("atlas")!=std::string::npos;asset.trimSheetRecommended=hint.find("trim")!=std::string::npos;
        if(asset.firstBatch){++firstBatchCount;firstBatchBytes=SaturatingAdd(firstBatchBytes,asset.estimatedBytes);if(firstBatchBytes>downloadBudget){SetError(error,PhotorealAssetErrorCode3D::InvalidCatalog,0,"First batch exceeds download budget");return false;}}
        parsed.push_back(std::move(asset));
    }
    std::sort(parsed.begin(),parsed.end(),[](const auto& left,const auto& right){return left.id<right.id;});std::sort(sourcePolicies.begin(),sourcePolicies.end(),[](const auto& left,const auto& right){return left.id<right.id;});std::uint64_t fingerprint=1469598103934665603ull;HashNumber(fingerprint,schema);HashNumber(fingerprint,downloadBudget);for(const auto& source:sourcePolicies){HashBytes(fingerprint,source.id);HashBytes(fingerprint,source.host);}
    for(const auto& asset:parsed){HashBytes(fingerprint,asset.id);HashBytes(fingerprint,asset.source);HashBytes(fingerprint,asset.category);HashNumber(fingerprint,static_cast<std::uint64_t>(asset.kind));HashBytes(fingerprint,asset.officialPage);HashBytes(fingerprint,asset.preferredResolution);HashBytes(fingerprint,asset.preferredFormat);HashNumber(fingerprint,asset.estimatedBytes);HashNumber(fingerprint,asset.firstBatch);HashBytes(fingerprint,asset.optimizationHint);}
    std::optional<PhotorealImportReceipt3D> preflight;if(const JsonValue* value=Field(root,"preflight")){PhotorealImportReceipt3D receipt;if(!ParseReceiptObject(*value,receipt,error))return false;preflight=std::move(receipt);}
    assets_=std::move(parsed);preflight_=std::move(preflight);stats_={assets_.size(),firstBatchCount,firstBatchBytes,downloadBudget,fingerprint?fingerprint:1};return true;
}

bool PhotorealAssetCatalog3D::LoadReceiptFile(const std::filesystem::path& path,PhotorealImportReceipt3D& output,PhotorealAssetError3D* error,std::size_t maximumJsonBytes){if(maximumJsonBytes>MaximumReceiptJsonBytes3D){SetError(error,PhotorealAssetErrorCode3D::InputLimitExceeded,0,"Receipt file limit exceeds hard cap");return false;}std::string json;if(!ReadBounded(path,maximumJsonBytes,json,error))return false;return LoadReceiptJson(json,output,error,maximumJsonBytes);}
bool PhotorealAssetCatalog3D::LoadReceiptJson(std::string_view json,PhotorealImportReceipt3D& output,PhotorealAssetError3D* error,std::size_t maximumJsonBytes){
    if(error)*error={};if(maximumJsonBytes>MaximumReceiptJsonBytes3D||json.size()>maximumJsonBytes){SetError(error,PhotorealAssetErrorCode3D::InputLimitExceeded,0,"Receipt JSON exceeds limit");return false;}JsonValue root;if(!JsonParser(json,4096).Parse(root,error))return false;if(const JsonValue* nested=Field(root,"preflight"))return ParseReceiptObject(*nested,output,error);return ParseReceiptObject(root,output,error);
}
const PhotorealCatalogAsset3D* PhotorealAssetCatalog3D::Find(std::string_view id)const noexcept{const auto found=std::lower_bound(assets_.begin(),assets_.end(),id,[](const auto& asset,std::string_view value){return asset.id<value;});return found!=assets_.end()&&found->id==id?&*found:nullptr;}

void PhotorealRuntimeManifest3D::Clear()noexcept{assets_.clear();stats_={};}
const PhotorealRuntimeAssetReceipt3D* PhotorealRuntimeManifest3D::Find(std::string_view id)const noexcept{const auto found=std::lower_bound(assets_.begin(),assets_.end(),id,[](const auto& asset,std::string_view value){return asset.id<value;});return found!=assets_.end()&&found->id==id?&*found:nullptr;}
bool PhotorealRuntimeManifest3D::LoadFile(const std::filesystem::path& path,PhotorealAssetError3D* error,std::size_t maximumJsonBytes){if(maximumJsonBytes>MaximumCatalogJsonBytes3D){SetError(error,PhotorealAssetErrorCode3D::InputLimitExceeded,0,"Runtime manifest file limit exceeds hard cap");return false;}std::string json;if(!ReadBounded(path,maximumJsonBytes,json,error))return false;return LoadJson(json,error,maximumJsonBytes);}
bool PhotorealRuntimeManifest3D::LoadJson(std::string_view json,PhotorealAssetError3D* error,std::size_t maximumJsonBytes){
    if(error)*error={};if(maximumJsonBytes>MaximumCatalogJsonBytes3D||json.size()>maximumJsonBytes){SetError(error,PhotorealAssetErrorCode3D::InputLimitExceeded,0,"Runtime manifest exceeds limit");return false;}JsonValue root;if(!JsonParser(json,4096).Parse(root,error))return false;std::uint64_t schema{},declaredCount{},declaredBytes{};std::string globalMemoryGate;const JsonValue* assets=Field(root,"assets");if(root.kind!=JsonKind::Object||!GetNumber(root,"schemaVersion",schema)||schema!=1||!GetNumber(root,"assetCount",declaredCount)||!GetNumber(root,"runtimeBytes",declaredBytes)||!GetString(root,"memoryGate",globalMemoryGate)||!assets||assets->kind!=JsonKind::Array||declaredCount!=assets->array.size()||!declaredCount||declaredCount>4096){SetError(error,PhotorealAssetErrorCode3D::InvalidSchema,0,"Runtime manifest schema is invalid");return false;}
    std::vector<PhotorealRuntimeAssetReceipt3D> parsed;parsed.reserve(assets->array.size());std::unordered_set<std::string> identifiers;std::uint64_t computedBytes{};bool allAssetsPassed=true;
    for(const JsonValue& value:assets->array){
        PhotorealRuntimeAssetReceipt3D asset{};std::string kind,license,memoryGate;bool commercial{};if(value.kind!=JsonKind::Object||!GetString(value,"id",asset.id)||!GetString(value,"source",asset.source)||!GetString(value,"kind",kind)||!GetString(value,"license",license)||!GetBoolean(value,"commercialUse",commercial)||!ValidIdentifier(asset.id)||!ValidIdentifier(asset.source)||!identifiers.insert(asset.id).second){SetError(error,PhotorealAssetErrorCode3D::RuntimeAssetIncomplete,0,"Runtime asset identity is invalid or duplicated");return false;}asset.kind=ParseKind(kind);asset.licensePassed=license=="CC0-1.0"&&commercial;asset.memoryGatePassed=GetString(value,"memoryGate",memoryGate)&&memoryGate=="pass";
        const JsonValue* files=Field(value,"files");asset.hashesPassed=files&&files->kind==JsonKind::Array&&!files->array.empty();if(asset.hashesPassed){for(const JsonValue& file:files->array){std::string hash,path;std::uint64_t bytes{};if(file.kind!=JsonKind::Object||!GetString(file,"sha256",hash)||!GetString(file,"path",path)||!GetNumber(file,"bytes",bytes)||!bytes||!ValidSha256(hash)||!ValidRuntimeAssetPath(path)){asset.hashesPassed=false;break;}asset.acceptedSha256.push_back(Lower(hash));asset.runtimeBytes=SaturatingAdd(asset.runtimeBytes,bytes);const std::string lowerPath=Lower(path);if(lowerPath.find("color")!=std::string::npos||lowerPath.find("diff")!=std::string::npos)asset.channels=asset.channels|PhotorealMaterialChannel3D::BaseColor;if(lowerPath.find("normaldx")!=std::string::npos||lowerPath.find("nor_dx")!=std::string::npos)asset.channels=asset.channels|PhotorealMaterialChannel3D::NormalDx;if(lowerPath.find("normalgl")!=std::string::npos||lowerPath.find("nor_gl")!=std::string::npos)asset.channels=asset.channels|PhotorealMaterialChannel3D::NormalGl;if(lowerPath.find("rough")!=std::string::npos)asset.channels=asset.channels|PhotorealMaterialChannel3D::Roughness;if(lowerPath.find("_arm")!=std::string::npos||lowerPath.find("_orm")!=std::string::npos)asset.channels=asset.channels|PhotorealMaterialChannel3D::PackedArm;}}
        if(const JsonValue* archive=Field(value,"archive");archive&&archive->kind==JsonKind::Object){std::string hash;std::uint64_t bytes{};if(GetString(*archive,"sha256",hash)&&GetNumber(*archive,"bytes",bytes)&&bytes&&ValidSha256(hash))asset.acceptedSha256.push_back(Lower(hash));else asset.hashesPassed=false;}
        if(const JsonValue* channels=Field(value,"materialChannels")){if(channels->kind!=JsonKind::Array)asset.channels=PhotorealMaterialChannel3D::None;else for(const JsonValue& channel:channels->array)if(channel.kind==JsonKind::String)asset.channels=asset.channels|ParseChannel(channel.string);}
        if(asset.kind==PhotorealAssetKind3D::Hdri){asset.channelsPassed=true;asset.lodReceiptPassed=true;}else{const bool color=static_cast<bool>(asset.channels&PhotorealMaterialChannel3D::BaseColor),normal=static_cast<bool>(asset.channels&PhotorealMaterialChannel3D::NormalDx)||static_cast<bool>(asset.channels&PhotorealMaterialChannel3D::NormalGl),surface=static_cast<bool>(asset.channels&PhotorealMaterialChannel3D::Roughness)||static_cast<bool>(asset.channels&PhotorealMaterialChannel3D::PackedArm);asset.channelsPassed=color&&normal&&surface;asset.lodReceiptPassed=asset.kind==PhotorealAssetKind3D::PbrMaterial;}
        if(asset.kind==PhotorealAssetKind3D::MeshPbr){const JsonValue* levels=Field(value,"lodLevels");asset.lodReceiptPassed=levels&&levels->kind==JsonKind::Array&&levels->array.size()>=3;std::uint64_t previousTriangles=(std::numeric_limits<std::uint64_t>::max)();if(asset.lodReceiptPassed){for(std::size_t index=0;index<levels->array.size();++index){std::uint64_t level{},triangles{},bytes{};std::string hash;if(!GetNumber(levels->array[index],"level",level)||level!=index||!GetNumber(levels->array[index],"triangles",triangles)||!triangles||triangles>=previousTriangles||!GetNumber(levels->array[index],"bytes",bytes)||!bytes||!GetString(levels->array[index],"sha256",hash)||!ValidSha256(hash)){asset.lodReceiptPassed=false;break;}previousTriangles=triangles;}asset.lodLevels=static_cast<std::uint32_t>(levels->array.size());}}
        if(asset.kind==PhotorealAssetKind3D::Unknown)asset.channelsPassed=asset.lodReceiptPassed=false;const bool passed=asset.licensePassed&&asset.memoryGatePassed&&asset.hashesPassed&&asset.channelsPassed&&asset.lodReceiptPassed;allAssetsPassed&=passed;computedBytes=SaturatingAdd(computedBytes,asset.runtimeBytes);parsed.push_back(std::move(asset));
    }
    if(declaredBytes<computedBytes||declaredBytes-computedBytes>16u*1024u*1024u){SetError(error,PhotorealAssetErrorCode3D::RuntimeAssetIncomplete,0,"Runtime byte receipt is smaller than files or has excessive unlisted overhead");return false;}std::sort(parsed.begin(),parsed.end(),[](const auto& left,const auto& right){return left.id<right.id;});std::uint64_t fingerprint=1469598103934665603ull;HashNumber(fingerprint,schema);HashNumber(fingerprint,declaredBytes);HashBytes(fingerprint,globalMemoryGate);for(const auto& asset:parsed){HashBytes(fingerprint,asset.id);HashBytes(fingerprint,asset.source);HashNumber(fingerprint,static_cast<std::uint64_t>(asset.kind));HashNumber(fingerprint,asset.runtimeBytes);HashNumber(fingerprint,asset.licensePassed);HashNumber(fingerprint,asset.memoryGatePassed);HashNumber(fingerprint,asset.hashesPassed);HashNumber(fingerprint,asset.channelsPassed);HashNumber(fingerprint,asset.lodReceiptPassed);for(const auto& hash:asset.acceptedSha256)HashBytes(fingerprint,hash);}
    assets_=std::move(parsed);stats_={assets_.size(),declaredBytes,fingerprint?fingerprint:1,globalMemoryGate=="pass",allAssetsPassed};return true;
}

bool PhotorealAssetProfile3D::Initialize(const PhotorealAssetCatalog3D& catalog,const PhotorealAssetProfileSettings3D& settings,PhotorealAssetError3D* error){
    if(error)*error={};if(catalog.Assets().empty()||!catalog.Stats().fingerprint||!settings.cpuBudgetBytes||!settings.gpuBudgetBytes||!settings.maximumAnisotropy||settings.maximumAnisotropy>16||settings.maximumTextureDimension<512||settings.maximumTextureDimension>4096||(settings.maximumTextureDimension&(settings.maximumTextureDimension-1))||!settings.maximumNearMeshTriangles||!settings.maximumFarMeshTriangles||settings.maximumFarMeshTriangles>settings.maximumNearMeshTriangles){SetError(error,PhotorealAssetErrorCode3D::InvalidCatalog,0,"Profile settings or catalog are invalid");return false;}
    PhotorealAssetCatalog3D snapshot;try{snapshot=catalog;}catch(const std::bad_alloc&){SetError(error,PhotorealAssetErrorCode3D::InputLimitExceeded,0,"Unable to snapshot catalog");return false;}catalog_=std::move(snapshot);runtimeManifest_.Clear();settings_=settings;resident_.clear();stats_={};catalogFingerprint_=catalog.Stats().fingerprint;sequence_=0;initialized_=true;runtimeManifestBound_=false;return true;
}

bool PhotorealAssetProfile3D::Initialize(const PhotorealAssetCatalog3D& catalog,const PhotorealRuntimeManifest3D& runtimeManifest,const PhotorealAssetProfileSettings3D& settings,PhotorealAssetError3D* error){
    initialized_=false;runtimeManifestBound_=false;resident_.clear();stats_={};if(!Initialize(catalog,settings,error))return false;if(!runtimeManifest.Passed()){initialized_=false;SetError(error,PhotorealAssetErrorCode3D::RuntimeManifestRejected,0,"Runtime manifest did not pass memory and asset gates");return false;}if(runtimeManifest.Stats().assetCount<catalog.Stats().firstBatchCount){initialized_=false;SetError(error,PhotorealAssetErrorCode3D::RuntimeManifestRejected,0,"Runtime manifest does not cover the complete first batch");return false;}for(const auto& runtime:runtimeManifest.Assets()){const auto* asset=catalog.Find(runtime.id);if(!asset||runtime.source!=asset->source||runtime.kind!=asset->kind){initialized_=false;SetError(error,PhotorealAssetErrorCode3D::RuntimeManifestRejected,0,"Runtime asset does not match catalog");return false;}}for(const auto& asset:catalog.Assets())if(asset.firstBatch){if(!runtimeManifest.Find(asset.id)){initialized_=false;SetError(error,PhotorealAssetErrorCode3D::RuntimeManifestRejected,0,"Runtime asset does not match catalog first batch");return false;}}
    try{runtimeManifest_=runtimeManifest;}catch(const std::bad_alloc&){initialized_=false;SetError(error,PhotorealAssetErrorCode3D::InputLimitExceeded,0,"Unable to snapshot runtime manifest");return false;}runtimeManifestBound_=true;return true;
}

PhotorealAssetSelection3D PhotorealAssetProfile3D::Select(const PhotorealCatalogAsset3D& asset,const PhotorealAssetObservation3D& observation)const noexcept{
    const float distance=std::isfinite(observation.distanceMeters)&&observation.distanceMeters>=0.f?observation.distanceMeters:(std::numeric_limits<float>::infinity)();const float coverage=std::clamp(std::isfinite(observation.projectedCoverage)?observation.projectedCoverage:0.f,0.f,1.f);
    PhotorealTextureTier3D tier=distance<=8.f||coverage>=.25f?PhotorealTextureTier3D::Near2K:distance<=35.f||coverage>=.05f?PhotorealTextureTier3D::General1K:PhotorealTextureTier3D::Far512;
    if(observation.importance==PhotorealImportance3D::Hero&&settings_.ultraDevelopmentQuality&&(distance<=12.f||coverage>=.15f))tier=PhotorealTextureTier3D::Hero4K;
    else if(observation.importance==PhotorealImportance3D::Important&&static_cast<unsigned>(tier)<static_cast<unsigned>(PhotorealTextureTier3D::Near2K)&&distance<=30.f)tier=PhotorealTextureTier3D::Near2K;
    else if(observation.importance==PhotorealImportance3D::Background&&static_cast<unsigned>(tier)>static_cast<unsigned>(PhotorealTextureTier3D::General1K))tier=PhotorealTextureTier3D::General1K;
    std::uint32_t dimension=(std::min)(TierDimension(tier),settings_.maximumTextureDimension);if(dimension<=512)tier=PhotorealTextureTier3D::Far512;else if(dimension<=1024)tier=PhotorealTextureTier3D::General1K;else if(dimension<=2048)tier=PhotorealTextureTier3D::Near2K;else tier=PhotorealTextureTier3D::Hero4K;
    PhotorealAssetSelection3D selection{};selection.textureTier=tier;selection.textureDimension=dimension;selection.firstResidentMip=0;for(std::uint32_t maximum=settings_.maximumTextureDimension;maximum>dimension;maximum>>=1)++selection.firstResidentMip;selection.residentMipCount=MipCount(dimension);
    const std::array<std::uint32_t,4> anisotropy{4,8,16,16};selection.anisotropy=(std::min)(anisotropy[static_cast<unsigned>(tier)],settings_.maximumAnisotropy);selection.meshLod=tier==PhotorealTextureTier3D::Hero4K?0:tier==PhotorealTextureTier3D::Near2K?1:tier==PhotorealTextureTier3D::General1K?2:3;
    switch(selection.meshLod){case 0:selection.triangleBudget=settings_.maximumNearMeshTriangles;break;case 1:selection.triangleBudget=(std::max)(settings_.maximumFarMeshTriangles,settings_.maximumNearMeshTriangles/2);break;case 2:selection.triangleBudget=(std::max)(settings_.maximumFarMeshTriangles,settings_.maximumNearMeshTriangles/6);break;default:selection.triangleBudget=settings_.maximumFarMeshTriangles;break;}
    if(asset.kind!=PhotorealAssetKind3D::MeshPbr){selection.meshLod=0;selection.triangleBudget=0;}const std::uint64_t pixels=std::uint64_t(dimension)*dimension;std::uint64_t textureBytes=asset.kind==PhotorealAssetKind3D::Hdri?pixels*8/6:pixels*10/3;std::uint64_t meshBytes=asset.kind==PhotorealAssetKind3D::MeshPbr?std::uint64_t(selection.triangleBudget)*56:0;selection.estimatedGpuBytes=SaturatingAdd(textureBytes,meshBytes);return selection;
}

bool PhotorealAssetProfile3D::ValidateMaterialChannels(const PhotorealCatalogAsset3D& asset,const PhotorealImportReceipt3D& receipt,PhotorealAssetError3D* error){
    if(asset.kind==PhotorealAssetKind3D::Hdri)return true;const auto channels=receipt.channels;const bool color=static_cast<bool>(channels&PhotorealMaterialChannel3D::BaseColor);const bool normal=static_cast<bool>(channels&PhotorealMaterialChannel3D::NormalDx)||static_cast<bool>(channels&PhotorealMaterialChannel3D::NormalGl);const bool surface=static_cast<bool>(channels&PhotorealMaterialChannel3D::Roughness)||static_cast<bool>(channels&PhotorealMaterialChannel3D::PackedArm);if(color&&normal&&surface)return true;SetError(error,PhotorealAssetErrorCode3D::MaterialChannelsIncomplete,0,"PBR receipt requires base color, normal and roughness or packed ARM");return false;
}

bool PhotorealAssetProfile3D::Admit(const PhotorealAssetResidencyRequest3D& request,PhotorealAssetResidencyReceipt3D& receipt,PhotorealAssetError3D* error){
    if(error)*error={};receipt={};if(!initialized_){SetError(error,PhotorealAssetErrorCode3D::InvalidCatalog,0,"Profile is not initialized");return false;}if(settings_.requireRuntimeManifest&&!runtimeManifestBound_){++stats_.rejectedLoads;SetError(error,PhotorealAssetErrorCode3D::RuntimeManifestRejected,0,"Runtime manifest is required before admission");return false;}const PhotorealCatalogAsset3D* asset=catalog_.Find(request.assetId);if(!asset){++stats_.rejectedLoads;SetError(error,PhotorealAssetErrorCode3D::AssetNotFound,0,"Catalog asset was not found");return false;}if(IsResident(request.assetId)){++stats_.rejectedLoads;SetError(error,PhotorealAssetErrorCode3D::AlreadyResident,0,"Asset is already resident");return false;}
    const PhotorealRuntimeAssetReceipt3D* runtime=runtimeManifestBound_?runtimeManifest_.Find(asset->id):nullptr;if(runtimeManifestBound_&&(!runtime||!runtime->licensePassed||!runtime->memoryGatePassed||!runtime->hashesPassed||!runtime->channelsPassed||!runtime->lodReceiptPassed)){++stats_.rejectedLoads;SetError(error,PhotorealAssetErrorCode3D::RuntimeAssetIncomplete,0,"Runtime asset receipt is incomplete");return false;}if(!request.receipt.passed||request.receipt.assetId!=asset->id||request.receipt.source!=asset->source||!ValidSha256(request.receipt.sha256)||!request.receipt.bytes||!request.cpuBytes||!request.gpuBytes||(runtime&&std::find(runtime->acceptedSha256.begin(),runtime->acceptedSha256.end(),Lower(request.receipt.sha256))==runtime->acceptedSha256.end())){++stats_.rejectedLoads;SetError(error,PhotorealAssetErrorCode3D::ReceiptMismatch,0,"Import receipt does not match catalog and runtime asset");return false;}if(!ValidateMaterialChannels(*asset,request.receipt,error)){++stats_.rejectedLoads;return false;}const auto& selection=request.selection;if(selection.textureDimension<512||selection.textureDimension>settings_.maximumTextureDimension||(selection.textureDimension&(selection.textureDimension-1))||!selection.anisotropy||selection.anisotropy>settings_.maximumAnisotropy||selection.residentMipCount!=MipCount(selection.textureDimension)||selection.firstResidentMip+selection.residentMipCount!=MipCount(settings_.maximumTextureDimension)||(asset->kind==PhotorealAssetKind3D::MeshPbr?(selection.meshLod>3||!selection.triangleBudget):(selection.meshLod||selection.triangleBudget))){++stats_.rejectedLoads;SetError(error,PhotorealAssetErrorCode3D::InvalidSelection,0,"Residency selection is incompatible with profile or asset kind");return false;}
    if(request.cpuBytes>settings_.cpuBudgetBytes-stats_.residentCpuBytes){++stats_.rejectedLoads;SetError(error,PhotorealAssetErrorCode3D::CpuBudgetExceeded,0,"CPU residency budget exceeded");return false;}if(request.gpuBytes>settings_.gpuBudgetBytes-stats_.residentGpuBytes){++stats_.rejectedLoads;SetError(error,PhotorealAssetErrorCode3D::GpuBudgetExceeded,0,"GPU residency budget exceeded");return false;}
    if(sequence_==(std::numeric_limits<std::uint64_t>::max)()){++stats_.rejectedLoads;SetError(error,PhotorealAssetErrorCode3D::InputLimitExceeded,0,"Residency sequence exhausted");return false;}const std::uint64_t nextSequence=sequence_+1;try{ResidentAsset3D value{asset->id,request.importance,request.cpuBytes,request.gpuBytes,nextSequence,request.selection.meshLod,request.selection.textureDimension};PhotorealAssetResidencyReceipt3D committed{nextSequence,catalogFingerprint_,asset->id,request.cpuBytes,request.gpuBytes,request.selection.meshLod,request.selection.textureDimension,true,false};const auto position=std::lower_bound(resident_.begin(),resident_.end(),value.id,[](const auto& item,std::string_view id){return item.id<id;});resident_.insert(position,std::move(value));sequence_=nextSequence;stats_.residentAssets=resident_.size();stats_.residentCpuBytes+=request.cpuBytes;stats_.residentGpuBytes+=request.gpuBytes;stats_.peakCpuBytes=(std::max)(stats_.peakCpuBytes,stats_.residentCpuBytes);stats_.peakGpuBytes=(std::max)(stats_.peakGpuBytes,stats_.residentGpuBytes);++stats_.loadCount;receipt=std::move(committed);return true;}catch(const std::bad_alloc&){++stats_.rejectedLoads;SetError(error,PhotorealAssetErrorCode3D::InputLimitExceeded,0,"Unable to allocate atomic residency record");return false;}
}

bool PhotorealAssetProfile3D::Touch(std::string_view assetId,std::uint64_t frameIndex)noexcept{const auto found=std::lower_bound(resident_.begin(),resident_.end(),assetId,[](const auto& item,std::string_view id){return item.id<id;});if(found==resident_.end()||found->id!=assetId)return false;found->lastTouchedFrame=(std::max)(found->lastTouchedFrame,frameIndex);return true;}
bool PhotorealAssetProfile3D::IsResident(std::string_view assetId)const noexcept{const auto found=std::lower_bound(resident_.begin(),resident_.end(),assetId,[](const auto& item,std::string_view id){return item.id<id;});return found!=resident_.end()&&found->id==assetId;}

bool PhotorealAssetProfile3D::Unload(std::string_view assetId,PhotorealAssetResidencyReceipt3D& receipt,PhotorealAssetError3D* error){
    if(error)*error={};receipt={};const auto found=std::lower_bound(resident_.begin(),resident_.end(),assetId,[](const auto& item,std::string_view id){return item.id<id;});if(found==resident_.end()||found->id!=assetId){SetError(error,PhotorealAssetErrorCode3D::AssetNotFound,0,"Resident asset was not found");return false;}const ResidentAsset3D value=*found;resident_.erase(found);stats_.residentAssets=resident_.size();stats_.residentCpuBytes-=value.cpuBytes;stats_.residentGpuBytes-=value.gpuBytes;++stats_.unloadCount;receipt={++sequence_,catalogFingerprint_,value.id,value.cpuBytes,value.gpuBytes,value.meshLod,value.textureDimension,false,true};return true;
}

PhotorealAssetTrimReceipt3D PhotorealAssetProfile3D::Trim(std::uint64_t targetCpuBytes,std::uint64_t targetGpuBytes,std::span<const std::string_view> protectedAssetIds){
    PhotorealAssetTrimReceipt3D receipt{};receipt.catalogFingerprint=catalogFingerprint_;receipt.cpuBytesBefore=stats_.residentCpuBytes;receipt.gpuBytesBefore=stats_.residentGpuBytes;targetCpuBytes=(std::min)(targetCpuBytes,settings_.cpuBudgetBytes);targetGpuBytes=(std::min)(targetGpuBytes,settings_.gpuBudgetBytes);
    std::vector<std::string> candidates;for(const auto& item:resident_)if(std::find(protectedAssetIds.begin(),protectedAssetIds.end(),item.id)==protectedAssetIds.end())candidates.push_back(item.id);std::sort(candidates.begin(),candidates.end(),[&](const std::string& left,const std::string& right){const auto* a=&*std::lower_bound(resident_.begin(),resident_.end(),left,[](const auto& item,std::string_view id){return item.id<id;});const auto* b=&*std::lower_bound(resident_.begin(),resident_.end(),right,[](const auto& item,std::string_view id){return item.id<id;});if(a->importance!=b->importance)return a->importance<b->importance;if(a->lastTouchedFrame!=b->lastTouchedFrame)return a->lastTouchedFrame<b->lastTouchedFrame;return a->id<b->id;});
    for(const std::string& id:candidates){if(stats_.residentCpuBytes<=targetCpuBytes&&stats_.residentGpuBytes<=targetGpuBytes)break;PhotorealAssetResidencyReceipt3D unloaded;if(Unload(id,unloaded,nullptr))receipt.unloadedAssetIds.push_back(id);}
    receipt.cpuBytesAfter=stats_.residentCpuBytes;receipt.gpuBytesAfter=stats_.residentGpuBytes;receipt.releasedCpuBytes=receipt.cpuBytesBefore-receipt.cpuBytesAfter;receipt.releasedGpuBytes=receipt.gpuBytesBefore-receipt.gpuBytesAfter;receipt.sequence=++sequence_;++stats_.trimCount;return receipt;
}

}
