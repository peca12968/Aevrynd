// 三份真 DDS 的 CPU 內容、結構及 store 生命週期；不修改來源檔，不代表 GPU 或森林美術通過。
#include "MistwatchForestIbl.h"
#include <Aevrynd/PinnedAssetRead3D.h>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>

namespace {
using namespace Aevrynd;
namespace F=Mistwatch::ForestIbl;
unsigned checks{},negatives{},policyNegatives{},hashNegatives{},subresources{};
void Need(bool good,const char* reason){++checks;if(!good)throw std::runtime_error(reason);}
template<class Work>void Throws(Work&& work,std::string_view reason){
    bool caught{};try{work();}catch(const std::invalid_argument& error){
        Need(std::string_view(error.what())==reason,"FOREST_IBL_TEST_WRONG_REASON");caught=true;
    }Need(caught,"FOREST_IBL_TEST_NEGATIVE_ACCEPTED");++negatives;
}
std::vector<std::byte> Read(const std::filesystem::path& path,std::size_t expected){
    std::ifstream file(path,std::ios::binary|std::ios::ate);Need(bool(file),"FOREST_IBL_TEST_SOURCE_MISSING");
    Need(file.tellg()==static_cast<std::streamoff>(expected),"FOREST_IBL_TEST_SOURCE_SIZE");file.seekg(0);
    std::vector<std::byte> bytes(expected);file.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(expected));
    Need(bool(file)&&file.peek()==std::char_traits<char>::eof(),"FOREST_IBL_TEST_SOURCE_READ");return bytes;
}
void Policy(){
    for(bool capture:{false,true})for(bool replay:{false,true}){
        Need(!F::Resolve(capture,replay,L"")&&!F::Resolve(capture,replay,L"original"),"FOREST_IBL_TEST_ORIGINAL_POLICY");
        if(capture!=replay)Need(F::Resolve(capture,replay,F::Candidate),"FOREST_IBL_TEST_CANDIDATE_POLICY");
        else{Throws([&]{(void)F::Resolve(capture,replay,F::Candidate);},"FOREST_IBL_ISOLATED_CONTEXT");++policyNegatives;}
        const auto bad=[&](std::wstring_view value){Throws([&]{(void)F::Resolve(capture,replay,value);},"FOREST_IBL_PROFILE");++policyNegatives;};
        for(const auto value:{L" ",L"\t",L"\n",L"original ",L" original",L"Original",L"ORIGINAL",L"xanderklinge-forest-ibl-v2",L"xanderklinge-forest-ibl-v1 ",L" xanderklinge-forest-ibl-v1"})bad(value);
        const std::wstring excessive(97,L'x');bad(excessive);
        const std::wstring embedded=std::wstring(F::Candidate)+L'\0';bad(std::wstring_view(embedded.data(),embedded.size()));
        for(std::size_t i=0;i<F::Candidate.size();++i){auto altered=std::wstring(F::Candidate);altered[i]=L'#';bad(altered);}
    }
    Need(policyNegatives>=70,"FOREST_IBL_TEST_POLICY_COVERAGE");
}
void Contents(const std::array<std::vector<std::byte>,3>& bytes){
    std::array<std::span<const std::byte>,3> originals{bytes[0],bytes[1],bytes[2]};Need(F::ValidatePayloads(originals),"FOREST_IBL_TEST_CONTENT_POSITIVE");
    const auto rejected=[&](const std::array<std::span<const std::byte>,3>& changed){Need(!F::ValidatePayloads(changed),"FOREST_IBL_TEST_CONTENT_NEGATIVE_ACCEPTED");++negatives;};
    for(unsigned source=0;source<3;++source){
        for(unsigned sample=0;sample<31;++sample){auto changed=bytes[source];const auto index=std::size_t(sample)*(changed.size()-1)/30;
            changed[index]^=std::byte(1u<<(sample%8));auto views=originals;views[source]=changed;rejected(views);++hashNegatives;}
        for(auto length:{std::size_t{0},std::size_t{1},std::size_t{147},bytes[source].size()-1}){auto views=originals;views[source]=std::span(bytes[source]).first(length);rejected(views);}
        auto appended=bytes[source];appended.push_back(std::byte{});auto views=originals;views[source]=appended;rejected(views);
        for(unsigned other=0;other<3;++other)if(source!=other){views=originals;views[source]=originals[other];rejected(views);}
    }
    for(unsigned a=0;a<3;++a)for(unsigned b=a+1;b<3;++b){auto swapped=originals;std::swap(swapped[a],swapped[b]);rejected(swapped);}
    Need(hashNegatives==93,"FOREST_IBL_TEST_HASH_COVERAGE");
}
void Texture(const EnvironmentIblTextureData3D& data,const F::Spec& spec){
    Need(data.Valid()&&data.kind==spec.kind&&data.width==spec.width&&data.height==spec.height&&data.mipCount==spec.mips&&data.faceCount==spec.faces,
        "FOREST_IBL_TEST_DDS_METADATA");
    Need(data.fileBytes.size()==spec.bytes&&F::ExactHash(data.fileBytes,spec.sha256),"FOREST_IBL_TEST_PARSED_SOURCE_HASH");
    Need(data.subresources.size()==std::size_t(spec.mips)*spec.faces,"FOREST_IBL_TEST_SUBRESOURCE_COUNT");
    std::size_t offset=148,total{};unsigned index{};
    for(unsigned face=0;face<spec.faces;++face)for(unsigned mip=0;mip<spec.mips;++mip){
        const auto& row=data.subresources[index++];const auto width=(std::max)(1u,spec.width>>mip),height=(std::max)(1u,spec.height>>mip);
        const auto expectedBytes=spec.kind==EnvironmentIblTextureKind3D::BrdfLut?std::size_t(width)*height*4:std::size_t((width+3)/4)*((height+3)/4)*16;
        Need(row.face==face&&row.mip==mip&&row.width==width&&row.height==height&&row.offset==offset&&row.bytes==expectedBytes,"FOREST_IBL_TEST_SUBRESOURCE_METADATA");
        Need(offset<=data.fileBytes.size()&&row.bytes<=data.fileBytes.size()-offset,"FOREST_IBL_TEST_SUBRESOURCE_BOUNDS");offset+=row.bytes;total+=row.bytes;++subresources;
    }
    Need(offset==spec.bytes&&total==data.PayloadBytes()&&total==spec.bytes-148,"FOREST_IBL_TEST_PAYLOAD_BYTES");
}
void Parsed(const EnvironmentIblValidatedData3D& data){
    Texture(data.specular,F::Specs[0]);Texture(data.irradiance,F::Specs[1]);Texture(data.brdf,F::Specs[2]);
    Need(data.payloadBytes==205120&&data.rotationRadians==0,"FOREST_IBL_TEST_SET_METADATA");
    Need(F::ValidatePayloads({data.specular.fileBytes,data.irradiance.fileBytes,data.brdf.fileBytes}),"FOREST_IBL_TEST_PARSED_PAYLOAD_IDENTITY");
}
void Empty(const EnvironmentLightingMemoryStats3D& stats){
    Need(stats.residentInstances==0&&stats.residentReferences==0&&stats.payloadSets==0&&stats.payloadBytes==0&&stats.sourceSets==0&&
        stats.instanceCacheEntries==0&&stats.payloadCacheEntries==0&&stats.rawSourceEntries==0&&stats.canonicalSourceEntries==0&&stats.residentSets==0&&stats.residentBytes==0,
        "FOREST_IBL_TEST_RESIDENT_LEAK");
}
void Store(const EnvironmentIblDefinition3D& definition){
    EnvironmentLightingStore3D store;const auto baseline=store.MemoryStats();Empty(baseline);
    Need(!store.Active()&&!store.Resolve({}),"FOREST_IBL_TEST_EMPTY_STORE");
    const auto first=store.Create(definition);Need(bool(first),"FOREST_IBL_TEST_STORE_CREATE");auto lease=store.Resolve(first);
    Need(bool(lease)&&lease.rotationRadians==0,"FOREST_IBL_TEST_STORE_RESOLVE");Parsed(*lease.payload);
    Need(store.Set(first)&&store.Active()==first,"FOREST_IBL_TEST_STORE_SET");
    const auto second=store.Create(definition);Need(second==first,"FOREST_IBL_TEST_STORE_REUSE");auto shared=store.Resolve(second);
    Need(bool(shared)&&shared.payload==lease.payload,"FOREST_IBL_TEST_SHARED_PAYLOAD");
    const auto resident=store.MemoryStats();Need(resident.residentInstances==1&&resident.residentReferences==2&&resident.payloadSets==1&&resident.payloadBytes==205120&&
        resident.sourceSets==1&&resident.cacheHits>=1,"FOREST_IBL_TEST_SHARED_MEMORY");
    store.Destroy(first);Need(bool(store.Resolve(second))&&store.Active()==second&&store.MemoryStats().residentReferences==1,"FOREST_IBL_TEST_FIRST_RELEASE");
    store.Destroy(second);Need(!store.Resolve(first)&&!store.Active()&&!store.Set(first),"FOREST_IBL_TEST_STALE_HANDLE");Empty(store.MemoryStats());
    // lease 的擁有者是測試本身；先歸還外部引用，再檢查 store trim 不殘留 payload 或 metadata。
    lease={};shared={};store.Destroy(first);Empty(store.MemoryStats());store.Trim();const auto trimmed=store.MemoryStats();Empty(trimmed);
    Need(trimmed.metadataBytes==baseline.metadataBytes&&trimmed.reclaimableBytes==baseline.reclaimableBytes&&trimmed.retiredSlots==baseline.retiredSlots&&
        trimmed.trimCount==baseline.trimCount+1,"FOREST_IBL_TEST_TRIM_BASELINE");
    store.Shutdown();Empty(store.MemoryStats());
}
}
int main(int argc,char** argv){try{
    Need(argc==3&&std::string_view(argv[1])=="--runtime-assets","FOREST_IBL_TEST_ARGUMENTS");const std::filesystem::path root(argv[2]);
    const auto definition=F::Definition(root);Policy();
    Throws([&]{(void)F::Definition({});},"FOREST_IBL_ROOT");Throws([&]{(void)F::Definition("relative/assets");},"FOREST_IBL_ROOT");
    Throws([&]{(void)F::Definition(root/".."/"assets");},"FOREST_IBL_ROOT");Throws([&]{(void)F::Definition(root/".");},"FOREST_IBL_ROOT");
    const auto rootWithNul=std::filesystem::path(root.native()+L'\0');Throws([&]{(void)F::Definition(rootWithNul);},"FOREST_IBL_ROOT");
    Need(definition.specularCubePath==root/std::filesystem::path(F::Specs[0].relativePath)&&definition.irradianceCubePath==root/std::filesystem::path(F::Specs[1].relativePath)&&
        definition.brdfLutPath==root/std::filesystem::path(F::Specs[2].relativePath)&&definition.rotationRadians==0,"FOREST_IBL_TEST_DEFINITION_PATHS");
    std::array<std::vector<std::byte>,3> bytes;for(unsigned i=0;i<3;++i){
        const auto path=root/std::filesystem::path(F::Specs[i].relativePath);bytes[i]=Read(path,F::Specs[i].bytes);
        PinnedAssetRead3D locked(path,F::Specs[i].bytes);
        Need(locked.Bytes().size()==bytes[i].size()&&std::equal(locked.Bytes().begin(),locked.Bytes().end(),bytes[i].begin()),"FOREST_IBL_TEST_PINNED_BYTES");
        // 只要求開啟寫入權限，不寫任何位元組；同檔鎖存活時必須拒絕寫入/刪除分享。
        const auto writer=CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        const bool denied=writer==INVALID_HANDLE_VALUE;if(!denied)CloseHandle(writer);
        Need(denied,"FOREST_IBL_TEST_SOURCE_WRITE_NOT_DENIED");
    }Contents(bytes);
    Throws([&]{PinnedAssetRead3D invalid("relative.dds",1);},"PINNED_ASSET_INPUT");
    Throws([&]{PinnedAssetRead3D invalid(definition.brdfLutPath,0);},"PINNED_ASSET_INPUT");
    Throws([&]{PinnedAssetRead3D invalid(definition.brdfLutPath,16u*1024u*1024u+1u);},"PINNED_ASSET_INPUT");
    EnvironmentIblValidatedData3D parsed;Need(LoadAndValidateEnvironmentIbl3D(definition,parsed),"FOREST_IBL_TEST_DDS_PARSE");Parsed(parsed);
    auto invalid=definition;invalid.rotationRadians=std::numeric_limits<float>::quiet_NaN();const auto preserved=parsed.payloadBytes;
    Need(!LoadAndValidateEnvironmentIbl3D(invalid,parsed)&&parsed.payloadBytes==preserved,"FOREST_IBL_TEST_INVALID_ROTATION_ATOMIC");++negatives;
    invalid=definition;std::swap(invalid.specularCubePath,invalid.brdfLutPath);
    Need(!LoadAndValidateEnvironmentIbl3D(invalid,parsed)&&parsed.payloadBytes==preserved,"FOREST_IBL_TEST_WRONG_KIND_ATOMIC");++negatives;
    Store(definition);
    std::cout<<"FOREST_IBL_TEST_PASS checks="<<checks<<" negatives="<<negatives<<" policy_negatives="<<policyNegatives<<" hash_negatives="<<hashNegatives
        <<" validated_subresources="<<subresources<<" payload_bytes=205120 source_files=3 GPU_verified=0 art_accepted=0\n";return 0;
}catch(const std::exception& error){std::cerr<<"FOREST_IBL_TEST_FAIL "<<error.what()<<" checks="<<checks<<" negatives="<<negatives<<'\n';return 1;}}
