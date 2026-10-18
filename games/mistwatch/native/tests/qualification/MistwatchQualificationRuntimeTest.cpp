#include "../fallen/MistwatchQualificationRuntime.h"
#include <iostream>
#include <functional>
#include <utility>

#ifndef MISTWATCH_QUALIFICATION_SOURCE_ROOT
#error MISTWATCH_QUALIFICATION_SOURCE_ROOT must bind the current source root
#endif

namespace {
using namespace Mistwatch::QualificationRuntime;
std::size_t controls{};
void Reject(const std::function<void()>& operation,std::string_view label) {
    bool rejected=false;
    try { operation(); } catch(const std::exception&) { rejected=true; }
    Require(rejected,"negative_control_accepted: "+std::string(label));
    ++controls;
}
std::vector<FileIdentity> Fixture(Profile profile) {
    std::vector<FileIdentity> result;
    for(const auto* e:Expected(profile))result.push_back({std::string(e->path),std::string(e->sha256),e->bytes});
    return result;
}
// 四個舊部署的完整映射採修改前獨立快照；遮罩擴充不能偷改既有內容。
std::string MappingHash(Profile profile) {
    std::string text;
    for(const auto* row:Expected(profile)) {
        text+=row->path;text+='\n';text+=row->source;text+='\n';
        text+=KindName(row->kind);text+='\n';text+=std::to_string(row->bytes);text+='\n';
        text+=row->sha256;text+='\n';
    }
    return Sha256(std::as_bytes(std::span(text.data(),text.size())));
}
bool SameMapping(const Entry& a,const Entry& b) {
    return a.path==b.path && a.source==b.source && a.kind==b.kind &&
        a.bytes==b.bytes && a.sha256==b.sha256;
}
const Entry* FindMapping(const std::vector<const Entry*>& rows,std::string_view path) {
    const auto found=std::find_if(rows.begin(),rows.end(),[&](const Entry* row){return row->path==path;});
    return found==rows.end()?nullptr:*found;
}
bool OriginalEightGroups(std::string_view path) {
    for(const std::string_view group:{"mistwatch-boulders/","mistwatch-broadleaf/","mistwatch-canopy-preview/",
            "mistwatch-ferns/","mistwatch-forest/","mistwatch-grass/","mistwatch-materials/","mistwatch-wildlife/"})
        if(path.starts_with(group))return true;
    return false;
}
void BoulderProfileTests() {
    const std::array<std::pair<Profile,std::string_view>,4> oldPins{{
        {Profile::LegacyFullV1,"18CF63F7D8C4B8FF7FA4D64C947D2BAE92FAEC514CB90B6B53CA1A4F8B81E5D4"},
        {Profile::HabitatFullV1,"C3D87C7A04919343A0B0CB08C1E8F4919D3C662EE387C4EBC02BA8E233D52789"},
        {Profile::AuthoredNettleFullV1,"C6BD6213E9B6BDC0AF3A85181A72CECA65DAEDDADF792D8C5B3540C65E4BEDDA"},
        {Profile::AuthoredMistwatchOnlyV1,"18B6E8AA2FA3DA3827B59FE7CA20AF5D1070DDD21CE7E5FF7A570EC2C69D742A"}
    }};
    for(std::size_t i=0;i<oldPins.size();++i) {
        const auto& [profile,sha]=oldPins[i];
        Require(ProfileMask(profile)==(1u<<static_cast<unsigned>(i)),"legacy_profile_bit_changed");
        Require(MappingHash(profile)==sha,"legacy_complete_mapping_changed");
    }
    // 新增兩檔的路徑、來源、位元組及雜湊獨立固定，不能用數量取代身分。
    const std::array<Entry,2> additions{{
        {"mistwatch-boulders/materials/slot-0-base-source-srgb-v1.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/boulder-01/slot-0-base-source-srgb-v1.dds",48u,Kind::Copy,1398276ull,"D874D5AC2E2C01CA7BD87970BA23415EF285E051B0A501C74E383E6219758B79"},
        {"mistwatch-boulders/meshes/boulder-01-lod0-node0-mesh0-primitive0-material0-boulder-01.aemesh","engine/assets/realistic-cc0-first-batch/cooked/meshes/boulder-01/boulder-01-lod0-node0-mesh0-primitive0-material0-boulder-01.aemesh",48u,Kind::Copy,3071924ull,"1005CCA1811D45922B9DADDCB1C6C6E155E09F569DF8EFE5DE093468B11C25D6"},
    }};
    const std::array<std::pair<Profile,Profile>,2> upgrades{{
        {Profile::AuthoredNettleFullV1,Profile::AuthoredBoulderFullV2},
        {Profile::AuthoredMistwatchOnlyV1,Profile::AuthoredBoulderMistwatchOnlyV2}
    }};
    Require(ProfileMask(Profile::AuthoredBoulderFullV2)==16u &&
        ProfileMask(Profile::AuthoredBoulderMistwatchOnlyV2)==32u,"boulder_profile_bits");
    for(const auto& [oldProfile,newProfile]:upgrades) {
        const auto previous=Expected(oldProfile),current=Expected(newProfile);
        Require(current.size()==previous.size()+additions.size(),"boulder_delta_count");
        for(const auto* oldRow:previous) {
            const auto* now=FindMapping(current,oldRow->path);
            Require(now && SameMapping(*now,*oldRow),"boulder_existing_mapping_changed");
        }
        std::size_t additionsSeen{};
        for(const auto* now:current) {
            if(FindMapping(previous,now->path))continue;
            const auto found=std::find_if(additions.begin(),additions.end(),
                [&](const Entry& row){return row.path==now->path;});
            Require(found!=additions.end() && SameMapping(*found,*now) && now->mask==48u,
                "boulder_unreviewed_addition");
            ++additionsSeen;
        }
        Require(additionsSeen==2,"boulder_exact_additions_missing");
        for(const auto& [legacyProfile,unused]:oldPins) {
            (void)unused;
            Reject([&]{ValidateIdentities(newProfile,Fixture(legacyProfile));},"old_fixture_in_new_profile");
            Reject([&]{ValidateIdentities(legacyProfile,Fixture(newProfile));},"new_fixture_in_old_profile");
        }
        for(const auto& added:additions) {
            auto modified=Fixture(newProfile);
            auto locate=[&] {
                const auto it=std::find_if(modified.begin(),modified.end(),
                    [&](const FileIdentity& row){return row.path==added.path;});
                Require(it!=modified.end(),"boulder_fixture_missing");return it;
            };
            modified.erase(locate());
            Reject([&]{ValidateIdentities(newProfile,modified);},"boulder_each_missing");
            modified=Fixture(newProfile);locate()->sha256=std::string(64,'F');
            Reject([&]{ValidateIdentities(newProfile,modified);},"boulder_each_hash");
            modified=Fixture(newProfile);++locate()->bytes;
            Reject([&]{ValidateIdentities(newProfile,modified);},"boulder_each_size");
            modified=Fixture(newProfile);locate()->path="mistwatch-boulders/unreviewed-replacement.bin";
            Reject([&]{ValidateIdentities(newProfile,modified);},"boulder_each_same_count_path");
        }
    }
    // 正式原八群是 133 原檔加五個落葉來源及兩個岩石檔，不能忽略額外檔。
    const std::array<Entry,7> eightGroupAdditions{{
        {"mistwatch-boulders/materials/slot-0-base-source-srgb-v1.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/boulder-01/slot-0-base-source-srgb-v1.dds",48u,Kind::Copy,1398276ull,"D874D5AC2E2C01CA7BD87970BA23415EF285E051B0A501C74E383E6219758B79"},
        {"mistwatch-boulders/meshes/boulder-01-lod0-node0-mesh0-primitive0-material0-boulder-01.aemesh","engine/assets/realistic-cc0-first-batch/cooked/meshes/boulder-01/boulder-01-lod0-node0-mesh0-primitive0-material0-boulder-01.aemesh",48u,Kind::Copy,3071924ull,"1005CCA1811D45922B9DADDCB1C6C6E155E09F569DF8EFE5DE093468B11C25D6"},
        {"mistwatch-materials/poly-haven/forest_leaves_04/2k/forest_leaves_04_diff_2k.png","engine/assets/material-packs/poly-haven/forest_leaves_04/2k/forest_leaves_04_diff_2k.png",14u,Kind::Copy,10402792ull,"40B1786AFA17C9930CCAE93A9CA75880FC52FC34B96453489C767B2CC82D7EE9"},
        {"mistwatch-materials/poly-haven/forest_leaves_04/2k/forest_leaves_04_nor_dx_2k.png","engine/assets/material-packs/poly-haven/forest_leaves_04/2k/forest_leaves_04_nor_dx_2k.png",14u,Kind::Copy,11539651ull,"19BBAA0EB67EDA146129946B9605CFC9B472A302BA1F75EB8EA0B48AA21BAA33"},
        {"mistwatch-materials/poly-haven/forest_leaves_04/2k/forest_leaves_04_rough_2k.png","engine/assets/material-packs/poly-haven/forest_leaves_04/2k/forest_leaves_04_rough_2k.png",14u,Kind::Copy,6083648ull,"713874257D143AF49C482CE8B7648CB7C5C748CA6540DB0E44563A04A9FE09CE"},
        {"mistwatch-materials/poly-haven/forest_leaves_04/DERIVATION.txt","engine/assets/material-packs/poly-haven/forest_leaves_04/DERIVATION.txt",14u,Kind::Copy,1389ull,"F6138A356ABB5A458A5E6C08B50E17361C6DDC07738F2066A4AF8936E4BE811C"},
        {"mistwatch-materials/poly-haven/forest_leaves_04/LICENSE.txt","engine/assets/material-packs/poly-haven/forest_leaves_04/LICENSE.txt",14u,Kind::Copy,695ull,"5E440DE55C40EB4DAD49DCC0505E7854267FB10688793E58EA5167CC29A5A719"},
    }};
    const auto original=Expected(Profile::LegacyFullV1),current=Expected(Profile::AuthoredBoulderFullV2);
    std::size_t originalEight{},currentEight{},deltaEight{};
    for(const auto* row:original)if(OriginalEightGroups(row->path))++originalEight;
    for(const auto* row:current) {
        if(!OriginalEightGroups(row->path))continue;
        ++currentEight;
        if(const auto* prior=FindMapping(original,row->path)) {
            Require(SameMapping(*row,*prior),"eight_group_original_mapping_changed");continue;
        }
        const auto found=std::find_if(eightGroupAdditions.begin(),eightGroupAdditions.end(),
            [&](const Entry& item){return item.path==row->path;});
        Require(found!=eightGroupAdditions.end() && SameMapping(*found,*row),"eight_group_unknown_delta");
        ++deltaEight;
    }
    Require(originalEight==133 && currentEight==140 && deltaEight==7,"eight_group_exact_133_plus_7");
}

void LeafClusterProfileTests() {
    const auto profile=Profile::AuthoredLeafClusterFullV3;
    Require(ProfileMask(profile)==64u,"leaf_profile_bit");
    Require(MappingHash(Profile::AuthoredBoulderFullV2)=="D1CB0D00DAF1853E1A3F8393B6D15610223931C6EC313555E110FD495E6093F3","boulder_full_mapping_changed");
    Require(MappingHash(Profile::AuthoredBoulderMistwatchOnlyV2)=="CA0B77C3165CF794905E14E233090AD478E7E910479C69308595EDA848219EB7","boulder_only_mapping_changed");
    const std::array<Entry,2> additions{{
        {"mistwatch-leaf-clusters/leaves-v1.aemesh","games/mistwatch/assets/leaf-clusters/leaves-v1.aemesh",64u,Kind::Copy,6377336ull,"23E72E7CE41CCA9567F2843EE8602B2D910B984F4D93D644199FD44E539E7767"},
        {"mistwatch-leaf-clusters/PROVENANCE.txt","games/mistwatch/assets/leaf-clusters/PROVENANCE.txt",64u,Kind::Copy,1790ull,"839D5C2A041D6D4E63A081DC65D3D47080B743FF734D8E01A4500E4C35B4D884"},
    }};
    const auto previous=Expected(Profile::AuthoredBoulderFullV2),current=Expected(profile);
    Require(current.size()==previous.size()+2,"leaf_exact_delta_count");
    for(const auto* row:previous) {
        const auto* now=FindMapping(current,row->path);
        Require(now && SameMapping(*row,*now),"leaf_inherited_mapping_changed");
    }
    std::size_t added{};
    for(const auto* row:current) {
        if(FindMapping(previous,row->path))continue;
        const auto match=std::find_if(additions.begin(),additions.end(),[&](const Entry& a){return SameMapping(*row,a);});
        Require(match!=additions.end() && row->mask==64u,"leaf_unreviewed_addition");++added;
    }
    Require(added==2,"leaf_exact_additions");
    for(const auto old:{Profile::LegacyFullV1,Profile::HabitatFullV1,Profile::AuthoredNettleFullV1,
            Profile::AuthoredMistwatchOnlyV1,Profile::AuthoredBoulderFullV2,Profile::AuthoredBoulderMistwatchOnlyV2}) {
        Reject([&]{ValidateIdentities(old,Fixture(profile));},"leaf_new_fixture_in_old");
        Reject([&]{ValidateIdentities(profile,Fixture(old));},"leaf_old_fixture_in_new");
    }
    for(const auto& entry:additions) {
        auto modified=Fixture(profile);
        const auto locate=[&]{const auto it=std::find_if(modified.begin(),modified.end(),[&](const auto& row){return row.path==entry.path;});Require(it!=modified.end(),"leaf_fixture_missing");return it;};
        modified.erase(locate());Reject([&]{ValidateIdentities(profile,modified);},"leaf_each_missing");
        modified=Fixture(profile);locate()->sha256=std::string(64,'F');Reject([&]{ValidateIdentities(profile,modified);},"leaf_each_hash");
        modified=Fixture(profile);++locate()->bytes;Reject([&]{ValidateIdentities(profile,modified);},"leaf_each_bytes");
        modified=Fixture(profile);locate()->path="mistwatch-leaf-clusters/replacement.bin";Reject([&]{ValidateIdentities(profile,modified);},"leaf_each_same_count_path");
        modified=Fixture(profile);locate()->path.front()='M';Reject([&]{ValidateIdentities(profile,modified);},"leaf_each_case");
    }
}
void ForestSurfaceProfileTests(){
    const auto profile=Profile::AuthoredForestSurfaceFullV4;
    Require(ProfileMask(profile)==128u,"surface_profile_bit");
    const std::array<Entry,2> additions{{
        {"mistwatch-materials/moss-height/Ground037_SurfaceOrmHeight_Level0.png","engine/assets/mistwatch-surface-height/moss-height/Ground037_SurfaceOrmHeight_Level0.png",128u,Kind::Copy,1531546ull,"012D2378AA6E62CCA05AD66D69B512966D5894694B7B24D39AFD0CA7CA655F38"},
        {"mistwatch-materials/leaf-height/ForestLeaves04_SurfaceOrmHeight_Level0.png","engine/assets/mistwatch-surface-height/leaf-height/ForestLeaves04_SurfaceOrmHeight_Level0.png",128u,Kind::Copy,9102955ull,"9F3259F4772F294EDCA1DE03FA4E8D7ED65C0924238B1A6D0B27B8FED7C0EF85"},
    }};
    const auto parent=Expected(Profile::AuthoredLeafClusterFullV3),current=Expected(profile);
    Require(parent.size()==416&&current.size()==418,"surface_exact_416_plus_2");
    for(const auto* row:parent){const auto* found=FindMapping(current,row->path);
        Require(found&&SameMapping(*row,*found)&&row->mask==found->mask,"surface_parent_mapping_changed");}
    std::size_t added{};
    for(const auto* row:current){if(FindMapping(parent,row->path))continue;
        const auto found=std::find_if(additions.begin(),additions.end(),[&](const auto& e){return SameMapping(e,*row)&&e.mask==row->mask;});
        Require(found!=additions.end(),"surface_unreviewed_addition");++added;}
    Require(added==2,"surface_exact_additions");
    for(const auto old:{Profile::LegacyFullV1,Profile::HabitatFullV1,Profile::AuthoredNettleFullV1,Profile::AuthoredMistwatchOnlyV1,
        Profile::AuthoredBoulderFullV2,Profile::AuthoredBoulderMistwatchOnlyV2,Profile::AuthoredLeafClusterFullV3}){
        Reject([&]{ValidateIdentities(old,Fixture(profile));},"surface_new_fixture_in_old");
        Reject([&]{ValidateIdentities(profile,Fixture(old));},"surface_old_fixture_in_new");}
    for(const auto& entry:additions){auto modified=Fixture(profile);
        const auto locate=[&]{const auto it=std::find_if(modified.begin(),modified.end(),[&](const auto& row){return row.path==entry.path;});
            Require(it!=modified.end(),"surface_fixture_missing");return it;};
        modified.erase(locate());Reject([&]{ValidateIdentities(profile,modified);},"surface_each_missing");
        modified=Fixture(profile);++locate()->bytes;Reject([&]{ValidateIdentities(profile,modified);},"surface_each_bytes");
        modified=Fixture(profile);locate()->sha256=std::string(64,'F');Reject([&]{ValidateIdentities(profile,modified);},"surface_each_sha");
        modified=Fixture(profile);locate()->path="mistwatch-materials/unreviewed-height.png";Reject([&]{ValidateIdentities(profile,modified);},"surface_same_count_path");
        modified=Fixture(profile);locate()->path.front()='M';Reject([&]{ValidateIdentities(profile,modified);},"surface_each_case");}
    for(const std::string_view invalid:{"authored-forest-surface-full-v3","AUTHORED-FOREST-SURFACE-FULL-V4",
        " authored-forest-surface-full-v4","authored-forest-surface-full-v4 ","authored-forest-surface-mistwatch-only-v4"})
        Reject([&]{(void)ParseProfile(invalid);},"surface_profile_literal");
}
void PureTests() {
    BoulderProfileTests();
    LeafClusterProfileTests();
    ForestSurfaceProfileTests();
    const auto current=Profile::AuthoredCurrentForestFullV5;
    Require(ProfileMask(current)==256u&&ProfileName(current)=="authored-current-forest-full-v5","current_profile_explicit_identity");
    Require(MappingHash(current)==MappingHash(Profile::AuthoredForestSurfaceFullV4),"current_profile_exact418_parent_mapping");
    // 部署同一418檔案並不判定世界身分；世界／配方必須由另一份明示證書嚴格核對。
    ValidateIdentities(current,Fixture(Profile::AuthoredForestSurfaceFullV4));
    ValidateIdentities(Profile::AuthoredForestSurfaceFullV4,Fixture(current));
    for(const std::string_view invalid:{"authored-current-forest-full-v4","AUTHORED-CURRENT-FOREST-FULL-V5",
        " authored-current-forest-full-v5","authored-current-forest-full-v5 ","authored-current-forest-only-v5"})
        Reject([&]{(void)ParseProfile(invalid);},"current_profile_literal");
    const std::array<std::pair<std::string_view,std::size_t>,9> profiles{{
        {"legacy-full-v1",387},{"habitat-full-v1",402},{"authored-nettle-full-v1",412},{"authored-mistwatch-only-v1",154},
        {"authored-boulder-full-v2",414},{"authored-boulder-mistwatch-only-v2",156},{"authored-leaf-cluster-full-v3",416},{"authored-forest-surface-full-v4",418},{"authored-current-forest-full-v5",418}}};
    for(const auto& [name,count]:profiles) {
        const auto profile=ParseProfile(name);Require(ProfileName(profile)==name && Expected(profile).size()==count,"profile_literal_or_membership");
        const auto fixture=Fixture(profile);ValidateIdentities(profile,fixture);
        auto modified=fixture;modified.pop_back();
        Reject([&]{ValidateIdentities(profile,modified);},"missing");
        modified=fixture;modified.push_back({"unreviewed.bin",std::string(64,'A'),12});
        Reject([&]{ValidateIdentities(profile,modified);},"extra");
        modified=fixture;modified.front().path="same-count-replacement.bin";
        Reject([&]{ValidateIdentities(profile,modified);},"same_count_replacement");
        modified=fixture;modified.front().path="../"+modified.front().path;
        Reject([&]{ValidateIdentities(profile,modified);},"path_escape");
        modified=fixture;modified.front().path="/"+modified.front().path;
        Reject([&]{ValidateIdentities(profile,modified);},"absolute");
        modified=fixture;modified.front().path="C:/file";
        Reject([&]{ValidateIdentities(profile,modified);},"drive");
        modified=fixture;modified.front().path=modified.back().path;
        Reject([&]{ValidateIdentities(profile,modified);},"duplicate");
        modified=fixture;modified.front().path=modified.back().path;
        modified.front().path.front()=static_cast<char>(std::toupper(static_cast<unsigned char>(modified.front().path.front())));
        Reject([&]{ValidateIdentities(profile,modified);},"case_shadow");
        modified=fixture;modified.front().sha256[0]=modified.front().sha256[0]=='0'?'1':'0';
        Reject([&]{ValidateIdentities(profile,modified);},"raw_hash");
        modified=fixture;++modified.front().bytes;
        Reject([&]{ValidateIdentities(profile,modified);},"raw_bytes");
        modified=fixture;modified.front().sha256.clear();
        Reject([&]{ValidateIdentities(profile,modified);},"empty_hash");
        modified=fixture;modified.front().path.front()=static_cast<char>(std::toupper(static_cast<unsigned char>(modified.front().path.front())));
        Reject([&]{ValidateIdentities(profile,modified);},"case_changed_exact_member");
        // 生成物不是例外空洞：每個 profile 都要拒絕三個生成物的個別內容漂移。
        for(const auto* entry:Expected(profile)) {
            if(entry->kind==Kind::Copy)continue;
            modified=fixture;
            const auto it=std::find_if(modified.begin(),modified.end(),[&](const auto& row){return row.path==entry->path;});
            Require(it!=modified.end(),"generated_fixture_missing");
            it->sha256=std::string(64,'F');
            Reject([&]{ValidateIdentities(profile,modified);},"generated_hash");
            modified=fixture;
            const auto byteIt=std::find_if(modified.begin(),modified.end(),[&](const auto& row){return row.path==entry->path;});
            ++byteIt->bytes;
            Reject([&]{ValidateIdentities(profile,modified);},"generated_bytes");
        }
    }
    for(const std::string_view invalid:{"","auto","387","legacy-full-v1387","authored-mistwatch-only-v1154","authored-mistwatch-only-v2","AUTHORED-MISTWATCH-ONLY-V1","authored-boulder-full-v1","authored-boulder-full-v2414","AUTHORED-BOULDER-FULL-V2"," authored-boulder-full-v2","authored-boulder-mistwatch-only-v2 "})
        Reject([&]{(void)ParseProfile(invalid);},"invalid_profile");
    Reject([]{(void)ProfileName(static_cast<Profile>(77));},"invalid_enum");
    for(const std::string_view invalid:{"authored-leaf-cluster-full-v2","AUTHORED-LEAF-CLUSTER-FULL-V3"," authored-leaf-cluster-full-v3","authored-leaf-cluster-full-v3 ","authored-leaf-cluster-mistwatch-only-v3"})
        Reject([&]{(void)ParseProfile(invalid);},"invalid_leaf_profile");
    Reject([]{ValidateIdentities(Profile::LegacyFullV1,Fixture(Profile::HabitatFullV1));},"wrong_profile");
    const std::filesystem::path source=std::filesystem::path("C:/source/input.aemesh").make_preferred();
    const std::filesystem::path output=std::filesystem::path("C:/fresh/generated/output.aemesh").make_preferred();
    const std::string receipt="{\n\"source\": \""+Escape(source.string())+"\",\n\"output\": \""+Escape(output.string())+"\",\n\"schema_version\": 1\n}";
    const auto normalized=NormalizeRootReceipt(receipt,source,output);
    Require(normalized.find("@SOURCE@")!=normalized.npos && normalized.find("@OUTPUT@")!=normalized.npos,"root_path_canonicalization");
    Reject([&]{(void)NormalizeRootReceipt(receipt,source/"wrong",output);},"wrong_generated_source");
    Reject([&]{(void)NormalizeRootReceipt(receipt,source,output/"wrong");},"wrong_generated_output");
    Reject([&]{(void)NormalizeRootReceipt(receipt+",\"source\": \"duplicate\"",source,output);},"duplicate_generated_source");
    Reject([&]{(void)ValidateRootMetadata(receipt,source,output);},"incomplete_root_metadata");
    ValidatePathAttributes(FILE_ATTRIBUTE_NORMAL);
    ValidatePathAttributes(FILE_ATTRIBUTE_DIRECTORY);
    Reject([]{ValidatePathAttributes(INVALID_FILE_ATTRIBUTES);},"missing_path_attributes");
    Reject([]{ValidatePathAttributes(FILE_ATTRIBUTE_REPARSE_POINT);},"file_reparse_attributes");
    Reject([]{ValidatePathAttributes(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT);},"directory_reparse_attributes");
    for(const auto& pin:ProducerInputs) {
        FileIdentity changed{std::string(pin.path),std::string(pin.sha256),pin.bytes};++changed.bytes;
        Reject([&]{Match(changed,pin.bytes,pin.sha256);},"producer_input_bytes");
        changed.bytes=pin.bytes;changed.sha256=std::string(64,'F');
        Reject([&]{Match(changed,pin.bytes,pin.sha256);},"producer_input_hash");
    }
    const std::string abc="abc";
    Require(Sha256(std::as_bytes(std::span(abc.data(),abc.size())))=="BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD","sha256_known_answer");
}
void CurrentMetadataControls(const std::filesystem::path& sourceRoot,const std::filesystem::path& runtime,const GeneratedEvidence& evidence) {
    auto generated=evidence.rootStemOutputDirectory;
    if(generated.empty())generated=runtime.parent_path().parent_path().parent_path()/"games/mistwatch/native/generated-root-stem";
    const auto source=sourceRoot/std::string(ProducerInputs[1].path);
    const auto output=generated/"island-tree-01-lod2-trunk-root-stem.aemesh";
    const auto bytes=Read(generated/"author-receipt.json");const auto original=Text(bytes);
    (void)ValidateRootMetadata(original,source,output);
    for(const std::string_view field:{"schema_version","source_vertices","output_vertices","output_triangles","source_sha256","output_sha256","author_sha256","status","operation","original_vertex_map","unchanged_source_faces","license"}) {
        auto modified=original;const std::string key="\""+std::string(field)+"\"";
        const auto at=modified.find(key);Require(at!=modified.npos,"current_generated_metadata_field_missing");
        modified[at+1]='X';
        Reject([&]{(void)ValidateRootMetadata(modified,source,output);},"current_metadata_field_drift");
    }
    auto extra=original;extra.insert(extra.size()-1,",\r\n  \"unreviewed\": 1\r\n");
    Reject([&]{(void)ValidateRootMetadata(extra,source,output);},"current_metadata_unknown_field");
    auto lf=original;lf.erase(std::remove(lf.begin(),lf.end(),'\r'),lf.end());
    Reject([&]{(void)ValidateRootMetadata(lf,source,output);},"current_metadata_format_drift");
    Reject([&]{(void)ValidateRootMetadata(original,source,output.parent_path()/"old-output.aemesh");},"current_metadata_wrong_output");
}
}
int main(int argc,char** argv) {
    try {
        PureTests();
        std::filesystem::path sourceRoot=MISTWATCH_QUALIFICATION_SOURCE_ROOT,runtime;
        Profile profile=Profile::AuthoredBoulderMistwatchOnlyV2;
        GeneratedEvidence generated;
        bool profileArgument=false;
        bool sourceArgument=false;
        for(int i=1;i<argc;i+=2) {
            Require(i+1<argc,"missing_option_value");
            const std::string_view option=argv[i];
            if(option=="--source-root"){Require(!sourceArgument,"duplicate_source_root");sourceRoot=argv[i+1];sourceArgument=true;}
            else if(option=="--runtime-assets"){Require(runtime.empty(),"duplicate_runtime_assets");runtime=argv[i+1];}
            else if(option=="--runtime-profile"){Require(!profileArgument,"duplicate_runtime_profile");profile=ParseProfile(argv[i+1]);profileArgument=true;}
            else if(option=="--generated-root-stem"){Require(generated.rootStemOutputDirectory.empty(),"duplicate_generated_root");generated.rootStemOutputDirectory=argv[i+1];}
            else throw std::runtime_error("unknown_option");
        }
        Require(std::filesystem::canonical(sourceRoot)==std::filesystem::canonical(std::filesystem::path(MISTWATCH_QUALIFICATION_SOURCE_ROOT)),"source_root_not_compiled_current_source");
        const auto manifest=Read(sourceRoot/"games/mistwatch/native/tests/qualification/runtime-profiles-v1.json");
        Require(Text(manifest)==ManifestText(),"manifest_header_byte_drift");
        const auto leafManifest=Read(sourceRoot/"games/mistwatch/native/tests/qualification/runtime-leaf-cluster-full-v3.json");
        Require(Text(leafManifest)==LeafClusterManifestText(),"leaf_manifest_header_byte_drift");
        const auto surfaceManifest=Read(sourceRoot/"games/mistwatch/native/tests/qualification/runtime-forest-surface-full-v4.json");
        Require(Text(surfaceManifest)==ForestSurfaceManifestText(),"surface_manifest_header_byte_drift");
        const auto currentManifest=Read(sourceRoot/"games/mistwatch/native/tests/qualification/runtime-current-forest-full-v5.json");
        Require(Text(currentManifest)==CurrentForestManifestText(),"current_manifest_header_byte_drift");
        std::size_t realCount{};
        Require(generated.rootStemOutputDirectory.empty() || !runtime.empty(),"generated_evidence_without_runtime");
        Require(runtime.empty() || profileArgument,"runtime_assets_requires_explicit_profile");
        if(!runtime.empty()) {
            realCount=Validate(sourceRoot,runtime,profile,generated).size();
            CurrentMetadataControls(sourceRoot,runtime,generated);
        }
        std::cout<<"MISTWATCH_RUNTIME_PROFILE_TEST_OK; schema=1; negative_controls="<<controls
            <<"; profile="<<ProfileName(profile)<<"; actual_files="<<realCount
            <<"; runtime_only=true; authored_world_qualified=false\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<"MISTWATCH_RUNTIME_PROFILE_TEST_FAILED: "<<error.what()<<'\n';return 1;
    }
}
