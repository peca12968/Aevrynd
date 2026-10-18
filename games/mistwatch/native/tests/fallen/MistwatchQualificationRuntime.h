#pragma once
// 部署身分與世界內容資格分開；本契約不宣稱作者世界已通過原生驗收。
#include "../../include/MistwatchBoar.h"
#include <Aevrynd/SkinnedModelAsset.h>
#include <windows.h>
#include <bcrypt.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <locale>
#include <limits>
#include <map>
#include <utility>
#include <sstream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#pragma comment(lib,"bcrypt.lib")

namespace Mistwatch::QualificationRuntime {
enum class Profile { LegacyFullV1, HabitatFullV1, AuthoredNettleFullV1, AuthoredMistwatchOnlyV1, AuthoredBoulderFullV2, AuthoredBoulderMistwatchOnlyV2, AuthoredLeafClusterFullV3, AuthoredForestSurfaceFullV4, AuthoredCurrentForestFullV5 };
enum class Kind { Copy, RootMesh, RootReceipt, Boar };
struct FileIdentity { std::string path,sha256; std::uint64_t bytes{}; };
struct GeneratedEvidence { std::filesystem::path rootStemOutputDirectory; };
struct Entry { std::string_view path,source; unsigned mask; Kind kind; std::uint64_t bytes; std::string_view sha256; };
struct ProducerInput { std::string_view path; std::uint64_t bytes; std::string_view sha256; };
inline constexpr Entry Entries[]{
    {"engine_probe.obj","games/engine_lab/assets/engine_probe.obj",23u,Kind::Copy,383ull,"953BC1BC172D20C7080F4C4732865E389850B69F649555327457C647FEAA8BC5"},
    {"environment-material-packs/LICENSE.md","engine/assets/material-packs/LICENSE.md",23u,Kind::Copy,865ull,"D81BC4A3F3B4FCC401182F7C307E601671E5A5E264EAEC47F1C7BBCA2C1E409D"},
    {"environment-material-packs/ambientcg/ground037/1k/Ground037.png","engine/assets/material-packs/ambientcg/ground037/1k/Ground037.png",23u,Kind::Copy,513329ull,"BC7CEBB2860A92E9446802A3018F56074879803F1BDDB0B80517CD5BFD57C6AE"},
    {"environment-material-packs/ambientcg/ground037/1k/Ground037_1K-JPG.blend","engine/assets/material-packs/ambientcg/ground037/1k/Ground037_1K-JPG.blend",23u,Kind::Copy,1109220ull,"980FEED3DFE7AADB2B2EF1A917E5AC6AFC5C92D96AD3152B43EB03018E554F85"},
    {"environment-material-packs/ambientcg/ground037/1k/Ground037_1K-JPG.mtlx","engine/assets/material-packs/ambientcg/ground037/1k/Ground037_1K-JPG.mtlx",23u,Kind::Copy,2307ull,"D00C32BAD302D4616D2BE47FB9CB34A97845F8B7609F13B29197CC448A61BFBF"},
    {"environment-material-packs/ambientcg/ground037/1k/Ground037_1K-JPG.tres","engine/assets/material-packs/ambientcg/ground037/1k/Ground037_1K-JPG.tres",23u,Kind::Copy,1039ull,"8F917AE493E419A7F6FF7ACEBEEEA239C270D17F5399B0D20C8A589187B80BAD"},
    {"environment-material-packs/ambientcg/ground037/1k/Ground037_1K-JPG.usdc","engine/assets/material-packs/ambientcg/ground037/1k/Ground037_1K-JPG.usdc",23u,Kind::Copy,2982ull,"3B3C481B29DFB3C862AD36CFEBC86C6AB0902A818262D67ABE52C6D91E9A2377"},
    {"environment-material-packs/ambientcg/ground037/1k/Ground037_1K-JPG_AmbientOcclusion.jpg","engine/assets/material-packs/ambientcg/ground037/1k/Ground037_1K-JPG_AmbientOcclusion.jpg",23u,Kind::Copy,612112ull,"8AFE189AE915888551076CB279F6282C84255E4B2BDB2C4CE8C1C891074245EF"},
    {"environment-material-packs/ambientcg/ground037/1k/Ground037_1K-JPG_Color.jpg","engine/assets/material-packs/ambientcg/ground037/1k/Ground037_1K-JPG_Color.jpg",23u,Kind::Copy,2187764ull,"D92BEC8A35B9CB16EC79B421CA0F572A7400286EAE109BEAFA266629EF49A6CF"},
    {"environment-material-packs/ambientcg/ground037/1k/Ground037_1K-JPG_Displacement.jpg","engine/assets/material-packs/ambientcg/ground037/1k/Ground037_1K-JPG_Displacement.jpg",23u,Kind::Copy,500481ull,"BC102168F5D014CE208ED5C1D9140C39513D29303E1B3436CB06C92C7780E419"},
    {"environment-material-packs/ambientcg/ground037/1k/Ground037_1K-JPG_NormalDX.jpg","engine/assets/material-packs/ambientcg/ground037/1k/Ground037_1K-JPG_NormalDX.jpg",23u,Kind::Copy,2474498ull,"0E2D613D117D9231AA0AA61F39B19755F891B4ED1D1599A004CB018A8224E039"},
    {"environment-material-packs/ambientcg/ground037/1k/Ground037_1K-JPG_NormalGL.jpg","engine/assets/material-packs/ambientcg/ground037/1k/Ground037_1K-JPG_NormalGL.jpg",23u,Kind::Copy,2474236ull,"EA59DBDCFA5BF2F28BDD4AA896D4E5F96C4CCE5939100DCC6DB4BBBCDEBFE38D"},
    {"environment-material-packs/ambientcg/ground037/1k/Ground037_1K-JPG_Roughness.jpg","engine/assets/material-packs/ambientcg/ground037/1k/Ground037_1K-JPG_Roughness.jpg",23u,Kind::Copy,694944ull,"3786B088E981AAE562F06B6477972D379CFFD09A5CC542AB42F9FFD94F9BCB0E"},
    {"environment-material-packs/ambientcg/woodsiding006/1k/WoodSiding006.png","engine/assets/material-packs/ambientcg/woodsiding006/1k/WoodSiding006.png",23u,Kind::Copy,304302ull,"B0F5C8FB01F5249F70574DAB2FE7E7AFA4CD90B0BEFF9F607C172A47FBB4F2CD"},
    {"environment-material-packs/ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG.blend","engine/assets/material-packs/ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG.blend",23u,Kind::Copy,1110260ull,"72925D4E752B1F2FA1A217AF2A77D02FDF53C89A4A177E4E8226BD9AEC8C2E39"},
    {"environment-material-packs/ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG.mtlx","engine/assets/material-packs/ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG.mtlx",23u,Kind::Copy,2367ull,"DC1AC65DBCA30FFCAEAED35C6DD45EEC6A5DD12C679CE13129E9623D7554948D"},
    {"environment-material-packs/ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG.tres","engine/assets/material-packs/ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG.tres",23u,Kind::Copy,1059ull,"C78565C440157D8E11707070A4F6BF33A1B2A1DA7FF7B49913D08444185BC721"},
    {"environment-material-packs/ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG.usdc","engine/assets/material-packs/ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG.usdc",23u,Kind::Copy,3050ull,"4C00FAA28CB21C2C299B1F8913A80356FE0F8C978DC14173F87562B96F3AF799"},
    {"environment-material-packs/ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG_AmbientOcclusion.jpg","engine/assets/material-packs/ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG_AmbientOcclusion.jpg",23u,Kind::Copy,127417ull,"61BDA7CF59ADD9C21A15C18328E4391BE3CFD8101986F058A5345D7BB39D3C60"},
    {"environment-material-packs/ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG_Color.jpg","engine/assets/material-packs/ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG_Color.jpg",23u,Kind::Copy,724730ull,"C626EC1B31055A85E8E09D42CE2D0C6F0B968250AD6672EA3282B136DD568D46"},
    {"environment-material-packs/ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG_Displacement.jpg","engine/assets/material-packs/ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG_Displacement.jpg",23u,Kind::Copy,285699ull,"51CF0B44D8A13AB05222C723AB06016B821DAC665CE7ECE64BDF150829EF3210"},
    {"environment-material-packs/ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG_NormalDX.jpg","engine/assets/material-packs/ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG_NormalDX.jpg",23u,Kind::Copy,1849769ull,"55895492F8B92A1D847201E76D339679097D0901F7B3FBF9BBCD1C740B05F3FB"},
    {"environment-material-packs/ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG_NormalGL.jpg","engine/assets/material-packs/ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG_NormalGL.jpg",23u,Kind::Copy,1847106ull,"C319E4180FC6261BF19753D597BC64189F9CD721DE4328FA4D67FFF8D1B71F32"},
    {"environment-material-packs/ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG_Roughness.jpg","engine/assets/material-packs/ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG_Roughness.jpg",23u,Kind::Copy,423700ull,"ADC51758DDAF97BCD795EEF154F705A4F020594EFC5A5B515D0F92B39240C844"},
    {"environment-material-packs/material-packs.json","engine/assets/material-packs/material-packs.json",23u,Kind::Copy,3758ull,"70853E6F97A1489C02A428A62295F556815C510D42948FCD33E9D33897773ED4"},
    {"environment-material-packs/poly-haven/aerial_ground_rock/1k/base-color.jpg","engine/assets/material-packs/poly-haven/aerial_ground_rock/1k/base-color.jpg",23u,Kind::Copy,599629ull,"C14761998B32630DD193B0FBFD31C8C6F5A144A701B84F87F9D7C33596CC554F"},
    {"environment-material-packs/poly-haven/aerial_ground_rock/1k/normal-dx.jpg","engine/assets/material-packs/poly-haven/aerial_ground_rock/1k/normal-dx.jpg",23u,Kind::Copy,601693ull,"1BCA894BD4DFADB3B06FD1AB1FB48441BF310EDC18418B20A52C08E67D1E15A7"},
    {"environment-material-packs/poly-haven/aerial_ground_rock/1k/roughness.jpg","engine/assets/material-packs/poly-haven/aerial_ground_rock/1k/roughness.jpg",23u,Kind::Copy,376876ull,"411D1FDA581C4D824C8510C6A31EAAD7C35DD3B6EA94F0227FE2C03DC0CC39CB"},
    {"environment-material-packs/poly-haven/bark_brown_02/1k/base-color.jpg","engine/assets/material-packs/poly-haven/bark_brown_02/1k/base-color.jpg",23u,Kind::Copy,659592ull,"920FA0BED0C9D78C1D530E99795113AFD532D7DB746DE440F4A85D7C83ED0F1A"},
    {"environment-material-packs/poly-haven/bark_brown_02/1k/normal-dx.jpg","engine/assets/material-packs/poly-haven/bark_brown_02/1k/normal-dx.jpg",23u,Kind::Copy,1033312ull,"7FCF9B0EFA59DEE597CC9F7A367E3F4131D59F72BC6D37966B9A8E69C4A1BF74"},
    {"environment-material-packs/poly-haven/bark_brown_02/1k/roughness.jpg","engine/assets/material-packs/poly-haven/bark_brown_02/1k/roughness.jpg",23u,Kind::Copy,337946ull,"68125592DE36E15BAE7AA6DB85C4D135AEB7E5653199BA226483A70F6D69837D"},
    {"environment-material-packs/poly-haven/brick_floor/1k/base-color.jpg","engine/assets/material-packs/poly-haven/brick_floor/1k/base-color.jpg",23u,Kind::Copy,469580ull,"9C783B294D5A3127D7C5F5E52DF67BBD418AEBC90EC05696DA5AAC2BBA834058"},
    {"environment-material-packs/poly-haven/brick_floor/1k/normal-dx.jpg","engine/assets/material-packs/poly-haven/brick_floor/1k/normal-dx.jpg",23u,Kind::Copy,646643ull,"81246FAA28125E66C67129A36BA99BD5617F3CB9FD27F8958E5C389524248735"},
    {"environment-material-packs/poly-haven/brick_floor/1k/roughness.jpg","engine/assets/material-packs/poly-haven/brick_floor/1k/roughness.jpg",23u,Kind::Copy,301533ull,"4B1073AF0BED93BE3A23F0D3A6A3C93D29E369E118C2FA7FAEC9D1D8C4EFC402"},
    {"environment-material-packs/poly-haven/forest_leaves_04/2k/forest_leaves_04_diff_2k.png","engine/assets/material-packs/poly-haven/forest_leaves_04/2k/forest_leaves_04_diff_2k.png",22u,Kind::Copy,10402792ull,"40B1786AFA17C9930CCAE93A9CA75880FC52FC34B96453489C767B2CC82D7EE9"},
    {"environment-material-packs/poly-haven/forest_leaves_04/2k/forest_leaves_04_nor_dx_2k.png","engine/assets/material-packs/poly-haven/forest_leaves_04/2k/forest_leaves_04_nor_dx_2k.png",22u,Kind::Copy,11539651ull,"19BBAA0EB67EDA146129946B9605CFC9B472A302BA1F75EB8EA0B48AA21BAA33"},
    {"environment-material-packs/poly-haven/forest_leaves_04/2k/forest_leaves_04_rough_2k.png","engine/assets/material-packs/poly-haven/forest_leaves_04/2k/forest_leaves_04_rough_2k.png",22u,Kind::Copy,6083648ull,"713874257D143AF49C482CE8B7648CB7C5C748CA6540DB0E44563A04A9FE09CE"},
    {"environment-material-packs/poly-haven/forest_leaves_04/DERIVATION.txt","engine/assets/material-packs/poly-haven/forest_leaves_04/DERIVATION.txt",22u,Kind::Copy,1389ull,"F6138A356ABB5A458A5E6C08B50E17361C6DDC07738F2066A4AF8936E4BE811C"},
    {"environment-material-packs/poly-haven/forest_leaves_04/LICENSE.txt","engine/assets/material-packs/poly-haven/forest_leaves_04/LICENSE.txt",22u,Kind::Copy,695ull,"5E440DE55C40EB4DAD49DCC0505E7854267FB10688793E58EA5167CC29A5A719"},
    {"environment-material-packs/poly-haven/rock_surface/1k/base-color.jpg","engine/assets/material-packs/poly-haven/rock_surface/1k/base-color.jpg",23u,Kind::Copy,886291ull,"07EDDA2BBE4715B01AAE2E4BAC9F21F204CF4AAE37B4733EF01DE33281E5E60E"},
    {"environment-material-packs/poly-haven/rock_surface/1k/normal-dx.jpg","engine/assets/material-packs/poly-haven/rock_surface/1k/normal-dx.jpg",23u,Kind::Copy,1161843ull,"1E1AA5B06F74D22734318CD93ECF49FFE7095C4B2307C5C9B3A4550207C6B1B7"},
    {"environment-material-packs/poly-haven/rock_surface/1k/roughness.jpg","engine/assets/material-packs/poly-haven/rock_surface/1k/roughness.jpg",23u,Kind::Copy,642680ull,"CA710D895B01CF323BF00F53194C43504A39D2C964FBFC343C4F45C7D41B10DC"},
    {"environment-material-packs/poly-haven/worn_planks/1k/base-color.jpg","engine/assets/material-packs/poly-haven/worn_planks/1k/base-color.jpg",23u,Kind::Copy,943056ull,"698C09B337375A9AAE12A76AD09FF2FDA11D735E6C1BBB18D72875BD61988DB6"},
    {"environment-material-packs/poly-haven/worn_planks/1k/normal-dx.png","engine/assets/material-packs/poly-haven/worn_planks/1k/normal-dx.png",23u,Kind::Copy,4886175ull,"743411ACF7332D8176D4E402237BB4B2D28A89FDC9BA160BB1EEE2DE6FBFB74B"},
    {"environment-material-packs/poly-haven/worn_planks/1k/roughness.jpg","engine/assets/material-packs/poly-haven/worn_planks/1k/roughness.jpg",23u,Kind::Copy,807719ull,"C79F640DC2D7EF81D69DE463EF6676B7C540D598FB0E462A6EB39EB5462770A9"},
    {"environment-model-packs/LICENSE.md","engine/assets/model-packs/LICENSE.md",23u,Kind::Copy,326ull,"5F80075030A5CD5B6B50315D103AB78B30E9C936C2773F4816E0F131BE622079"},
    {"environment-model-packs/model-packs.json","engine/assets/model-packs/model-packs.json",23u,Kind::Copy,1087ull,"CB19378913FA4C5F797FF6981E90259AF0EBFED632249DC13C7DC3DC592ADACE"},
    {"environment-model-packs/poly-haven/quiver_tree_02/1k/base-color.jpg","engine/assets/model-packs/poly-haven/quiver_tree_02/1k/base-color.jpg",23u,Kind::Copy,736321ull,"546A745AD765529D3C310178321E145909F4817B06679788AD24D29EC7EAE7B7"},
    {"environment-model-packs/poly-haven/quiver_tree_02/1k/normal-dx.jpg","engine/assets/model-packs/poly-haven/quiver_tree_02/1k/normal-dx.jpg",23u,Kind::Copy,993320ull,"EEFA8B6C50BC40E8BA5EB4AA3092882625954DE0E352F4C510AEB8D394412238"},
    {"environment-model-packs/poly-haven/quiver_tree_02/1k/roughness.jpg","engine/assets/model-packs/poly-haven/quiver_tree_02/1k/roughness.jpg",23u,Kind::Copy,441029ull,"076EF877C8D4CF27B0B37B974A8C7AACC21676020FD61D38CDC9CF0B330D3A24"},
    {"environment-model-packs/poly-haven/quiver_tree_02/quiver_tree_02.aemesh","engine/assets/model-packs/poly-haven/quiver_tree_02/quiver_tree_02.aemesh",23u,Kind::Copy,2357044ull,"ADC034B356E7A381F4CC69F911AE7F4E2637ADEF47A0AE482DE356C37BAF8142"},
    {"environment-model-packs/poly-haven/quiver_tree_02/source/quiver_tree_02.bin","engine/assets/model-packs/poly-haven/quiver_tree_02/source/quiver_tree_02.bin",23u,Kind::Copy,2060604ull,"FD3598B4AEA34BC6864452D2DBB37A04E4BFA7AD1AB6062A3B97C485FAE258F3"},
    {"environment-model-packs/poly-haven/quiver_tree_02/source/quiver_tree_02_1k.gltf","engine/assets/model-packs/poly-haven/quiver_tree_02/source/quiver_tree_02_1k.gltf",23u,Kind::Copy,2968ull,"ABDD8C2031A12905783EF2DABED11E5BC87123D322E057C138AEB9B67E400E91"},
    {"environment-model-packs/poly-haven/rock_09/1k/base-color.jpg","engine/assets/model-packs/poly-haven/rock_09/1k/base-color.jpg",23u,Kind::Copy,595103ull,"AFB7AE5AE82238D304ABA589391AC6BC87F7CF1DBFE50980C21F762CFDD87EFD"},
    {"environment-model-packs/poly-haven/rock_09/1k/normal-dx.jpg","engine/assets/model-packs/poly-haven/rock_09/1k/normal-dx.jpg",23u,Kind::Copy,747331ull,"97AB8B59D38C40602851D13F7641E2773146C617CB57D4CF573398B48AB0DA9A"},
    {"environment-model-packs/poly-haven/rock_09/1k/roughness.jpg","engine/assets/model-packs/poly-haven/rock_09/1k/roughness.jpg",23u,Kind::Copy,445567ull,"A32A88790A2F5A1E2A89FAAA2CAEADC7C89453B49916495DBF2A88CD0A8F39EF"},
    {"environment-model-packs/poly-haven/rock_09/rock_09.aemesh","engine/assets/model-packs/poly-haven/rock_09/rock_09.aemesh",23u,Kind::Copy,334592ull,"968B5CF056071C8926E425DA6E3A557214942F4DFE97653500975A6AB85EE548"},
    {"environment-model-packs/poly-haven/rock_09/source/rock_09.bin","engine/assets/model-packs/poly-haven/rock_09/source/rock_09.bin",23u,Kind::Copy,286592ull,"780C37D6FC11CA3A01C3763535EF883908B6007271A46BD446D2289DED820751"},
    {"environment-model-packs/poly-haven/rock_09/source/rock_09_1k.gltf","engine/assets/model-packs/poly-haven/rock_09/source/rock_09_1k.gltf",23u,Kind::Copy,2904ull,"2BF7FD605785CA55846BE710255EDA6123B1A927334068E290E4C7A4842C88C7"},
    {"material-packs/LICENSE.md","engine/assets/material-packs/LICENSE.md",23u,Kind::Copy,865ull,"D81BC4A3F3B4FCC401182F7C307E601671E5A5E264EAEC47F1C7BBCA2C1E409D"},
    {"material-packs/ambientcg/ground037/1k/Ground037.png","engine/assets/material-packs/ambientcg/ground037/1k/Ground037.png",23u,Kind::Copy,513329ull,"BC7CEBB2860A92E9446802A3018F56074879803F1BDDB0B80517CD5BFD57C6AE"},
    {"material-packs/ambientcg/ground037/1k/Ground037_1K-JPG.blend","engine/assets/material-packs/ambientcg/ground037/1k/Ground037_1K-JPG.blend",23u,Kind::Copy,1109220ull,"980FEED3DFE7AADB2B2EF1A917E5AC6AFC5C92D96AD3152B43EB03018E554F85"},
    {"material-packs/ambientcg/ground037/1k/Ground037_1K-JPG.mtlx","engine/assets/material-packs/ambientcg/ground037/1k/Ground037_1K-JPG.mtlx",23u,Kind::Copy,2307ull,"D00C32BAD302D4616D2BE47FB9CB34A97845F8B7609F13B29197CC448A61BFBF"},
    {"material-packs/ambientcg/ground037/1k/Ground037_1K-JPG.tres","engine/assets/material-packs/ambientcg/ground037/1k/Ground037_1K-JPG.tres",23u,Kind::Copy,1039ull,"8F917AE493E419A7F6FF7ACEBEEEA239C270D17F5399B0D20C8A589187B80BAD"},
    {"material-packs/ambientcg/ground037/1k/Ground037_1K-JPG.usdc","engine/assets/material-packs/ambientcg/ground037/1k/Ground037_1K-JPG.usdc",23u,Kind::Copy,2982ull,"3B3C481B29DFB3C862AD36CFEBC86C6AB0902A818262D67ABE52C6D91E9A2377"},
    {"material-packs/ambientcg/ground037/1k/Ground037_1K-JPG_AmbientOcclusion.jpg","engine/assets/material-packs/ambientcg/ground037/1k/Ground037_1K-JPG_AmbientOcclusion.jpg",23u,Kind::Copy,612112ull,"8AFE189AE915888551076CB279F6282C84255E4B2BDB2C4CE8C1C891074245EF"},
    {"material-packs/ambientcg/ground037/1k/Ground037_1K-JPG_Color.jpg","engine/assets/material-packs/ambientcg/ground037/1k/Ground037_1K-JPG_Color.jpg",23u,Kind::Copy,2187764ull,"D92BEC8A35B9CB16EC79B421CA0F572A7400286EAE109BEAFA266629EF49A6CF"},
    {"material-packs/ambientcg/ground037/1k/Ground037_1K-JPG_Displacement.jpg","engine/assets/material-packs/ambientcg/ground037/1k/Ground037_1K-JPG_Displacement.jpg",23u,Kind::Copy,500481ull,"BC102168F5D014CE208ED5C1D9140C39513D29303E1B3436CB06C92C7780E419"},
    {"material-packs/ambientcg/ground037/1k/Ground037_1K-JPG_NormalDX.jpg","engine/assets/material-packs/ambientcg/ground037/1k/Ground037_1K-JPG_NormalDX.jpg",23u,Kind::Copy,2474498ull,"0E2D613D117D9231AA0AA61F39B19755F891B4ED1D1599A004CB018A8224E039"},
    {"material-packs/ambientcg/ground037/1k/Ground037_1K-JPG_NormalGL.jpg","engine/assets/material-packs/ambientcg/ground037/1k/Ground037_1K-JPG_NormalGL.jpg",23u,Kind::Copy,2474236ull,"EA59DBDCFA5BF2F28BDD4AA896D4E5F96C4CCE5939100DCC6DB4BBBCDEBFE38D"},
    {"material-packs/ambientcg/ground037/1k/Ground037_1K-JPG_Roughness.jpg","engine/assets/material-packs/ambientcg/ground037/1k/Ground037_1K-JPG_Roughness.jpg",23u,Kind::Copy,694944ull,"3786B088E981AAE562F06B6477972D379CFFD09A5CC542AB42F9FFD94F9BCB0E"},
    {"material-packs/ambientcg/woodsiding006/1k/WoodSiding006.png","engine/assets/material-packs/ambientcg/woodsiding006/1k/WoodSiding006.png",23u,Kind::Copy,304302ull,"B0F5C8FB01F5249F70574DAB2FE7E7AFA4CD90B0BEFF9F607C172A47FBB4F2CD"},
    {"material-packs/ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG.blend","engine/assets/material-packs/ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG.blend",23u,Kind::Copy,1110260ull,"72925D4E752B1F2FA1A217AF2A77D02FDF53C89A4A177E4E8226BD9AEC8C2E39"},
    {"material-packs/ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG.mtlx","engine/assets/material-packs/ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG.mtlx",23u,Kind::Copy,2367ull,"DC1AC65DBCA30FFCAEAED35C6DD45EEC6A5DD12C679CE13129E9623D7554948D"},
    {"material-packs/ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG.tres","engine/assets/material-packs/ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG.tres",23u,Kind::Copy,1059ull,"C78565C440157D8E11707070A4F6BF33A1B2A1DA7FF7B49913D08444185BC721"},
    {"material-packs/ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG.usdc","engine/assets/material-packs/ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG.usdc",23u,Kind::Copy,3050ull,"4C00FAA28CB21C2C299B1F8913A80356FE0F8C978DC14173F87562B96F3AF799"},
    {"material-packs/ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG_AmbientOcclusion.jpg","engine/assets/material-packs/ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG_AmbientOcclusion.jpg",23u,Kind::Copy,127417ull,"61BDA7CF59ADD9C21A15C18328E4391BE3CFD8101986F058A5345D7BB39D3C60"},
    {"material-packs/ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG_Color.jpg","engine/assets/material-packs/ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG_Color.jpg",23u,Kind::Copy,724730ull,"C626EC1B31055A85E8E09D42CE2D0C6F0B968250AD6672EA3282B136DD568D46"},
    {"material-packs/ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG_Displacement.jpg","engine/assets/material-packs/ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG_Displacement.jpg",23u,Kind::Copy,285699ull,"51CF0B44D8A13AB05222C723AB06016B821DAC665CE7ECE64BDF150829EF3210"},
    {"material-packs/ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG_NormalDX.jpg","engine/assets/material-packs/ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG_NormalDX.jpg",23u,Kind::Copy,1849769ull,"55895492F8B92A1D847201E76D339679097D0901F7B3FBF9BBCD1C740B05F3FB"},
    {"material-packs/ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG_NormalGL.jpg","engine/assets/material-packs/ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG_NormalGL.jpg",23u,Kind::Copy,1847106ull,"C319E4180FC6261BF19753D597BC64189F9CD721DE4328FA4D67FFF8D1B71F32"},
    {"material-packs/ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG_Roughness.jpg","engine/assets/material-packs/ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG_Roughness.jpg",23u,Kind::Copy,423700ull,"ADC51758DDAF97BCD795EEF154F705A4F020594EFC5A5B515D0F92B39240C844"},
    {"material-packs/material-packs.json","engine/assets/material-packs/material-packs.json",23u,Kind::Copy,3758ull,"70853E6F97A1489C02A428A62295F556815C510D42948FCD33E9D33897773ED4"},
    {"material-packs/original/LICENSE.txt","games/character_lab/assets/material-packs/original/LICENSE.txt",23u,Kind::Copy,224ull,"45365F83BB562A0E90A6A76466D90A27E311FAB9780DD0079B3ABE5186FD5F00"},
    {"material-packs/original/README.md","games/character_lab/assets/material-packs/original/README.md",23u,Kind::Copy,1691ull,"878DAE9ADFD878340ADE0D7BBF963BE96F865BE82E2C10E257FF6EC273B936A5"},
    {"material-packs/original/cloth/base-color.png","games/character_lab/assets/material-packs/original/cloth/base-color.png",23u,Kind::Copy,360206ull,"13C08AA1C90A62EF324C5D42B433B1E9F9F4FF41D32CE51E2013C0AB8DDDC671"},
    {"material-packs/original/cloth/normal-dx.png","games/character_lab/assets/material-packs/original/cloth/normal-dx.png",23u,Kind::Copy,44832ull,"3AB8082981B9446DA4196E9B5C0FB9CC77818A2DE55976D4EB0E6667D213F9BF"},
    {"material-packs/original/cloth/roughness.png","games/character_lab/assets/material-packs/original/cloth/roughness.png",23u,Kind::Copy,550189ull,"9BF5461F825D25719EAC0EC786B920EBCB6B72A37A6452CD2DC0A691EDFCD24F"},
    {"material-packs/original/eye/base-color.png","games/character_lab/assets/material-packs/original/eye/base-color.png",23u,Kind::Copy,459681ull,"3A41436B919751D74378A500E3055578110E58C5487907589C0D5EBC07103228"},
    {"material-packs/original/eye/normal-dx.png","games/character_lab/assets/material-packs/original/eye/normal-dx.png",23u,Kind::Copy,550895ull,"52F695E7319FC5706C11DDC1B1FA047041073FC8C2B455BFE0BEB3D2476C4B78"},
    {"material-packs/original/eye/roughness.png","games/character_lab/assets/material-packs/original/eye/roughness.png",23u,Kind::Copy,681663ull,"38F2943B2DD786B0C212F0CB8C43CCA3325371869F2003E3E0841E99C7AD3B53"},
    {"material-packs/original/female.materials.json","games/character_lab/assets/material-packs/original/female.materials.json",23u,Kind::Copy,9183ull,"8ABABA9618D4EB3B879C11CA57DB43DFE96C59C18AFFA3274CA7768576DC21AD"},
    {"material-packs/original/hair/base-color.png","games/character_lab/assets/material-packs/original/hair/base-color.png",23u,Kind::Copy,651367ull,"58FE70D841393F1E7EFC40C4EE6FBD717EE3BC191FEE99B759AB59D9EBA6385F"},
    {"material-packs/original/hair/normal-dx.png","games/character_lab/assets/material-packs/original/hair/normal-dx.png",23u,Kind::Copy,1347909ull,"9AFA9B5E31A07E3B42FCCA71687FCD13DCE85EF094CB687A71922CABDAC0D2FF"},
    {"material-packs/original/hair/roughness.png","games/character_lab/assets/material-packs/original/hair/roughness.png",23u,Kind::Copy,561713ull,"5B2F7EAC47F51B7B735E74C364B317ABF828A0E9607067B2A83680DD1963A7AC"},
    {"material-packs/original/iris/base-color.png","games/character_lab/assets/material-packs/original/iris/base-color.png",23u,Kind::Copy,1878197ull,"48B2D452E67B88A42A683F7977A052D34F7E52D155E2A0A5645D9B1B134F73CA"},
    {"material-packs/original/iris/normal-dx.png","games/character_lab/assets/material-packs/original/iris/normal-dx.png",23u,Kind::Copy,586547ull,"66ADFB15F839257AA89C680CDA81840881D62321F6A75041273ADCA2BA34279A"},
    {"material-packs/original/iris/roughness.png","games/character_lab/assets/material-packs/original/iris/roughness.png",23u,Kind::Copy,15787ull,"1B05A7FBDD18771D92887E6DCA70FB27DCE0099224E5A482F05924457B4F3E05"},
    {"material-packs/original/leather/base-color.png","games/character_lab/assets/material-packs/original/leather/base-color.png",23u,Kind::Copy,122657ull,"9346F25C5DED6415EA3595F1BF6A260164A4EEB23FE2974126CAE16F127819EA"},
    {"material-packs/original/leather/normal-dx.png","games/character_lab/assets/material-packs/original/leather/normal-dx.png",23u,Kind::Copy,547141ull,"C0A4CF79EC728B6414DB65014CCE1EA24C812E8EEFDF236269C5468DE1FC36C5"},
    {"material-packs/original/leather/roughness.png","games/character_lab/assets/material-packs/original/leather/roughness.png",23u,Kind::Copy,179298ull,"3ED0C622A4629432B8F9DB39AB889FDA8309361B4AEBC457DA32BCD9531B6619"},
    {"material-packs/original/lips/base-color.png","games/character_lab/assets/material-packs/original/lips/base-color.png",23u,Kind::Copy,2455662ull,"D8AC6B18C4A549F990EAE5F149E66D193C23954A6BB2DFE197AD6BFF8D4DA8ED"},
    {"material-packs/original/lips/normal-dx.png","games/character_lab/assets/material-packs/original/lips/normal-dx.png",23u,Kind::Copy,7591008ull,"C1D352D99F27D68ABE62863C659F1258E7554A740611F4B5533C1AA147B113EF"},
    {"material-packs/original/lips/roughness.png","games/character_lab/assets/material-packs/original/lips/roughness.png",23u,Kind::Copy,15787ull,"A7AD5DBBC1CA962915EE47D8ED06DDDD415C767594F55C90765EB5693ED0DBBE"},
    {"material-packs/original/male.materials.json","games/character_lab/assets/material-packs/original/male.materials.json",23u,Kind::Copy,8566ull,"9337B658F7D766002C741A18FBFC7ACA37FDE2AFA6632A3589ADD3E5A8568067"},
    {"material-packs/original/manifest.json","games/character_lab/assets/material-packs/original/manifest.json",23u,Kind::Copy,6617ull,"947DE404394BDDDE2E9D26F580E6703BCE11A8E7CC377E959569752201DC6E86"},
    {"material-packs/original/metal/base-color.png","games/character_lab/assets/material-packs/original/metal/base-color.png",23u,Kind::Copy,113255ull,"E503938EBAC9A00B2805A09016E54B59BE91B178BE1CB60C738878A49054D734"},
    {"material-packs/original/metal/normal-dx.png","games/character_lab/assets/material-packs/original/metal/normal-dx.png",23u,Kind::Copy,359078ull,"549F4D5E59A33F6CAFF0A9EABA2F025DB5C7A316BE028557370D47DF4CD9DA7F"},
    {"material-packs/original/metal/roughness.png","games/character_lab/assets/material-packs/original/metal/roughness.png",23u,Kind::Copy,169181ull,"20CD476831E12D58F4C9637A3C0AC6703D39CFF4149282F4E5589CAFD5BB2184"},
    {"material-packs/original/sclera/base-color.png","games/character_lab/assets/material-packs/original/sclera/base-color.png",23u,Kind::Copy,1443361ull,"B626017636223C2D8BAA4F1F21CE1966B36E8BAF4EFC499969EFBE546C2048A6"},
    {"material-packs/original/sclera/normal-dx.png","games/character_lab/assets/material-packs/original/sclera/normal-dx.png",23u,Kind::Copy,586547ull,"66ADFB15F839257AA89C680CDA81840881D62321F6A75041273ADCA2BA34279A"},
    {"material-packs/original/sclera/roughness.png","games/character_lab/assets/material-packs/original/sclera/roughness.png",23u,Kind::Copy,15787ull,"23D09153326E2F67108E69B3265D23D1456A03D597F0911E7F6932D36B383EB8"},
    {"material-packs/original/skin/base-color.png","games/character_lab/assets/material-packs/original/skin/base-color.png",23u,Kind::Copy,1727656ull,"AC13ED5EE1E788814AD35CD6CA0410F49B5383618A3C611CC8BE90DC3BB03A66"},
    {"material-packs/original/skin/normal-dx.png","games/character_lab/assets/material-packs/original/skin/normal-dx.png",23u,Kind::Copy,7591008ull,"C1D352D99F27D68ABE62863C659F1258E7554A740611F4B5533C1AA147B113EF"},
    {"material-packs/original/skin/roughness.png","games/character_lab/assets/material-packs/original/skin/roughness.png",23u,Kind::Copy,2269354ull,"E51B1A067E208C4764D5897AEDF1F258E1AF8B71CE19F731E22E0092407DA183"},
    {"material-packs/original/skin/source/skin-microdetail-reference-v1.png","games/character_lab/assets/material-packs/original/skin/source/skin-microdetail-reference-v1.png",23u,Kind::Copy,2538819ull,"58E83E905F67572D58778E32D838E98F394175680F63ED9FC23C8E23845062D7"},
    {"material-packs/poly-haven/aerial_ground_rock/1k/base-color.jpg","engine/assets/material-packs/poly-haven/aerial_ground_rock/1k/base-color.jpg",23u,Kind::Copy,599629ull,"C14761998B32630DD193B0FBFD31C8C6F5A144A701B84F87F9D7C33596CC554F"},
    {"material-packs/poly-haven/aerial_ground_rock/1k/normal-dx.jpg","engine/assets/material-packs/poly-haven/aerial_ground_rock/1k/normal-dx.jpg",23u,Kind::Copy,601693ull,"1BCA894BD4DFADB3B06FD1AB1FB48441BF310EDC18418B20A52C08E67D1E15A7"},
    {"material-packs/poly-haven/aerial_ground_rock/1k/roughness.jpg","engine/assets/material-packs/poly-haven/aerial_ground_rock/1k/roughness.jpg",23u,Kind::Copy,376876ull,"411D1FDA581C4D824C8510C6A31EAAD7C35DD3B6EA94F0227FE2C03DC0CC39CB"},
    {"material-packs/poly-haven/bark_brown_02/1k/base-color.jpg","engine/assets/material-packs/poly-haven/bark_brown_02/1k/base-color.jpg",23u,Kind::Copy,659592ull,"920FA0BED0C9D78C1D530E99795113AFD532D7DB746DE440F4A85D7C83ED0F1A"},
    {"material-packs/poly-haven/bark_brown_02/1k/normal-dx.jpg","engine/assets/material-packs/poly-haven/bark_brown_02/1k/normal-dx.jpg",23u,Kind::Copy,1033312ull,"7FCF9B0EFA59DEE597CC9F7A367E3F4131D59F72BC6D37966B9A8E69C4A1BF74"},
    {"material-packs/poly-haven/bark_brown_02/1k/roughness.jpg","engine/assets/material-packs/poly-haven/bark_brown_02/1k/roughness.jpg",23u,Kind::Copy,337946ull,"68125592DE36E15BAE7AA6DB85C4D135AEB7E5653199BA226483A70F6D69837D"},
    {"material-packs/poly-haven/brick_floor/1k/base-color.jpg","engine/assets/material-packs/poly-haven/brick_floor/1k/base-color.jpg",23u,Kind::Copy,469580ull,"9C783B294D5A3127D7C5F5E52DF67BBD418AEBC90EC05696DA5AAC2BBA834058"},
    {"material-packs/poly-haven/brick_floor/1k/normal-dx.jpg","engine/assets/material-packs/poly-haven/brick_floor/1k/normal-dx.jpg",23u,Kind::Copy,646643ull,"81246FAA28125E66C67129A36BA99BD5617F3CB9FD27F8958E5C389524248735"},
    {"material-packs/poly-haven/brick_floor/1k/roughness.jpg","engine/assets/material-packs/poly-haven/brick_floor/1k/roughness.jpg",23u,Kind::Copy,301533ull,"4B1073AF0BED93BE3A23F0D3A6A3C93D29E369E118C2FA7FAEC9D1D8C4EFC402"},
    {"material-packs/poly-haven/forest_leaves_04/2k/forest_leaves_04_diff_2k.png","engine/assets/material-packs/poly-haven/forest_leaves_04/2k/forest_leaves_04_diff_2k.png",22u,Kind::Copy,10402792ull,"40B1786AFA17C9930CCAE93A9CA75880FC52FC34B96453489C767B2CC82D7EE9"},
    {"material-packs/poly-haven/forest_leaves_04/2k/forest_leaves_04_nor_dx_2k.png","engine/assets/material-packs/poly-haven/forest_leaves_04/2k/forest_leaves_04_nor_dx_2k.png",22u,Kind::Copy,11539651ull,"19BBAA0EB67EDA146129946B9605CFC9B472A302BA1F75EB8EA0B48AA21BAA33"},
    {"material-packs/poly-haven/forest_leaves_04/2k/forest_leaves_04_rough_2k.png","engine/assets/material-packs/poly-haven/forest_leaves_04/2k/forest_leaves_04_rough_2k.png",22u,Kind::Copy,6083648ull,"713874257D143AF49C482CE8B7648CB7C5C748CA6540DB0E44563A04A9FE09CE"},
    {"material-packs/poly-haven/forest_leaves_04/DERIVATION.txt","engine/assets/material-packs/poly-haven/forest_leaves_04/DERIVATION.txt",22u,Kind::Copy,1389ull,"F6138A356ABB5A458A5E6C08B50E17361C6DDC07738F2066A4AF8936E4BE811C"},
    {"material-packs/poly-haven/forest_leaves_04/LICENSE.txt","engine/assets/material-packs/poly-haven/forest_leaves_04/LICENSE.txt",22u,Kind::Copy,695ull,"5E440DE55C40EB4DAD49DCC0505E7854267FB10688793E58EA5167CC29A5A719"},
    {"material-packs/poly-haven/rock_surface/1k/base-color.jpg","engine/assets/material-packs/poly-haven/rock_surface/1k/base-color.jpg",23u,Kind::Copy,886291ull,"07EDDA2BBE4715B01AAE2E4BAC9F21F204CF4AAE37B4733EF01DE33281E5E60E"},
    {"material-packs/poly-haven/rock_surface/1k/normal-dx.jpg","engine/assets/material-packs/poly-haven/rock_surface/1k/normal-dx.jpg",23u,Kind::Copy,1161843ull,"1E1AA5B06F74D22734318CD93ECF49FFE7095C4B2307C5C9B3A4550207C6B1B7"},
    {"material-packs/poly-haven/rock_surface/1k/roughness.jpg","engine/assets/material-packs/poly-haven/rock_surface/1k/roughness.jpg",23u,Kind::Copy,642680ull,"CA710D895B01CF323BF00F53194C43504A39D2C964FBFC343C4F45C7D41B10DC"},
    {"material-packs/poly-haven/worn_planks/1k/base-color.jpg","engine/assets/material-packs/poly-haven/worn_planks/1k/base-color.jpg",23u,Kind::Copy,943056ull,"698C09B337375A9AAE12A76AD09FF2FDA11D735E6C1BBB18D72875BD61988DB6"},
    {"material-packs/poly-haven/worn_planks/1k/normal-dx.png","engine/assets/material-packs/poly-haven/worn_planks/1k/normal-dx.png",23u,Kind::Copy,4886175ull,"743411ACF7332D8176D4E402237BB4B2D28A89FDC9BA160BB1EEE2DE6FBFB74B"},
    {"material-packs/poly-haven/worn_planks/1k/roughness.jpg","engine/assets/material-packs/poly-haven/worn_planks/1k/roughness.jpg",23u,Kind::Copy,807719ull,"C79F640DC2D7EF81D69DE463EF6676B7C540D598FB0E462A6EB39EB5462770A9"},
    {"mistwatch-boulders/LICENSE.md","engine/assets/realistic-cc0-first-batch/LICENSE.md",63u,Kind::Copy,523ull,"8232D8364958A2C530272968194F55D9F306099C3C9F3D8BE5BEC741A9CD1826"},
    {"mistwatch-boulders/boulder-01-lod-receipt.json","engine/assets/realistic-cc0-first-batch/boulder-01/boulder-01-lod-receipt.json",63u,Kind::Copy,1135ull,"44C630B69C1E6F9909670E1D2067DB7E8BADB47A608121DB4BB5DD56BA2B1F7C"},
    {"mistwatch-boulders/materials/slot-0-arm.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/boulder-01/slot-0-arm.dds",63u,Kind::Copy,1398276ull,"564BE8685C266FE7FA03ACDA76C6C06F706911C4344777CEAB6563D743284A1E"},
    {"mistwatch-boulders/materials/slot-0-base-source-srgb-v1.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/boulder-01/slot-0-base-source-srgb-v1.dds",48u,Kind::Copy,1398276ull,"D874D5AC2E2C01CA7BD87970BA23415EF285E051B0A501C74E383E6219758B79"},
    {"mistwatch-boulders/materials/slot-0-base.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/boulder-01/slot-0-base.dds",63u,Kind::Copy,1398276ull,"5A847A3174E09505B3A404C93716CBAE576A5CB8BD32F69EC6395E8C95008D36"},
    {"mistwatch-boulders/materials/slot-0-normal.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/boulder-01/slot-0-normal.dds",63u,Kind::Copy,1398276ull,"68FBBE04FFDB1C6D327F4262B9230438623F0D3B4B17A0A5155EB13B9ED21E83"},
    {"mistwatch-boulders/meshes/boulder-01-lod0-node0-mesh0-primitive0-material0-boulder-01.aemesh","engine/assets/realistic-cc0-first-batch/cooked/meshes/boulder-01/boulder-01-lod0-node0-mesh0-primitive0-material0-boulder-01.aemesh",48u,Kind::Copy,3071924ull,"1005CCA1811D45922B9DADDCB1C6C6E155E09F569DF8EFE5DE093468B11C25D6"},
    {"mistwatch-boulders/meshes/boulder-01-lod2-node0-mesh0-primitive0-material0-boulder-01.aemesh","engine/assets/realistic-cc0-first-batch/cooked/meshes/boulder-01/boulder-01-lod2-node0-mesh0-primitive0-material0-boulder-01.aemesh",63u,Kind::Copy,1913612ull,"57513292274275206F02CFDCA569D515AE3B12247477CECB505ED465E5915133"},
    {"mistwatch-boulders/runtime-asset-manifest.json","engine/assets/realistic-cc0-first-batch/runtime-asset-manifest.json",63u,Kind::Copy,41058ull,"0D5A6801CDA2AF0B3424FCE78658DC42529E5B758511B43289E6C997985C68DF"},
    {"mistwatch-broadleaf/LICENSE.md","engine/assets/realistic-cc0-first-batch/LICENSE.md",63u,Kind::Copy,523ull,"8232D8364958A2C530272968194F55D9F306099C3C9F3D8BE5BEC741A9CD1826"},
    {"mistwatch-broadleaf/materials/branches-arm.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/tree-small-02/branches-arm.dds",63u,Kind::Copy,1398276ull,"1C54A88EC7A17C55528A99319378283D61B4800002AB4E63CA0FE30EBB1249EA"},
    {"mistwatch-broadleaf/materials/branches-base.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/tree-small-02/branches-base.dds",63u,Kind::Copy,1398276ull,"CF4F6FCC9329E66F05B7B2E86D20A38B2D0E8E054013CF3E0431A4CBC1E17D0A"},
    {"mistwatch-broadleaf/materials/branches-normal.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/tree-small-02/branches-normal.dds",63u,Kind::Copy,1398276ull,"EA8A046E64CE7EACCE435E64F31FC45501624FD131C7E6C8492BFD9DE9FB1BEB"},
    {"mistwatch-broadleaf/materials/leaves-arm.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/tree-small-02/leaves-arm.dds",63u,Kind::Copy,1398276ull,"C8A6A8747023A64ED506AD62B8E5C0AE16BF14E0353D5E67F8D2511967AAF2ED"},
    {"mistwatch-broadleaf/materials/leaves-base.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/tree-small-02/leaves-base.dds",63u,Kind::Copy,1398276ull,"88FCE8DC2F1815C421A0C7C4D9CC7D9CACB4024F6C51E8FC928A5E938B7996A9"},
    {"mistwatch-broadleaf/materials/leaves-normal.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/tree-small-02/leaves-normal.dds",63u,Kind::Copy,1398276ull,"320D6DF57BF075DBD701ACAB52740A623F0C86D298BD3048F3986D1988BB0CD9"},
    {"mistwatch-broadleaf/materials/material-bake-receipt.json","engine/assets/realistic-cc0-first-batch/cooked/materials/tree-small-02/material-bake-receipt.json",63u,Kind::Copy,11548ull,"4641330E3CD18CE1390FFC4C730333051157417476B3BD630BAF84547E7242B1"},
    {"mistwatch-broadleaf/materials/trunk-arm.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/tree-small-02/trunk-arm.dds",63u,Kind::Copy,1398276ull,"716C128DA96FEB1F3AD4EA041ECD9D5C167115A7AA0671F517EBAEFBC9963B7D"},
    {"mistwatch-broadleaf/materials/trunk-base.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/tree-small-02/trunk-base.dds",63u,Kind::Copy,1398276ull,"513D07ECE694D03E3C095C4148D7DA4649FFDF6DB54826CF1BF1754A368B1453"},
    {"mistwatch-broadleaf/materials/trunk-normal.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/tree-small-02/trunk-normal.dds",63u,Kind::Copy,1398276ull,"099E981864B1E90B2C0500B0BCB0A92FCDC38EE196F00F7161905DC9BF762712"},
    {"mistwatch-broadleaf/meshes/tree-small-02-lod2-node0-mesh0-primitive0-material0-tree-small-02-branches.aemesh","engine/assets/realistic-cc0-first-batch/cooked/meshes/tree-small-02/tree-small-02-lod2-node0-mesh0-primitive0-material0-tree-small-02-branches.aemesh",63u,Kind::Copy,163400ull,"4182C1D2D3E726F5C77DD84ADF59EA5D4E022D49B1F211B34B488DBC165B68CC"},
    {"mistwatch-broadleaf/meshes/tree-small-02-lod2-node0-mesh0-primitive1-material1-tree-small-02-leaves.aemesh","engine/assets/realistic-cc0-first-batch/cooked/meshes/tree-small-02/tree-small-02-lod2-node0-mesh0-primitive1-material1-tree-small-02-leaves.aemesh",63u,Kind::Copy,3704316ull,"C46ECA29C84D6EA9DB320444D01ED4F16403A2A7F50729178E70C7886B28BC5B"},
    {"mistwatch-broadleaf/meshes/tree-small-02-lod2-node0-mesh0-primitive2-material2-tree-small-02-trunk.aemesh","engine/assets/realistic-cc0-first-batch/cooked/meshes/tree-small-02/tree-small-02-lod2-node0-mesh0-primitive2-material2-tree-small-02-trunk.aemesh",63u,Kind::Copy,57760ull,"1C765173EBC17A3BE167C35EF1B8DF8329FB4CF98F468F4DA13FD42533B677FD"},
    {"mistwatch-broadleaf/tree-small-02-provenance-receipt.json","engine/assets/realistic-cc0-first-batch/tree-small-02/tree-small-02-provenance-receipt.json",63u,Kind::Copy,5166ull,"98916EB214099A7E6FFE8DD319D291693D02BE293B1DA012F6C5EDB1A9348CF4"},
    {"mistwatch-canopy-preview/LICENSE.md","engine/assets/realistic-cc0-first-batch/LICENSE.md",63u,Kind::Copy,523ull,"8232D8364958A2C530272968194F55D9F306099C3C9F3D8BE5BEC741A9CD1826"},
    {"mistwatch-canopy-preview/ROOT_DERIVATION.md","games/mistwatch/ROOT_DERIVATION.md",63u,Kind::Copy,2368ull,"DC630414056322DF2B306BB273F18C3A86E9E29308DDCA1A06D2BC4B3EF5C799"},
    {"mistwatch-canopy-preview/materials/branches-arm.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/island-tree-01/branches-arm.dds",63u,Kind::Copy,1398276ull,"92E5AE39785DBCEFC885588DEAAFD35A1B45D7E5E47F222320B85CB9CE810F05"},
    {"mistwatch-canopy-preview/materials/branches-base.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/island-tree-01/branches-base.dds",63u,Kind::Copy,1398276ull,"75E25268369704A561432DF6F3368734E8B5648FDCE184A0A4C2C5863317145B"},
    {"mistwatch-canopy-preview/materials/branches-normal.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/island-tree-01/branches-normal.dds",63u,Kind::Copy,1398276ull,"1AA3118C6FBA3891C31FB2E5C961EDD463E2A65084F611158DEA4ADC3E39DBBB"},
    {"mistwatch-canopy-preview/materials/leaves-arm.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/island-tree-01/leaves-arm.dds",63u,Kind::Copy,1398276ull,"792095487B7B49038DA96CA9CB9E417A52B29F98EB109D953EF4EEDCB82E9013"},
    {"mistwatch-canopy-preview/materials/leaves-base.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/island-tree-01/leaves-base.dds",63u,Kind::Copy,1398276ull,"4A1F4E5544D2B91ED3822455F457B93A4D3237019BC8AE6202CE1E0AEF9F14C2"},
    {"mistwatch-canopy-preview/materials/leaves-normal.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/island-tree-01/leaves-normal.dds",63u,Kind::Copy,1398276ull,"CC43D9A2635A0F8A6A750E41CC9857600A16D39FE9EDC2D7F2403D39790595EE"},
    {"mistwatch-canopy-preview/materials/trunk-arm.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/island-tree-01/trunk-arm.dds",63u,Kind::Copy,1398276ull,"638E6710501BB04D9626294B182246F7E651970B6EF2F791EFB941E2FAD9AAA4"},
    {"mistwatch-canopy-preview/materials/trunk-base.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/island-tree-01/trunk-base.dds",63u,Kind::Copy,1398276ull,"AA3708B589C5F80DDDB98022EEA49FBB71EB157B9EA7C32A02B3F0111E03C98D"},
    {"mistwatch-canopy-preview/materials/trunk-normal.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/island-tree-01/trunk-normal.dds",63u,Kind::Copy,1398276ull,"24EBA3FCB9E8D59B088E9CF312BDF25B32445562CE466262AA1D7834CD17BB7A"},
    {"mistwatch-canopy-preview/meshes/island-tree-01-lod1-node0-mesh0-primitive1-material1-island-tree-01-leaves.aemesh","engine/assets/realistic-cc0-first-batch/cooked/meshes/island-tree-01/island-tree-01-lod1-node0-mesh0-primitive1-material1-island-tree-01-leaves.aemesh",63u,Kind::Copy,21375028ull,"1773F7705A43DA3B3673D0D34C66C2C4FEFC47952970CB84665A774D6A54F871"},
    {"mistwatch-canopy-preview/meshes/island-tree-01-lod2-node0-mesh0-primitive0-material0-island-tree-01.aemesh","engine/assets/realistic-cc0-first-batch/cooked/meshes/island-tree-01/island-tree-01-lod2-node0-mesh0-primitive0-material0-island-tree-01.aemesh",63u,Kind::Copy,159612ull,"B964D1F7B16FB3602FF038AE111C79D0BFC029185896E95DA97CC1ACB4F59145"},
    {"mistwatch-canopy-preview/meshes/island-tree-01-lod2-node0-mesh0-primitive2-material2-island-tree-01-branches.aemesh","engine/assets/realistic-cc0-first-batch/cooked/meshes/island-tree-01/island-tree-01-lod2-node0-mesh0-primitive2-material2-island-tree-01-branches.aemesh",63u,Kind::Copy,2709952ull,"5D32528C7604D041A0730755DDF765B111F82B43847C5F4AED0FE386E99D6188"},
    {"mistwatch-canopy-preview/meshes/island-tree-01-lod2-trunk-root-stem.aemesh","",63u,Kind::RootMesh,165000ull,"96EA5581E6A5D2A0C3260DEEBE614A4FA9BCCA0A77772E5EAE42E0C88730BC7C"},
    {"mistwatch-canopy-preview/root-stem-derivation.json","",63u,Kind::RootReceipt,250893ull,"104607F9FA1E815A37583CCB8812A3982138B4F0F86230CDA91C494D50C649D6"},
    {"mistwatch-canopy-preview/runtime-asset-manifest.json","engine/assets/realistic-cc0-first-batch/runtime-asset-manifest.json",63u,Kind::Copy,41058ull,"0D5A6801CDA2AF0B3424FCE78658DC42529E5B758511B43289E6C997985C68DF"},
    {"mistwatch-fallen-logs/LICENSE.md","engine/assets/mistwatch-fallen-logs/LICENSE.md",63u,Kind::Copy,1998ull,"CE9483C1783A84DC63A77C36B376D23A44598F1ECFC34A7A689BDA5EBC405083"},
    {"mistwatch-fallen-logs/fallen-log-package-manifest.json","engine/assets/mistwatch-fallen-logs/fallen-log-package-manifest.json",63u,Kind::Copy,10663ull,"B8F6AEBD3986B3A674981F45A6A6BFA0D4262CCCA458818D518CA3F8140EB98B"},
    {"mistwatch-fallen-logs/materials/slot-0-arm.dds","engine/assets/mistwatch-fallen-logs/materials/slot-0-arm.dds",63u,Kind::Copy,5592580ull,"147D81E27AFEAA1D1AFC6E88D70D3814912A7D6235110CA4935EDF6FFB5838FE"},
    {"mistwatch-fallen-logs/materials/slot-0-base.dds","engine/assets/mistwatch-fallen-logs/materials/slot-0-base.dds",63u,Kind::Copy,5592580ull,"DA9DB4EDE331A22F4E47BA2EAB34CFCFB2C6979F6F180007E28DFC61EA23410B"},
    {"mistwatch-fallen-logs/materials/slot-0-normal.dds","engine/assets/mistwatch-fallen-logs/materials/slot-0-normal.dds",63u,Kind::Copy,5592580ull,"F2F8EBD0A4C52C543047FB8131AD42391374313D595CD0901DDF5136C2B01354"},
    {"mistwatch-fallen-logs/meshes/dead-tree-trunk-lod2-node0-mesh0-primitive0-materialnone-material-none.aemesh","engine/assets/mistwatch-fallen-logs/meshes/dead-tree-trunk-lod2-node0-mesh0-primitive0-materialnone-material-none.aemesh",63u,Kind::Copy,434012ull,"B7F6A05BF928787082DEA92A0051557496E45894A9C2B1A0AF1E51110ABD0D46"},
    {"mistwatch-fern-colonies-test/meshes/fern-02-lod0-node0-mesh0-primitive0-material0-fern-02.aemesh","engine/assets/mistwatch-fern-02/meshes/fern-02-lod0-node0-mesh0-primitive0-material0-fern-02.aemesh",23u,Kind::Copy,75104ull,"83BEE63C50758AF9833AAF8CB0ABC06000DF21C412F4971BD582D64ADD4EAE81"},
    {"mistwatch-fern-colonies-test/meshes/fern-02-lod0-node1-mesh1-primitive0-material0-fern-02.aemesh","engine/assets/mistwatch-fern-02/meshes/fern-02-lod0-node1-mesh1-primitive0-material0-fern-02.aemesh",23u,Kind::Copy,70896ull,"07DAAD96517DA59466BE5551A6460F0943E5142B55CF2F7F0254938D92BA0306"},
    {"mistwatch-fern-colonies-test/meshes/fern-02-lod0-node2-mesh2-primitive0-material0-fern-02.aemesh","engine/assets/mistwatch-fern-02/meshes/fern-02-lod0-node2-mesh2-primitive0-material0-fern-02.aemesh",23u,Kind::Copy,24656ull,"78702057068C309D6C559F223FB282C29BAC32B8C7B40987C14338F5F65E7E5A"},
    {"mistwatch-fern-colonies-test/meshes/fern-02-lod0-node3-mesh3-primitive0-material0-fern-02.aemesh","engine/assets/mistwatch-fern-02/meshes/fern-02-lod0-node3-mesh3-primitive0-material0-fern-02.aemesh",23u,Kind::Copy,25628ull,"243A1BA9863EE362F8BEBCDDA7BA0DC44D847FF8684CAC327B68A422C26663F8"},
    {"mistwatch-ferns/LICENSE.md","engine/assets/mistwatch-fern-02/LICENSE.md",63u,Kind::Copy,1002ull,"F09B0F2C7D5AD858E18F8B23A4A80DABFFBEB45A69F4342352B9AF0732E4E21E"},
    {"mistwatch-ferns/download-receipt.json","engine/assets/mistwatch-fern-02/download-receipt.json",63u,Kind::Copy,2027ull,"569CD33410ACDA00C4B5596BEDCBC364C0175E805DAE57F92F3E92802BF54E48"},
    {"mistwatch-ferns/material-bake-receipt.json","engine/assets/mistwatch-fern-02/material-bake-receipt.json",63u,Kind::Copy,5052ull,"9B467340C54BC1AD799047A996B3406DE5AFBDCED2E28A329F1737DCFE1D0075"},
    {"mistwatch-ferns/materials/fern-arm.dds","engine/assets/mistwatch-fern-02/materials/fern-arm.dds",63u,Kind::Copy,5592580ull,"F3F3EAF5C28F1C70ECF466042DC10FC62AE0C66169746BA9833F9E63A1A79CF9"},
    {"mistwatch-ferns/materials/fern-base.dds","engine/assets/mistwatch-fern-02/materials/fern-base.dds",63u,Kind::Copy,5592580ull,"FAEBCC3EAB0BC4A7C334E40E4229E7D35E5745119C2D0A50EEF0DB22E1E8D563"},
    {"mistwatch-ferns/materials/fern-normal.dds","engine/assets/mistwatch-fern-02/materials/fern-normal.dds",63u,Kind::Copy,5592580ull,"980126CB53F2E3407F14A066B1B96E9F9205EBA67CFF74B761D4BA11263BB860"},
    {"mistwatch-ferns/meshes/fern-02-lod0-node0-mesh0-primitive0-material0-fern-02.aemesh","engine/assets/mistwatch-fern-02/meshes/fern-02-lod0-node0-mesh0-primitive0-material0-fern-02.aemesh",63u,Kind::Copy,75104ull,"83BEE63C50758AF9833AAF8CB0ABC06000DF21C412F4971BD582D64ADD4EAE81"},
    {"mistwatch-ferns/meshes/fern-02-lod0-node1-mesh1-primitive0-material0-fern-02.aemesh","engine/assets/mistwatch-fern-02/meshes/fern-02-lod0-node1-mesh1-primitive0-material0-fern-02.aemesh",63u,Kind::Copy,70896ull,"07DAAD96517DA59466BE5551A6460F0943E5142B55CF2F7F0254938D92BA0306"},
    {"mistwatch-ferns/meshes/fern-02-lod0-node2-mesh2-primitive0-material0-fern-02.aemesh","engine/assets/mistwatch-fern-02/meshes/fern-02-lod0-node2-mesh2-primitive0-material0-fern-02.aemesh",63u,Kind::Copy,24656ull,"78702057068C309D6C559F223FB282C29BAC32B8C7B40987C14338F5F65E7E5A"},
    {"mistwatch-ferns/meshes/fern-02-lod0-node3-mesh3-primitive0-material0-fern-02.aemesh","engine/assets/mistwatch-fern-02/meshes/fern-02-lod0-node3-mesh3-primitive0-material0-fern-02.aemesh",63u,Kind::Copy,25628ull,"243A1BA9863EE362F8BEBCDDA7BA0DC44D847FF8684CAC327B68A422C26663F8"},
    {"mistwatch-forest-cover-test/meshes/grass-medium-02-lod0-node0-mesh0-primitive0-material0-grass-medium-02.aemesh","engine/assets/mistwatch-grass-medium-02/meshes/grass-medium-02-lod0-node0-mesh0-primitive0-material0-grass-medium-02.aemesh",23u,Kind::Copy,26644ull,"84C8C7DA8EEB335B1BD60415531A30798933C688FF0D3A241ECA1958FB9C2E56"},
    {"mistwatch-forest-cover-test/meshes/grass-medium-02-lod0-node1-mesh1-primitive0-material0-grass-medium-02.aemesh","engine/assets/mistwatch-grass-medium-02/meshes/grass-medium-02-lod0-node1-mesh1-primitive0-material0-grass-medium-02.aemesh",23u,Kind::Copy,65236ull,"CE3EB8A245F7C36682FCABB106EBB56E79BEAE95D6345DF330AC2D2DD5F76D05"},
    {"mistwatch-forest-cover-test/meshes/grass-medium-02-lod0-node2-mesh2-primitive0-material0-grass-medium-02.aemesh","engine/assets/mistwatch-grass-medium-02/meshes/grass-medium-02-lod0-node2-mesh2-primitive0-material0-grass-medium-02.aemesh",23u,Kind::Copy,44612ull,"0AA49B54653A3B69A266CC7575AC8FF405A968E220BAC500A6BA3FAA3D432363"},
    {"mistwatch-forest-cover-test/meshes/grass-medium-02-lod0-node3-mesh3-primitive0-material0-grass-medium-02.aemesh","engine/assets/mistwatch-grass-medium-02/meshes/grass-medium-02-lod0-node3-mesh3-primitive0-material0-grass-medium-02.aemesh",23u,Kind::Copy,91400ull,"78ED34130EAC919ED41F02918676E68D4060713C52C570B9E4ABEBCD1AA47C5B"},
    {"mistwatch-forest-cover-test/meshes/grass-medium-02-lod0-node4-mesh4-primitive0-material0-grass-medium-02.aemesh","engine/assets/mistwatch-grass-medium-02/meshes/grass-medium-02-lod0-node4-mesh4-primitive0-material0-grass-medium-02.aemesh",23u,Kind::Copy,63160ull,"6B95323ACE660F307C8C1EBAB617B2D87118E773FAB4925B4B452B232389A689"},
    {"mistwatch-forest/LICENSE.md","engine/assets/realistic-cc0-first-batch/LICENSE.md",63u,Kind::Copy,523ull,"8232D8364958A2C530272968194F55D9F306099C3C9F3D8BE5BEC741A9CD1826"},
    {"mistwatch-forest/materials/bark-arm.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/fir-tree-01-c/bark-arm.dds",63u,Kind::Copy,1398276ull,"A1D895BAA855D31DD705E10BE6B3C8ED56E2E663665B8A7001C0CF39E128A3E8"},
    {"mistwatch-forest/materials/bark-base.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/fir-tree-01-c/bark-base.dds",63u,Kind::Copy,1398276ull,"49FD9058E6A5059E6BCED88360E44D0BCF303A164A3C0F4C08A151DDB236015A"},
    {"mistwatch-forest/materials/bark-normal.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/fir-tree-01-c/bark-normal.dds",63u,Kind::Copy,1398276ull,"21291B807A30761309F810086D6732B752546C951E2A2A091101DD4F7CDB29F3"},
    {"mistwatch-forest/materials/dead-branches-arm.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/fir-tree-01-c/dead-branches-arm.dds",63u,Kind::Copy,1398276ull,"A1D895BAA855D31DD705E10BE6B3C8ED56E2E663665B8A7001C0CF39E128A3E8"},
    {"mistwatch-forest/materials/dead-branches-base.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/fir-tree-01-c/dead-branches-base.dds",63u,Kind::Copy,1398276ull,"49FD9058E6A5059E6BCED88360E44D0BCF303A164A3C0F4C08A151DDB236015A"},
    {"mistwatch-forest/materials/dead-branches-normal.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/fir-tree-01-c/dead-branches-normal.dds",63u,Kind::Copy,1398276ull,"21291B807A30761309F810086D6732B752546C951E2A2A091101DD4F7CDB29F3"},
    {"mistwatch-forest/materials/material-bake-receipt.json","engine/assets/realistic-cc0-first-batch/cooked/materials/fir-tree-01-c/material-bake-receipt.json",63u,Kind::Copy,15102ull,"444470381E7A6ABFC3398C56E0D11A8C558B69D4C55CEF54F0E436C568D45919"},
    {"mistwatch-forest/materials/trunk-c-arm.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/fir-tree-01-c/trunk-c-arm.dds",63u,Kind::Copy,1398276ull,"6B894F1C1DA10361F2F07C1211CB1902342FBDEE10AFEF66C03150C995836DEE"},
    {"mistwatch-forest/materials/trunk-c-base.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/fir-tree-01-c/trunk-c-base.dds",63u,Kind::Copy,1398276ull,"C5F308B9A1EBC6FBF4A1EB44750309E1CC191FD829BFDC50098D69B817F8A168"},
    {"mistwatch-forest/materials/trunk-c-normal.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/fir-tree-01-c/trunk-c-normal.dds",63u,Kind::Copy,1398276ull,"22726FAAAB8C104237FFE7C8F17CCFA50637561222B4148B61EDA0B0FCC03344"},
    {"mistwatch-forest/materials/twig-arm.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/fir-tree-01-c/twig-arm.dds",63u,Kind::Copy,1398276ull,"37AF8BA7A3A7F94EDB8A58F3BB716C978CA9631BD04027625EA5818BE0223E10"},
    {"mistwatch-forest/materials/twig-base.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/fir-tree-01-c/twig-base.dds",63u,Kind::Copy,1398276ull,"B1A458C5584B5FA40CE04144F341E013B1A999881BF17FFF2880032363D58D5E"},
    {"mistwatch-forest/materials/twig-normal.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/fir-tree-01-c/twig-normal.dds",63u,Kind::Copy,1398276ull,"3F656DFA0F462F4812C17FF0487261C770A637D8D7C97536C9D98A36F429D3BB"},
    {"mistwatch-forest/meshes/fir-tree-01-c-lod0-aemesh-receipt.json","engine/assets/realistic-cc0-first-batch/cooked/meshes/fir-tree-01-c/fir-tree-01-c-lod0-aemesh-receipt.json",63u,Kind::Copy,5359ull,"D7A20344C5B5EF2DBB872414D1C371EB3203B197E2FCB333D32DA1122D507C6F"},
    {"mistwatch-forest/meshes/fir-tree-01-c-lod0-node0-mesh0-primitive0-material0-fir-tree-01-bark.aemesh","engine/assets/realistic-cc0-first-batch/cooked/meshes/fir-tree-01-c/fir-tree-01-c-lod0-node0-mesh0-primitive0-material0-fir-tree-01-bark.aemesh",63u,Kind::Copy,274304ull,"F2623A7BBA3EE4D16A7325B196CC320DA9565AD0F5704D6C083BA2783BC704B0"},
    {"mistwatch-forest/meshes/fir-tree-01-c-lod0-node0-mesh0-primitive1-material1-fir-tree-01-twig.aemesh","engine/assets/realistic-cc0-first-batch/cooked/meshes/fir-tree-01-c/fir-tree-01-c-lod0-node0-mesh0-primitive1-material1-fir-tree-01-twig.aemesh",63u,Kind::Copy,450740ull,"C9A16431AC6687FAD5322C802B5A0EAD6064020A3DF8B097220DE1A18FF65FD1"},
    {"mistwatch-forest/meshes/fir-tree-01-c-lod0-node0-mesh0-primitive2-material2-fir-tree-01-dead-branches.aemesh","engine/assets/realistic-cc0-first-batch/cooked/meshes/fir-tree-01-c/fir-tree-01-c-lod0-node0-mesh0-primitive2-material2-fir-tree-01-dead-branches.aemesh",63u,Kind::Copy,103220ull,"C47F9FCB9719C2A7425B4E200BF25A64EA484CA8376409F1418429675DE34387"},
    {"mistwatch-forest/meshes/fir-tree-01-c-lod0-node0-mesh0-primitive3-material3-fir-tree-01-trunk-c.aemesh","engine/assets/realistic-cc0-first-batch/cooked/meshes/fir-tree-01-c/fir-tree-01-c-lod0-node0-mesh0-primitive3-material3-fir-tree-01-trunk-c.aemesh",63u,Kind::Copy,610948ull,"578838CD434E54DA61152241FBCBF2E43CDC3663783521D516886084667F3180"},
    {"mistwatch-forest/meshes/fir-tree-01-c-lod1-aemesh-receipt.json","engine/assets/realistic-cc0-first-batch/cooked/meshes/fir-tree-01-c/fir-tree-01-c-lod1-aemesh-receipt.json",63u,Kind::Copy,5355ull,"F1EBFC73ED4E3D37456F53A568E8C1A55B9A4EF803C917237482B999B7223C60"},
    {"mistwatch-forest/meshes/fir-tree-01-c-lod1-node0-mesh0-primitive0-material0-fir-tree-01-bark.aemesh","engine/assets/realistic-cc0-first-batch/cooked/meshes/fir-tree-01-c/fir-tree-01-c-lod1-node0-mesh0-primitive0-material0-fir-tree-01-bark.aemesh",63u,Kind::Copy,208204ull,"02084A8C2F9A4AEADB16D1A9438091D93920C4C9852658DA99D219E5A74F61A5"},
    {"mistwatch-forest/meshes/fir-tree-01-c-lod1-node0-mesh0-primitive1-material1-fir-tree-01-twig.aemesh","engine/assets/realistic-cc0-first-batch/cooked/meshes/fir-tree-01-c/fir-tree-01-c-lod1-node0-mesh0-primitive1-material1-fir-tree-01-twig.aemesh",63u,Kind::Copy,288464ull,"5C597F3D5722F61CB957689D3719B949E7550455D0A0012D254329B4FCADD8DA"},
    {"mistwatch-forest/meshes/fir-tree-01-c-lod1-node0-mesh0-primitive2-material2-fir-tree-01-dead-branches.aemesh","engine/assets/realistic-cc0-first-batch/cooked/meshes/fir-tree-01-c/fir-tree-01-c-lod1-node0-mesh0-primitive2-material2-fir-tree-01-dead-branches.aemesh",63u,Kind::Copy,35748ull,"DC3FE7F82E9BECC8BB23AA1AF9822B82DBD4687D868A3D8B776A198038E0F899"},
    {"mistwatch-forest/meshes/fir-tree-01-c-lod1-node0-mesh0-primitive3-material3-fir-tree-01-trunk-c.aemesh","engine/assets/realistic-cc0-first-batch/cooked/meshes/fir-tree-01-c/fir-tree-01-c-lod1-node0-mesh0-primitive3-material3-fir-tree-01-trunk-c.aemesh",63u,Kind::Copy,175520ull,"13DEC2D3F0332B83130EFE00AB0256B0EF83A955B86CA60D4F448E908BDA37A2"},
    {"mistwatch-forest/meshes/fir-tree-01-c-lod2-aemesh-receipt.json","engine/assets/realistic-cc0-first-batch/cooked/meshes/fir-tree-01-c/fir-tree-01-c-lod2-aemesh-receipt.json",63u,Kind::Copy,5351ull,"B10BB8B2938126C9D1F5071343E4F8E19984C3F7EE0A4E94B633D6EC75F4E620"},
    {"mistwatch-forest/meshes/fir-tree-01-c-lod2-node0-mesh0-primitive0-material0-fir-tree-01-bark.aemesh","engine/assets/realistic-cc0-first-batch/cooked/meshes/fir-tree-01-c/fir-tree-01-c-lod2-node0-mesh0-primitive0-material0-fir-tree-01-bark.aemesh",63u,Kind::Copy,136728ull,"55A5CCC2CBCA2078C97BAB46F6F98F2A33A0114CDB683280049E74955F78A4AB"},
    {"mistwatch-forest/meshes/fir-tree-01-c-lod2-node0-mesh0-primitive1-material1-fir-tree-01-twig.aemesh","engine/assets/realistic-cc0-first-batch/cooked/meshes/fir-tree-01-c/fir-tree-01-c-lod2-node0-mesh0-primitive1-material1-fir-tree-01-twig.aemesh",63u,Kind::Copy,83488ull,"2EC74FE514C152F03867A8D9D2D2C975D8C35E91AA3E2551602F72D1BDF219D4"},
    {"mistwatch-forest/meshes/fir-tree-01-c-lod2-node0-mesh0-primitive2-material2-fir-tree-01-dead-branches.aemesh","engine/assets/realistic-cc0-first-batch/cooked/meshes/fir-tree-01-c/fir-tree-01-c-lod2-node0-mesh0-primitive2-material2-fir-tree-01-dead-branches.aemesh",63u,Kind::Copy,2992ull,"2B31900122A30B74CF95A6DB04F7ABAC6875BBD7C8C3EC642223E9CEF93869F5"},
    {"mistwatch-forest/meshes/fir-tree-01-c-lod2-node0-mesh0-primitive3-material3-fir-tree-01-trunk-c.aemesh","engine/assets/realistic-cc0-first-batch/cooked/meshes/fir-tree-01-c/fir-tree-01-c-lod2-node0-mesh0-primitive3-material3-fir-tree-01-trunk-c.aemesh",63u,Kind::Copy,7348ull,"E5B84998EA8631F2B8E0DB1B8C2C90A21A22EBC996DEFAD90B61D96F7FB80532"},
    {"mistwatch-forest/runtime-asset-manifest.json","engine/assets/realistic-cc0-first-batch/runtime-asset-manifest.json",63u,Kind::Copy,41058ull,"0D5A6801CDA2AF0B3424FCE78658DC42529E5B758511B43289E6C997985C68DF"},
    {"mistwatch-grass/LICENSE.md","engine/assets/mistwatch-grass-medium-02/LICENSE.md",63u,Kind::Copy,1487ull,"9DAD5F4479BF63D6252E44275CB78CE4FDD74A700CA75D6C16F233746EFC4EF7"},
    {"mistwatch-grass/download-receipt.json","engine/assets/mistwatch-grass-medium-02/download-receipt.json",63u,Kind::Copy,2128ull,"B0D7A84B721CEE4EAB4E3F8F77399CFE4B0B51DC5D7F7A85C0CA783624077C4E"},
    {"mistwatch-grass/materials/grass-arm.dds","engine/assets/mistwatch-grass-medium-02/materials/grass-arm.dds",63u,Kind::Copy,5592580ull,"2BC94E52366159F6528689F5C813EC94E2BE0EE92D193582C947526EF0DE9B44"},
    {"mistwatch-grass/materials/grass-base.dds","engine/assets/mistwatch-grass-medium-02/materials/grass-base.dds",63u,Kind::Copy,5592580ull,"DB0AED48E90590D5E606CC3A29844398C015FA72FA1058641082DBB7DB5B4CA9"},
    {"mistwatch-grass/materials/grass-normal.dds","engine/assets/mistwatch-grass-medium-02/materials/grass-normal.dds",63u,Kind::Copy,5592580ull,"C4B2D6D7815092947B186DD57B99AF218506EF5FA0408D5E6B476802498836D5"},
    {"mistwatch-grass/meshes/grass-medium-02-lod0-node0-mesh0-primitive0-material0-grass-medium-02.aemesh","engine/assets/mistwatch-grass-medium-02/meshes/grass-medium-02-lod0-node0-mesh0-primitive0-material0-grass-medium-02.aemesh",63u,Kind::Copy,26644ull,"84C8C7DA8EEB335B1BD60415531A30798933C688FF0D3A241ECA1958FB9C2E56"},
    {"mistwatch-grass/meshes/grass-medium-02-lod0-node1-mesh1-primitive0-material0-grass-medium-02.aemesh","engine/assets/mistwatch-grass-medium-02/meshes/grass-medium-02-lod0-node1-mesh1-primitive0-material0-grass-medium-02.aemesh",63u,Kind::Copy,65236ull,"CE3EB8A245F7C36682FCABB106EBB56E79BEAE95D6345DF330AC2D2DD5F76D05"},
    {"mistwatch-grass/meshes/grass-medium-02-lod0-node2-mesh2-primitive0-material0-grass-medium-02.aemesh","engine/assets/mistwatch-grass-medium-02/meshes/grass-medium-02-lod0-node2-mesh2-primitive0-material0-grass-medium-02.aemesh",63u,Kind::Copy,44612ull,"0AA49B54653A3B69A266CC7575AC8FF405A968E220BAC500A6BA3FAA3D432363"},
    {"mistwatch-grass/meshes/grass-medium-02-lod0-node3-mesh3-primitive0-material0-grass-medium-02.aemesh","engine/assets/mistwatch-grass-medium-02/meshes/grass-medium-02-lod0-node3-mesh3-primitive0-material0-grass-medium-02.aemesh",63u,Kind::Copy,91400ull,"78ED34130EAC919ED41F02918676E68D4060713C52C570B9E4ABEBCD1AA47C5B"},
    {"mistwatch-grass/meshes/grass-medium-02-lod0-node4-mesh4-primitive0-material0-grass-medium-02.aemesh","engine/assets/mistwatch-grass-medium-02/meshes/grass-medium-02-lod0-node4-mesh4-primitive0-material0-grass-medium-02.aemesh",63u,Kind::Copy,63160ull,"6B95323ACE660F307C8C1EBAB617B2D87118E773FAB4925B4B452B232389A689"},
    {"mistwatch-materials/FOREST_GROUND_LICENSE.md","engine/assets/realistic-cc0-first-batch/LICENSE.md",63u,Kind::Copy,523ull,"8232D8364958A2C530272968194F55D9F306099C3C9F3D8BE5BEC741A9CD1826"},
    {"mistwatch-materials/GROUND_SOIL_LICENSE.md","engine/assets/realistic-cc0-first-batch/LICENSE.md",63u,Kind::Copy,523ull,"8232D8364958A2C530272968194F55D9F306099C3C9F3D8BE5BEC741A9CD1826"},
    {"mistwatch-materials/LICENSE.md","engine/assets/material-packs/LICENSE.md",63u,Kind::Copy,865ull,"D81BC4A3F3B4FCC401182F7C307E601671E5A5E264EAEC47F1C7BBCA2C1E409D"},
    {"mistwatch-materials/ambientcg/ground037/1k/Ground037.png","engine/assets/material-packs/ambientcg/ground037/1k/Ground037.png",63u,Kind::Copy,513329ull,"BC7CEBB2860A92E9446802A3018F56074879803F1BDDB0B80517CD5BFD57C6AE"},
    {"mistwatch-materials/ambientcg/ground037/1k/Ground037_1K-JPG.blend","engine/assets/material-packs/ambientcg/ground037/1k/Ground037_1K-JPG.blend",63u,Kind::Copy,1109220ull,"980FEED3DFE7AADB2B2EF1A917E5AC6AFC5C92D96AD3152B43EB03018E554F85"},
    {"mistwatch-materials/ambientcg/ground037/1k/Ground037_1K-JPG.mtlx","engine/assets/material-packs/ambientcg/ground037/1k/Ground037_1K-JPG.mtlx",63u,Kind::Copy,2307ull,"D00C32BAD302D4616D2BE47FB9CB34A97845F8B7609F13B29197CC448A61BFBF"},
    {"mistwatch-materials/ambientcg/ground037/1k/Ground037_1K-JPG.tres","engine/assets/material-packs/ambientcg/ground037/1k/Ground037_1K-JPG.tres",63u,Kind::Copy,1039ull,"8F917AE493E419A7F6FF7ACEBEEEA239C270D17F5399B0D20C8A589187B80BAD"},
    {"mistwatch-materials/ambientcg/ground037/1k/Ground037_1K-JPG.usdc","engine/assets/material-packs/ambientcg/ground037/1k/Ground037_1K-JPG.usdc",63u,Kind::Copy,2982ull,"3B3C481B29DFB3C862AD36CFEBC86C6AB0902A818262D67ABE52C6D91E9A2377"},
    {"mistwatch-materials/ambientcg/ground037/1k/Ground037_1K-JPG_AmbientOcclusion.jpg","engine/assets/material-packs/ambientcg/ground037/1k/Ground037_1K-JPG_AmbientOcclusion.jpg",63u,Kind::Copy,612112ull,"8AFE189AE915888551076CB279F6282C84255E4B2BDB2C4CE8C1C891074245EF"},
    {"mistwatch-materials/ambientcg/ground037/1k/Ground037_1K-JPG_Color.jpg","engine/assets/material-packs/ambientcg/ground037/1k/Ground037_1K-JPG_Color.jpg",63u,Kind::Copy,2187764ull,"D92BEC8A35B9CB16EC79B421CA0F572A7400286EAE109BEAFA266629EF49A6CF"},
    {"mistwatch-materials/ambientcg/ground037/1k/Ground037_1K-JPG_Displacement.jpg","engine/assets/material-packs/ambientcg/ground037/1k/Ground037_1K-JPG_Displacement.jpg",63u,Kind::Copy,500481ull,"BC102168F5D014CE208ED5C1D9140C39513D29303E1B3436CB06C92C7780E419"},
    {"mistwatch-materials/ambientcg/ground037/1k/Ground037_1K-JPG_NormalDX.jpg","engine/assets/material-packs/ambientcg/ground037/1k/Ground037_1K-JPG_NormalDX.jpg",63u,Kind::Copy,2474498ull,"0E2D613D117D9231AA0AA61F39B19755F891B4ED1D1599A004CB018A8224E039"},
    {"mistwatch-materials/ambientcg/ground037/1k/Ground037_1K-JPG_NormalGL.jpg","engine/assets/material-packs/ambientcg/ground037/1k/Ground037_1K-JPG_NormalGL.jpg",63u,Kind::Copy,2474236ull,"EA59DBDCFA5BF2F28BDD4AA896D4E5F96C4CCE5939100DCC6DB4BBBCDEBFE38D"},
    {"mistwatch-materials/ambientcg/ground037/1k/Ground037_1K-JPG_Roughness.jpg","engine/assets/material-packs/ambientcg/ground037/1k/Ground037_1K-JPG_Roughness.jpg",63u,Kind::Copy,694944ull,"3786B088E981AAE562F06B6477972D379CFFD09A5CC542AB42F9FFD94F9BCB0E"},
    {"mistwatch-materials/ambientcg/ground108/slot-0-arm.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/ground108/slot-0-arm.dds",63u,Kind::Copy,1398276ull,"CE0C636264A9C1C5CB5055259DDE80B92C9BEA3145DC8EA8F28A7F9F3903E5ED"},
    {"mistwatch-materials/ambientcg/ground108/slot-0-base.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/ground108/slot-0-base.dds",63u,Kind::Copy,1398276ull,"2703B20EE77D556DAA64535AF15433BB3157083174E6D71671AA052CCAE11589"},
    {"mistwatch-materials/ambientcg/ground108/slot-0-normal.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/ground108/slot-0-normal.dds",63u,Kind::Copy,1398276ull,"E63EFD69F4D1E7978F1CD152C5B2115151BCBC208F9F661BC35AF0EC6AF80032"},
    {"mistwatch-materials/ambientcg/woodsiding006/1k/WoodSiding006.png","engine/assets/material-packs/ambientcg/woodsiding006/1k/WoodSiding006.png",63u,Kind::Copy,304302ull,"B0F5C8FB01F5249F70574DAB2FE7E7AFA4CD90B0BEFF9F607C172A47FBB4F2CD"},
    {"mistwatch-materials/ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG.blend","engine/assets/material-packs/ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG.blend",63u,Kind::Copy,1110260ull,"72925D4E752B1F2FA1A217AF2A77D02FDF53C89A4A177E4E8226BD9AEC8C2E39"},
    {"mistwatch-materials/ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG.mtlx","engine/assets/material-packs/ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG.mtlx",63u,Kind::Copy,2367ull,"DC1AC65DBCA30FFCAEAED35C6DD45EEC6A5DD12C679CE13129E9623D7554948D"},
    {"mistwatch-materials/ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG.tres","engine/assets/material-packs/ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG.tres",63u,Kind::Copy,1059ull,"C78565C440157D8E11707070A4F6BF33A1B2A1DA7FF7B49913D08444185BC721"},
    {"mistwatch-materials/ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG.usdc","engine/assets/material-packs/ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG.usdc",63u,Kind::Copy,3050ull,"4C00FAA28CB21C2C299B1F8913A80356FE0F8C978DC14173F87562B96F3AF799"},
    {"mistwatch-materials/ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG_AmbientOcclusion.jpg","engine/assets/material-packs/ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG_AmbientOcclusion.jpg",63u,Kind::Copy,127417ull,"61BDA7CF59ADD9C21A15C18328E4391BE3CFD8101986F058A5345D7BB39D3C60"},
    {"mistwatch-materials/ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG_Color.jpg","engine/assets/material-packs/ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG_Color.jpg",63u,Kind::Copy,724730ull,"C626EC1B31055A85E8E09D42CE2D0C6F0B968250AD6672EA3282B136DD568D46"},
    {"mistwatch-materials/ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG_Displacement.jpg","engine/assets/material-packs/ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG_Displacement.jpg",63u,Kind::Copy,285699ull,"51CF0B44D8A13AB05222C723AB06016B821DAC665CE7ECE64BDF150829EF3210"},
    {"mistwatch-materials/ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG_NormalDX.jpg","engine/assets/material-packs/ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG_NormalDX.jpg",63u,Kind::Copy,1849769ull,"55895492F8B92A1D847201E76D339679097D0901F7B3FBF9BBCD1C740B05F3FB"},
    {"mistwatch-materials/ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG_NormalGL.jpg","engine/assets/material-packs/ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG_NormalGL.jpg",63u,Kind::Copy,1847106ull,"C319E4180FC6261BF19753D597BC64189F9CD721DE4328FA4D67FFF8D1B71F32"},
    {"mistwatch-materials/ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG_Roughness.jpg","engine/assets/material-packs/ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG_Roughness.jpg",63u,Kind::Copy,423700ull,"ADC51758DDAF97BCD795EEF154F705A4F020594EFC5A5B515D0F92B39240C844"},
    {"mistwatch-materials/forest-ground-render-manifest.json","engine/assets/realistic-cc0-first-batch/cooked/runtime-render-manifest.json",63u,Kind::Copy,129864ull,"3A28A543F02673E515164A002D000E6CAF8BAFD9849568E3EE9C024342F40D4D"},
    {"mistwatch-materials/ground-soil-render-manifest.json","engine/assets/realistic-cc0-first-batch/cooked/runtime-render-manifest.json",63u,Kind::Copy,129864ull,"3A28A543F02673E515164A002D000E6CAF8BAFD9849568E3EE9C024342F40D4D"},
    {"mistwatch-materials/material-packs.json","engine/assets/material-packs/material-packs.json",63u,Kind::Copy,3758ull,"70853E6F97A1489C02A428A62295F556815C510D42948FCD33E9D33897773ED4"},
    {"mistwatch-materials/poly-haven/bark_brown_02/1k/base-color.jpg","engine/assets/material-packs/poly-haven/bark_brown_02/1k/base-color.jpg",63u,Kind::Copy,659592ull,"920FA0BED0C9D78C1D530E99795113AFD532D7DB746DE440F4A85D7C83ED0F1A"},
    {"mistwatch-materials/poly-haven/bark_brown_02/1k/normal-dx.jpg","engine/assets/material-packs/poly-haven/bark_brown_02/1k/normal-dx.jpg",63u,Kind::Copy,1033312ull,"7FCF9B0EFA59DEE597CC9F7A367E3F4131D59F72BC6D37966B9A8E69C4A1BF74"},
    {"mistwatch-materials/poly-haven/bark_brown_02/1k/roughness.jpg","engine/assets/material-packs/poly-haven/bark_brown_02/1k/roughness.jpg",63u,Kind::Copy,337946ull,"68125592DE36E15BAE7AA6DB85C4D135AEB7E5653199BA226483A70F6D69837D"},
    {"mistwatch-materials/poly-haven/forest-ground-04/slot-0-arm.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/forest-ground-04/slot-0-arm.dds",63u,Kind::Copy,5592580ull,"3427BF0A83E0C33326A207F700305620402FD0FB6C532240F204BCEBEFEABCC4"},
    {"mistwatch-materials/poly-haven/forest-ground-04/slot-0-base.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/forest-ground-04/slot-0-base.dds",63u,Kind::Copy,5592580ull,"00850C7C033BE2BCB2818C0AB3DB91A62C8353EBA746DAEDE3C0DE373C3E32B4"},
    {"mistwatch-materials/poly-haven/forest-ground-04/slot-0-normal.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/forest-ground-04/slot-0-normal.dds",63u,Kind::Copy,5592580ull,"56E687B2A208F2EAAF9B0E3670530F0A36E5E88D6E74B26090633DAAA4E9DB93"},
    {"mistwatch-materials/poly-haven/forest_leaves_04/2k/forest_leaves_04_diff_2k.png","engine/assets/material-packs/poly-haven/forest_leaves_04/2k/forest_leaves_04_diff_2k.png",62u,Kind::Copy,10402792ull,"40B1786AFA17C9930CCAE93A9CA75880FC52FC34B96453489C767B2CC82D7EE9"},
    {"mistwatch-materials/poly-haven/forest_leaves_04/2k/forest_leaves_04_nor_dx_2k.png","engine/assets/material-packs/poly-haven/forest_leaves_04/2k/forest_leaves_04_nor_dx_2k.png",62u,Kind::Copy,11539651ull,"19BBAA0EB67EDA146129946B9605CFC9B472A302BA1F75EB8EA0B48AA21BAA33"},
    {"mistwatch-materials/poly-haven/forest_leaves_04/2k/forest_leaves_04_rough_2k.png","engine/assets/material-packs/poly-haven/forest_leaves_04/2k/forest_leaves_04_rough_2k.png",62u,Kind::Copy,6083648ull,"713874257D143AF49C482CE8B7648CB7C5C748CA6540DB0E44563A04A9FE09CE"},
    {"mistwatch-materials/poly-haven/forest_leaves_04/DERIVATION.txt","engine/assets/material-packs/poly-haven/forest_leaves_04/DERIVATION.txt",62u,Kind::Copy,1389ull,"F6138A356ABB5A458A5E6C08B50E17361C6DDC07738F2066A4AF8936E4BE811C"},
    {"mistwatch-materials/poly-haven/forest_leaves_04/LICENSE.txt","engine/assets/material-packs/poly-haven/forest_leaves_04/LICENSE.txt",62u,Kind::Copy,695ull,"5E440DE55C40EB4DAD49DCC0505E7854267FB10688793E58EA5167CC29A5A719"},
    {"mistwatch-materials/poly-haven/rock_surface/1k/base-color.jpg","engine/assets/material-packs/poly-haven/rock_surface/1k/base-color.jpg",63u,Kind::Copy,886291ull,"07EDDA2BBE4715B01AAE2E4BAC9F21F204CF4AAE37B4733EF01DE33281E5E60E"},
    {"mistwatch-materials/poly-haven/rock_surface/1k/normal-dx.jpg","engine/assets/material-packs/poly-haven/rock_surface/1k/normal-dx.jpg",63u,Kind::Copy,1161843ull,"1E1AA5B06F74D22734318CD93ECF49FFE7095C4B2307C5C9B3A4550207C6B1B7"},
    {"mistwatch-materials/poly-haven/rock_surface/1k/roughness.jpg","engine/assets/material-packs/poly-haven/rock_surface/1k/roughness.jpg",63u,Kind::Copy,642680ull,"CA710D895B01CF323BF00F53194C43504A39D2C964FBFC343C4F45C7D41B10DC"},
    {"mistwatch-materials/poly-haven/worn_planks/1k/base-color.jpg","engine/assets/material-packs/poly-haven/worn_planks/1k/base-color.jpg",63u,Kind::Copy,943056ull,"698C09B337375A9AAE12A76AD09FF2FDA11D735E6C1BBB18D72875BD61988DB6"},
    {"mistwatch-materials/poly-haven/worn_planks/1k/normal-dx.png","engine/assets/material-packs/poly-haven/worn_planks/1k/normal-dx.png",63u,Kind::Copy,4886175ull,"743411ACF7332D8176D4E402237BB4B2D28A89FDC9BA160BB1EEE2DE6FBFB74B"},
    {"mistwatch-materials/poly-haven/worn_planks/1k/roughness.jpg","engine/assets/material-packs/poly-haven/worn_planks/1k/roughness.jpg",63u,Kind::Copy,807719ull,"C79F640DC2D7EF81D69DE463EF6676B7C540D598FB0E462A6EB39EB5462770A9"},
    {"mistwatch-nettle/LICENSE.txt","engine/assets/mistwatch-nettle-plant/LICENSE.txt",60u,Kind::Copy,1893ull,"D5DB9DE2BD5EA05ED4DB61C9CAE1C1828B66F345BABEAE8E899E2FCD7DB373EF"},
    {"mistwatch-nettle/materials/nettle-arm.dds","engine/assets/mistwatch-nettle-plant/materials/nettle-arm.dds",60u,Kind::Copy,5592580ull,"895365555A197CF5F86FEB088858CB0700CFC27F0BA05477BBD9407783CBACD0"},
    {"mistwatch-nettle/materials/nettle-base.dds","engine/assets/mistwatch-nettle-plant/materials/nettle-base.dds",60u,Kind::Copy,5592580ull,"1E875AA8967262DFEF1CB8B3B372527EEE308071948865489D263630FB826FB1"},
    {"mistwatch-nettle/materials/nettle-normal.dds","engine/assets/mistwatch-nettle-plant/materials/nettle-normal.dds",60u,Kind::Copy,5592580ull,"85729C0B6920F4D2C640C7FC038506C9071450DE103A44CA2B6F2BDE5451790F"},
    {"mistwatch-nettle/meshes/nettle-plant-lod0-node0-mesh0-primitive0-material0-nettle-plant.aemesh","engine/assets/mistwatch-nettle-plant/meshes/nettle-plant-lod0-node0-mesh0-primitive0-material0-nettle-plant.aemesh",60u,Kind::Copy,199148ull,"91556C49EB48AC45EE556975A08E4C31B4EC25B94E80208751C62FEB2F7EF195"},
    {"mistwatch-nettle/meshes/nettle-plant-lod0-node1-mesh1-primitive0-material0-nettle-plant.aemesh","engine/assets/mistwatch-nettle-plant/meshes/nettle-plant-lod0-node1-mesh1-primitive0-material0-nettle-plant.aemesh",60u,Kind::Copy,189392ull,"4DA84D00D7EE5C86A3396D0522518661EB98E0A2E589907B3CFE6B4864D8117F"},
    {"mistwatch-nettle/meshes/nettle-plant-lod0-node2-mesh2-primitive0-material0-nettle-plant.aemesh","engine/assets/mistwatch-nettle-plant/meshes/nettle-plant-lod0-node2-mesh2-primitive0-material0-nettle-plant.aemesh",60u,Kind::Copy,119728ull,"5C5D1C571D6FFD0CCF75D969C37ED34D8164D127C89EF28D2F94957548AB44D6"},
    {"mistwatch-nettle/meshes/nettle-plant-lod0-node3-mesh3-primitive0-material0-nettle-plant.aemesh","engine/assets/mistwatch-nettle-plant/meshes/nettle-plant-lod0-node3-mesh3-primitive0-material0-nettle-plant.aemesh",60u,Kind::Copy,36844ull,"550AB1456DF945E1AF708B3D3B5C8613FE9D8FDCDE90CA4FD5AA69655BE706BC"},
    {"mistwatch-nettle/meshes/nettle-plant-lod0-node4-mesh4-primitive0-material0-nettle-plant.aemesh","engine/assets/mistwatch-nettle-plant/meshes/nettle-plant-lod0-node4-mesh4-primitive0-material0-nettle-plant.aemesh",60u,Kind::Copy,231076ull,"D1043C22537C243329BD8610EC14692234F54AF0FD5AE49132D0B7540F550E58"},
    {"mistwatch-nettle/meshes/nettle-plant-lod0-node5-mesh5-primitive0-material0-nettle-plant.aemesh","engine/assets/mistwatch-nettle-plant/meshes/nettle-plant-lod0-node5-mesh5-primitive0-material0-nettle-plant.aemesh",60u,Kind::Copy,272200ull,"ADDE7AA47A39F321ED4FA8AB8BC336D4E42EF319FF6F7CEF3BAF068F65F53369"},
    {"mistwatch-wildlife/boar.aeskin","",63u,Kind::Boar,329520ull,"DFABA5E98F00D4CB86363A14E93B20A3DAF88B49C93A2972254C7FFC605DC8AC"},
    {"model-packs/CC0-HUMAN-BASE-MESHES.md","games/character_lab/assets/model-packs/CC0-HUMAN-BASE-MESHES.md",23u,Kind::Copy,519ull,"F6DE026C9A4DDF7559FB2E03251EFE7B4C962A28D3F93115C5EEFCF5B78A31B9"},
    {"model-packs/LICENSE.md","engine/assets/model-packs/LICENSE.md",23u,Kind::Copy,326ull,"5F80075030A5CD5B6B50315D103AB78B30E9C936C2773F4816E0F131BE622079"},
    {"model-packs/model-packs.json","engine/assets/model-packs/model-packs.json",23u,Kind::Copy,1087ull,"CB19378913FA4C5F797FF6981E90259AF0EBFED632249DC13C7DC3DC592ADACE"},
    {"model-packs/poly-haven/quiver_tree_02/1k/base-color.jpg","engine/assets/model-packs/poly-haven/quiver_tree_02/1k/base-color.jpg",23u,Kind::Copy,736321ull,"546A745AD765529D3C310178321E145909F4817B06679788AD24D29EC7EAE7B7"},
    {"model-packs/poly-haven/quiver_tree_02/1k/normal-dx.jpg","engine/assets/model-packs/poly-haven/quiver_tree_02/1k/normal-dx.jpg",23u,Kind::Copy,993320ull,"EEFA8B6C50BC40E8BA5EB4AA3092882625954DE0E352F4C510AEB8D394412238"},
    {"model-packs/poly-haven/quiver_tree_02/1k/roughness.jpg","engine/assets/model-packs/poly-haven/quiver_tree_02/1k/roughness.jpg",23u,Kind::Copy,441029ull,"076EF877C8D4CF27B0B37B974A8C7AACC21676020FD61D38CDC9CF0B330D3A24"},
    {"model-packs/poly-haven/quiver_tree_02/quiver_tree_02.aemesh","engine/assets/model-packs/poly-haven/quiver_tree_02/quiver_tree_02.aemesh",23u,Kind::Copy,2357044ull,"ADC034B356E7A381F4CC69F911AE7F4E2637ADEF47A0AE482DE356C37BAF8142"},
    {"model-packs/poly-haven/rock_09/1k/base-color.jpg","engine/assets/model-packs/poly-haven/rock_09/1k/base-color.jpg",23u,Kind::Copy,595103ull,"AFB7AE5AE82238D304ABA589391AC6BC87F7CF1DBFE50980C21F762CFDD87EFD"},
    {"model-packs/poly-haven/rock_09/1k/normal-dx.jpg","engine/assets/model-packs/poly-haven/rock_09/1k/normal-dx.jpg",23u,Kind::Copy,747331ull,"97AB8B59D38C40602851D13F7641E2773146C617CB57D4CF573398B48AB0DA9A"},
    {"model-packs/poly-haven/rock_09/1k/roughness.jpg","engine/assets/model-packs/poly-haven/rock_09/1k/roughness.jpg",23u,Kind::Copy,445567ull,"A32A88790A2F5A1E2A89FAAA2CAEADC7C89453B49916495DBF2A88CD0A8F39EF"},
    {"model-packs/poly-haven/rock_09/rock_09.aemesh","engine/assets/model-packs/poly-haven/rock_09/rock_09.aemesh",23u,Kind::Copy,334592ull,"968B5CF056071C8926E425DA6E3A557214942F4DFE97653500975A6AB85EE548"},
    {"model-packs/realistic-female/realistic-female.aeskin","games/character_lab/assets/model-packs/realistic-female/realistic-female.aeskin",23u,Kind::Copy,1480232ull,"1554EDD3215F2DD697288B1189B24A2247637A61D594AC60B4589FF45F082C07"},
    {"model-packs/realistic-female/realistic-female.provenance.json","games/character_lab/assets/model-packs/realistic-female/realistic-female.provenance.json",23u,Kind::Copy,2353ull,"0BC845EE76105CF25E6438670DE94795CCFF669B3E1F86BC9775CA43C8C9184E"},
    {"model-packs/realistic-female/source/realistic-female.bin","games/character_lab/assets/model-packs/realistic-female/source/realistic-female.bin",23u,Kind::Copy,1944160ull,"DC4DEFDACDD1904C4117CBC615FE48C6A357974F2C7507F17AA87700FB64A970"},
    {"model-packs/realistic-female/source/realistic-female.gltf","games/character_lab/assets/model-packs/realistic-female/source/realistic-female.gltf",23u,Kind::Copy,113690ull,"D92A9F7F996660A7FA193B9527E2C435B069ED4FDEE3565D594934B8D4FFE4FF"},
    {"model-packs/realistic-male/realistic-male.aeskin","games/character_lab/assets/model-packs/realistic-male/realistic-male.aeskin",23u,Kind::Copy,1473090ull,"4AFD8736F47541F37C3BA4F723A039BE74FF0B8B8424E633A8F97F979E5CA034"},
    {"photoreal-cooked/ibl/environment-brdf.dds","engine/assets/realistic-cc0-first-batch/cooked/ibl/environment-brdf.dds",23u,Kind::Copy,65684ull,"B36B75CC849301004B795F89A1DAED5F5F78B8686B93679EF15B763006F2B2F6"},
    {"photoreal-cooked/ibl/environment-irradiance.dds","engine/assets/realistic-cc0-first-batch/cooked/ibl/environment-irradiance.dds",23u,Kind::Copy,8500ull,"B058AA7278FC3127ACE7685E9710A613E904C18337F3EE676D5D1647FAA9DE5E"},
    {"photoreal-cooked/ibl/environment-specular.dds","engine/assets/realistic-cc0-first-batch/cooked/ibl/environment-specular.dds",23u,Kind::Copy,131380ull,"6FBA886F002D6FCD9FDDA655C0C604611BCB6C1F9C6B5E0D67AB255282A6FAD1"},
    {"photoreal-cooked/materials/boulder-01/slot-0-arm.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/boulder-01/slot-0-arm.dds",23u,Kind::Copy,1398276ull,"564BE8685C266FE7FA03ACDA76C6C06F706911C4344777CEAB6563D743284A1E"},
    {"photoreal-cooked/materials/boulder-01/slot-0-base.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/boulder-01/slot-0-base.dds",23u,Kind::Copy,1398276ull,"5A847A3174E09505B3A404C93716CBAE576A5CB8BD32F69EC6395E8C95008D36"},
    {"photoreal-cooked/materials/boulder-01/slot-0-normal.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/boulder-01/slot-0-normal.dds",23u,Kind::Copy,1398276ull,"68FBBE04FFDB1C6D327F4262B9230438623F0D3B4B17A0A5155EB13B9ED21E83"},
    {"photoreal-cooked/materials/bricks100/slot-0-arm.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/bricks100/slot-0-arm.dds",23u,Kind::Copy,699236ull,"476CD54C699996F73BBB11FAF0E9D93A3C2C9C69DF330E348FBC718EEDA3D914"},
    {"photoreal-cooked/materials/bricks100/slot-0-base.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/bricks100/slot-0-base.dds",23u,Kind::Copy,699236ull,"90F9E96AF5FD5D95CFD374E933381055E42AF319895E9C24C9764C1B87657676"},
    {"photoreal-cooked/materials/bricks100/slot-0-normal.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/bricks100/slot-0-normal.dds",23u,Kind::Copy,699236ull,"733119AFB13DFFEF439275247D75B5D80E21929C538BDB7B9CDBABEAB411D6FB"},
    {"photoreal-cooked/materials/coastal-cliff-01/slot-0-arm.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/coastal-cliff-01/slot-0-arm.dds",23u,Kind::Copy,1398276ull,"A9262B140284F58B1574E661C842D026BF76A5596EF561F70B1DFDDD34080E02"},
    {"photoreal-cooked/materials/coastal-cliff-01/slot-0-base.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/coastal-cliff-01/slot-0-base.dds",23u,Kind::Copy,1398276ull,"410DD7AD59FAC581D97C3D11DCA6117C4697F8ABCB32C5478085421B2585A38C"},
    {"photoreal-cooked/materials/coastal-cliff-01/slot-0-normal.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/coastal-cliff-01/slot-0-normal.dds",23u,Kind::Copy,1398276ull,"93C7E9DA6ACB5D9237E322B15AAE6E907C7B65EF37FD698BBF103A31A12BD09F"},
    {"photoreal-cooked/materials/dead-tree-trunk-02/slot-0-arm.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/dead-tree-trunk-02/slot-0-arm.dds",23u,Kind::Copy,1398276ull,"A79AAE7247FB82577D6434C36EFE0EEF02793DFF0DE91AE411ECD1EABC593546"},
    {"photoreal-cooked/materials/dead-tree-trunk-02/slot-0-base.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/dead-tree-trunk-02/slot-0-base.dds",23u,Kind::Copy,1398276ull,"EC34D15DE64C18940CF08A70442E367118AC7583ABFD1136329037099B52D5C6"},
    {"photoreal-cooked/materials/dead-tree-trunk-02/slot-0-normal.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/dead-tree-trunk-02/slot-0-normal.dds",23u,Kind::Copy,1398276ull,"6D353D62F9120E13B331BCD66E1C2716E145FFED5F530170310E45DEC25C567E"},
    {"photoreal-cooked/materials/fir-tree-01-c/bark-arm.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/fir-tree-01-c/bark-arm.dds",23u,Kind::Copy,1398276ull,"A1D895BAA855D31DD705E10BE6B3C8ED56E2E663665B8A7001C0CF39E128A3E8"},
    {"photoreal-cooked/materials/fir-tree-01-c/bark-base.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/fir-tree-01-c/bark-base.dds",23u,Kind::Copy,1398276ull,"49FD9058E6A5059E6BCED88360E44D0BCF303A164A3C0F4C08A151DDB236015A"},
    {"photoreal-cooked/materials/fir-tree-01-c/bark-normal.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/fir-tree-01-c/bark-normal.dds",23u,Kind::Copy,1398276ull,"21291B807A30761309F810086D6732B752546C951E2A2A091101DD4F7CDB29F3"},
    {"photoreal-cooked/materials/fir-tree-01-c/dead-branches-arm.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/fir-tree-01-c/dead-branches-arm.dds",23u,Kind::Copy,1398276ull,"A1D895BAA855D31DD705E10BE6B3C8ED56E2E663665B8A7001C0CF39E128A3E8"},
    {"photoreal-cooked/materials/fir-tree-01-c/dead-branches-base.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/fir-tree-01-c/dead-branches-base.dds",23u,Kind::Copy,1398276ull,"49FD9058E6A5059E6BCED88360E44D0BCF303A164A3C0F4C08A151DDB236015A"},
    {"photoreal-cooked/materials/fir-tree-01-c/dead-branches-normal.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/fir-tree-01-c/dead-branches-normal.dds",23u,Kind::Copy,1398276ull,"21291B807A30761309F810086D6732B752546C951E2A2A091101DD4F7CDB29F3"},
    {"photoreal-cooked/materials/fir-tree-01-c/trunk-c-arm.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/fir-tree-01-c/trunk-c-arm.dds",23u,Kind::Copy,1398276ull,"6B894F1C1DA10361F2F07C1211CB1902342FBDEE10AFEF66C03150C995836DEE"},
    {"photoreal-cooked/materials/fir-tree-01-c/trunk-c-base.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/fir-tree-01-c/trunk-c-base.dds",23u,Kind::Copy,1398276ull,"C5F308B9A1EBC6FBF4A1EB44750309E1CC191FD829BFDC50098D69B817F8A168"},
    {"photoreal-cooked/materials/fir-tree-01-c/trunk-c-normal.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/fir-tree-01-c/trunk-c-normal.dds",23u,Kind::Copy,1398276ull,"22726FAAAB8C104237FFE7C8F17CCFA50637561222B4148B61EDA0B0FCC03344"},
    {"photoreal-cooked/materials/fir-tree-01-c/twig-arm.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/fir-tree-01-c/twig-arm.dds",23u,Kind::Copy,1398276ull,"37AF8BA7A3A7F94EDB8A58F3BB716C978CA9631BD04027625EA5818BE0223E10"},
    {"photoreal-cooked/materials/fir-tree-01-c/twig-base.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/fir-tree-01-c/twig-base.dds",23u,Kind::Copy,1398276ull,"B1A458C5584B5FA40CE04144F341E013B1A999881BF17FFF2880032363D58D5E"},
    {"photoreal-cooked/materials/fir-tree-01-c/twig-normal.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/fir-tree-01-c/twig-normal.dds",23u,Kind::Copy,1398276ull,"3F656DFA0F462F4812C17FF0487261C770A637D8D7C97536C9D98A36F429D3BB"},
    {"photoreal-cooked/materials/forest-ground-04/slot-0-arm.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/forest-ground-04/slot-0-arm.dds",23u,Kind::Copy,5592580ull,"3427BF0A83E0C33326A207F700305620402FD0FB6C532240F204BCEBEFEABCC4"},
    {"photoreal-cooked/materials/forest-ground-04/slot-0-base.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/forest-ground-04/slot-0-base.dds",23u,Kind::Copy,5592580ull,"00850C7C033BE2BCB2818C0AB3DB91A62C8353EBA746DAEDE3C0DE373C3E32B4"},
    {"photoreal-cooked/materials/forest-ground-04/slot-0-normal.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/forest-ground-04/slot-0-normal.dds",23u,Kind::Copy,5592580ull,"56E687B2A208F2EAAF9B0E3670530F0A36E5E88D6E74B26090633DAAA4E9DB93"},
    {"photoreal-cooked/materials/ground108/slot-0-arm.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/ground108/slot-0-arm.dds",23u,Kind::Copy,1398276ull,"CE0C636264A9C1C5CB5055259DDE80B92C9BEA3145DC8EA8F28A7F9F3903E5ED"},
    {"photoreal-cooked/materials/ground108/slot-0-base.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/ground108/slot-0-base.dds",23u,Kind::Copy,1398276ull,"2703B20EE77D556DAA64535AF15433BB3157083174E6D71671AA052CCAE11589"},
    {"photoreal-cooked/materials/ground108/slot-0-normal.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/ground108/slot-0-normal.dds",23u,Kind::Copy,1398276ull,"E63EFD69F4D1E7978F1CD152C5B2115151BCBC208F9F661BC35AF0EC6AF80032"},
    {"photoreal-cooked/materials/island-tree-01/branches-arm.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/island-tree-01/branches-arm.dds",23u,Kind::Copy,1398276ull,"92E5AE39785DBCEFC885588DEAAFD35A1B45D7E5E47F222320B85CB9CE810F05"},
    {"photoreal-cooked/materials/island-tree-01/branches-base.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/island-tree-01/branches-base.dds",23u,Kind::Copy,1398276ull,"75E25268369704A561432DF6F3368734E8B5648FDCE184A0A4C2C5863317145B"},
    {"photoreal-cooked/materials/island-tree-01/branches-normal.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/island-tree-01/branches-normal.dds",23u,Kind::Copy,1398276ull,"1AA3118C6FBA3891C31FB2E5C961EDD463E2A65084F611158DEA4ADC3E39DBBB"},
    {"photoreal-cooked/materials/island-tree-01/leaves-arm.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/island-tree-01/leaves-arm.dds",23u,Kind::Copy,1398276ull,"792095487B7B49038DA96CA9CB9E417A52B29F98EB109D953EF4EEDCB82E9013"},
    {"photoreal-cooked/materials/island-tree-01/leaves-base.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/island-tree-01/leaves-base.dds",23u,Kind::Copy,1398276ull,"4A1F4E5544D2B91ED3822455F457B93A4D3237019BC8AE6202CE1E0AEF9F14C2"},
    {"photoreal-cooked/materials/island-tree-01/leaves-normal.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/island-tree-01/leaves-normal.dds",23u,Kind::Copy,1398276ull,"CC43D9A2635A0F8A6A750E41CC9857600A16D39FE9EDC2D7F2403D39790595EE"},
    {"photoreal-cooked/materials/island-tree-01/trunk-arm.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/island-tree-01/trunk-arm.dds",23u,Kind::Copy,1398276ull,"638E6710501BB04D9626294B182246F7E651970B6EF2F791EFB941E2FAD9AAA4"},
    {"photoreal-cooked/materials/island-tree-01/trunk-base.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/island-tree-01/trunk-base.dds",23u,Kind::Copy,1398276ull,"AA3708B589C5F80DDDB98022EEA49FBB71EB157B9EA7C32A02B3F0111E03C98D"},
    {"photoreal-cooked/materials/island-tree-01/trunk-normal.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/island-tree-01/trunk-normal.dds",23u,Kind::Copy,1398276ull,"24EBA3FCB9E8D59B088E9CF312BDF25B32445562CE466262AA1D7834CD17BB7A"},
    {"photoreal-cooked/materials/roofing-tiles014-b/slot-0-arm.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/roofing-tiles014-b/slot-0-arm.dds",23u,Kind::Copy,1398276ull,"DB37169D0EE712C3B9BAD805878612F2380C3E7BA57A323910B6E21E12AE4DE6"},
    {"photoreal-cooked/materials/roofing-tiles014-b/slot-0-base.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/roofing-tiles014-b/slot-0-base.dds",23u,Kind::Copy,1398276ull,"B373CA39A3FA5054156B5A19E826044FD1E9D790765B341D6845A64235B01992"},
    {"photoreal-cooked/materials/roofing-tiles014-b/slot-0-normal.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/roofing-tiles014-b/slot-0-normal.dds",23u,Kind::Copy,1398276ull,"535682DA7E8F938BF7206D7A626E5139266125B18D5447FAAA0CC4C631EA7E6C"},
    {"photoreal-cooked/materials/stony-dirt-path/slot-0-arm.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/stony-dirt-path/slot-0-arm.dds",23u,Kind::Copy,1398276ull,"BFDEB9645BAA1DB8949BBD2E29C957BD070F25FEE4B7064343209CB6D9F93ABD"},
    {"photoreal-cooked/materials/stony-dirt-path/slot-0-base.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/stony-dirt-path/slot-0-base.dds",23u,Kind::Copy,1398276ull,"AD65700BEDB816E745C8F742B26C1F5E59428E350261B1C7B667399BA21A03A4"},
    {"photoreal-cooked/materials/stony-dirt-path/slot-0-normal.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/stony-dirt-path/slot-0-normal.dds",23u,Kind::Copy,1398276ull,"1C5A312DE5E4C6A7EDE14EF6D8F4EA51D7319BD70A3C725085296CDA316635E0"},
    {"photoreal-cooked/materials/tree-small-02/branches-arm.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/tree-small-02/branches-arm.dds",23u,Kind::Copy,1398276ull,"1C54A88EC7A17C55528A99319378283D61B4800002AB4E63CA0FE30EBB1249EA"},
    {"photoreal-cooked/materials/tree-small-02/branches-base.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/tree-small-02/branches-base.dds",23u,Kind::Copy,1398276ull,"CF4F6FCC9329E66F05B7B2E86D20A38B2D0E8E054013CF3E0431A4CBC1E17D0A"},
    {"photoreal-cooked/materials/tree-small-02/branches-normal.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/tree-small-02/branches-normal.dds",23u,Kind::Copy,1398276ull,"EA8A046E64CE7EACCE435E64F31FC45501624FD131C7E6C8492BFD9DE9FB1BEB"},
    {"photoreal-cooked/materials/tree-small-02/leaves-arm.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/tree-small-02/leaves-arm.dds",23u,Kind::Copy,1398276ull,"C8A6A8747023A64ED506AD62B8E5C0AE16BF14E0353D5E67F8D2511967AAF2ED"},
    {"photoreal-cooked/materials/tree-small-02/leaves-base.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/tree-small-02/leaves-base.dds",23u,Kind::Copy,1398276ull,"88FCE8DC2F1815C421A0C7C4D9CC7D9CACB4024F6C51E8FC928A5E938B7996A9"},
    {"photoreal-cooked/materials/tree-small-02/leaves-normal.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/tree-small-02/leaves-normal.dds",23u,Kind::Copy,1398276ull,"320D6DF57BF075DBD701ACAB52740A623F0C86D298BD3048F3986D1988BB0CD9"},
    {"photoreal-cooked/materials/tree-small-02/trunk-arm.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/tree-small-02/trunk-arm.dds",23u,Kind::Copy,1398276ull,"716C128DA96FEB1F3AD4EA041ECD9D5C167115A7AA0671F517EBAEFBC9963B7D"},
    {"photoreal-cooked/materials/tree-small-02/trunk-base.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/tree-small-02/trunk-base.dds",23u,Kind::Copy,1398276ull,"513D07ECE694D03E3C095C4148D7DA4649FFDF6DB54826CF1BF1754A368B1453"},
    {"photoreal-cooked/materials/tree-small-02/trunk-normal.dds","engine/assets/realistic-cc0-first-batch/cooked/materials/tree-small-02/trunk-normal.dds",23u,Kind::Copy,1398276ull,"099E981864B1E90B2C0500B0BCB0A92FCDC38EE196F00F7161905DC9BF762712"},
    {"photoreal-cooked/meshes/boulder-01/boulder-01-lod0-node0-mesh0-primitive0-material0-boulder-01.aemesh","engine/assets/realistic-cc0-first-batch/cooked/meshes/boulder-01/boulder-01-lod0-node0-mesh0-primitive0-material0-boulder-01.aemesh",23u,Kind::Copy,3071924ull,"1005CCA1811D45922B9DADDCB1C6C6E155E09F569DF8EFE5DE093468B11C25D6"},
    {"photoreal-cooked/meshes/boulder-01/boulder-01-lod1-node0-mesh0-primitive0-material0-boulder-01.aemesh","engine/assets/realistic-cc0-first-batch/cooked/meshes/boulder-01/boulder-01-lod1-node0-mesh0-primitive0-material0-boulder-01.aemesh",23u,Kind::Copy,2240604ull,"4C5172DCA2FA20AB7FC0A44DD9C38C5853E59F8BF3AB95EB711F1F8B2771728B"},
    {"photoreal-cooked/meshes/boulder-01/boulder-01-lod2-node0-mesh0-primitive0-material0-boulder-01.aemesh","engine/assets/realistic-cc0-first-batch/cooked/meshes/boulder-01/boulder-01-lod2-node0-mesh0-primitive0-material0-boulder-01.aemesh",23u,Kind::Copy,1913612ull,"57513292274275206F02CFDCA569D515AE3B12247477CECB505ED465E5915133"},
    {"photoreal-cooked/meshes/coastal-cliff-01/coastal-cliff-01-lod0-node0-mesh0-primitive0-material0-coastal-cliff-01.aemesh","engine/assets/realistic-cc0-first-batch/cooked/meshes/coastal-cliff-01/coastal-cliff-01-lod0-node0-mesh0-primitive0-material0-coastal-cliff-01.aemesh",23u,Kind::Copy,12225112ull,"33BB368FCF012811A8439C29947CEF8E2386CED19EC7C8D4863D6114C8A869CE"},
    {"photoreal-cooked/meshes/coastal-cliff-01/coastal-cliff-01-lod1-node0-mesh0-primitive0-material0-coastal-cliff-01.aemesh","engine/assets/realistic-cc0-first-batch/cooked/meshes/coastal-cliff-01/coastal-cliff-01-lod1-node0-mesh0-primitive0-material0-coastal-cliff-01.aemesh",23u,Kind::Copy,6199688ull,"BDC29F72AE81F54E1D771462CF64082138B480CE26E8C007E4692EC78F71ED7B"},
    {"photoreal-cooked/meshes/coastal-cliff-01/coastal-cliff-01-lod2-node0-mesh0-primitive0-material0-coastal-cliff-01.aemesh","engine/assets/realistic-cc0-first-batch/cooked/meshes/coastal-cliff-01/coastal-cliff-01-lod2-node0-mesh0-primitive0-material0-coastal-cliff-01.aemesh",23u,Kind::Copy,1934912ull,"01D08FF97699086915750BB8F7FEBFB414AFEA47691D3EDC3AC62D0C793CD451"},
    {"photoreal-cooked/meshes/dead-tree-trunk-02/dead-tree-trunk-02-lod0-node0-mesh0-primitive0-material0-dead-tree-trunk-02.aemesh","engine/assets/realistic-cc0-first-batch/cooked/meshes/dead-tree-trunk-02/dead-tree-trunk-02-lod0-node0-mesh0-primitive0-material0-dead-tree-trunk-02.aemesh",23u,Kind::Copy,2288744ull,"A0CD7C1D78653B41167FFA4C78A778C8F990FFE19F1B3DFD72D233F6452B4B5B"},
    {"photoreal-cooked/meshes/dead-tree-trunk-02/dead-tree-trunk-02-lod1-node0-mesh0-primitive0-material0-dead-tree-trunk-02.aemesh","engine/assets/realistic-cc0-first-batch/cooked/meshes/dead-tree-trunk-02/dead-tree-trunk-02-lod1-node0-mesh0-primitive0-material0-dead-tree-trunk-02.aemesh",23u,Kind::Copy,1195564ull,"B792F05654143B9729F3FF59726E1933F689BB1C445A5570CA3BBB68548680E4"},
    {"photoreal-cooked/meshes/dead-tree-trunk-02/dead-tree-trunk-02-lod2-node0-mesh0-primitive0-material0-dead-tree-trunk-02.aemesh","engine/assets/realistic-cc0-first-batch/cooked/meshes/dead-tree-trunk-02/dead-tree-trunk-02-lod2-node0-mesh0-primitive0-material0-dead-tree-trunk-02.aemesh",23u,Kind::Copy,393160ull,"1CFF326A0E81653EECE33AECFDB0FB40F795E08F278176EB5FD0AD7505395D88"},
    {"photoreal-cooked/meshes/fir-tree-01-c/fir-tree-01-c-lod0-node0-mesh0-primitive0-material0-fir-tree-01-bark.aemesh","engine/assets/realistic-cc0-first-batch/cooked/meshes/fir-tree-01-c/fir-tree-01-c-lod0-node0-mesh0-primitive0-material0-fir-tree-01-bark.aemesh",23u,Kind::Copy,274304ull,"F2623A7BBA3EE4D16A7325B196CC320DA9565AD0F5704D6C083BA2783BC704B0"},
    {"photoreal-cooked/meshes/fir-tree-01-c/fir-tree-01-c-lod0-node0-mesh0-primitive1-material1-fir-tree-01-twig.aemesh","engine/assets/realistic-cc0-first-batch/cooked/meshes/fir-tree-01-c/fir-tree-01-c-lod0-node0-mesh0-primitive1-material1-fir-tree-01-twig.aemesh",23u,Kind::Copy,450740ull,"C9A16431AC6687FAD5322C802B5A0EAD6064020A3DF8B097220DE1A18FF65FD1"},
    {"photoreal-cooked/meshes/fir-tree-01-c/fir-tree-01-c-lod0-node0-mesh0-primitive2-material2-fir-tree-01-dead-branches.aemesh","engine/assets/realistic-cc0-first-batch/cooked/meshes/fir-tree-01-c/fir-tree-01-c-lod0-node0-mesh0-primitive2-material2-fir-tree-01-dead-branches.aemesh",23u,Kind::Copy,103220ull,"C47F9FCB9719C2A7425B4E200BF25A64EA484CA8376409F1418429675DE34387"},
    {"photoreal-cooked/meshes/fir-tree-01-c/fir-tree-01-c-lod0-node0-mesh0-primitive3-material3-fir-tree-01-trunk-c.aemesh","engine/assets/realistic-cc0-first-batch/cooked/meshes/fir-tree-01-c/fir-tree-01-c-lod0-node0-mesh0-primitive3-material3-fir-tree-01-trunk-c.aemesh",23u,Kind::Copy,610948ull,"578838CD434E54DA61152241FBCBF2E43CDC3663783521D516886084667F3180"},
    {"photoreal-cooked/meshes/fir-tree-01-c/fir-tree-01-c-lod1-node0-mesh0-primitive0-material0-fir-tree-01-bark.aemesh","engine/assets/realistic-cc0-first-batch/cooked/meshes/fir-tree-01-c/fir-tree-01-c-lod1-node0-mesh0-primitive0-material0-fir-tree-01-bark.aemesh",23u,Kind::Copy,208204ull,"02084A8C2F9A4AEADB16D1A9438091D93920C4C9852658DA99D219E5A74F61A5"},
    {"photoreal-cooked/meshes/fir-tree-01-c/fir-tree-01-c-lod1-node0-mesh0-primitive1-material1-fir-tree-01-twig.aemesh","engine/assets/realistic-cc0-first-batch/cooked/meshes/fir-tree-01-c/fir-tree-01-c-lod1-node0-mesh0-primitive1-material1-fir-tree-01-twig.aemesh",23u,Kind::Copy,288464ull,"5C597F3D5722F61CB957689D3719B949E7550455D0A0012D254329B4FCADD8DA"},
    {"photoreal-cooked/meshes/fir-tree-01-c/fir-tree-01-c-lod1-node0-mesh0-primitive2-material2-fir-tree-01-dead-branches.aemesh","engine/assets/realistic-cc0-first-batch/cooked/meshes/fir-tree-01-c/fir-tree-01-c-lod1-node0-mesh0-primitive2-material2-fir-tree-01-dead-branches.aemesh",23u,Kind::Copy,35748ull,"DC3FE7F82E9BECC8BB23AA1AF9822B82DBD4687D868A3D8B776A198038E0F899"},
    {"photoreal-cooked/meshes/fir-tree-01-c/fir-tree-01-c-lod1-node0-mesh0-primitive3-material3-fir-tree-01-trunk-c.aemesh","engine/assets/realistic-cc0-first-batch/cooked/meshes/fir-tree-01-c/fir-tree-01-c-lod1-node0-mesh0-primitive3-material3-fir-tree-01-trunk-c.aemesh",23u,Kind::Copy,175520ull,"13DEC2D3F0332B83130EFE00AB0256B0EF83A955B86CA60D4F448E908BDA37A2"},
    {"photoreal-cooked/meshes/fir-tree-01-c/fir-tree-01-c-lod2-node0-mesh0-primitive0-material0-fir-tree-01-bark.aemesh","engine/assets/realistic-cc0-first-batch/cooked/meshes/fir-tree-01-c/fir-tree-01-c-lod2-node0-mesh0-primitive0-material0-fir-tree-01-bark.aemesh",23u,Kind::Copy,136728ull,"55A5CCC2CBCA2078C97BAB46F6F98F2A33A0114CDB683280049E74955F78A4AB"},
    {"photoreal-cooked/meshes/fir-tree-01-c/fir-tree-01-c-lod2-node0-mesh0-primitive1-material1-fir-tree-01-twig.aemesh","engine/assets/realistic-cc0-first-batch/cooked/meshes/fir-tree-01-c/fir-tree-01-c-lod2-node0-mesh0-primitive1-material1-fir-tree-01-twig.aemesh",23u,Kind::Copy,83488ull,"2EC74FE514C152F03867A8D9D2D2C975D8C35E91AA3E2551602F72D1BDF219D4"},
    {"photoreal-cooked/meshes/fir-tree-01-c/fir-tree-01-c-lod2-node0-mesh0-primitive2-material2-fir-tree-01-dead-branches.aemesh","engine/assets/realistic-cc0-first-batch/cooked/meshes/fir-tree-01-c/fir-tree-01-c-lod2-node0-mesh0-primitive2-material2-fir-tree-01-dead-branches.aemesh",23u,Kind::Copy,2992ull,"2B31900122A30B74CF95A6DB04F7ABAC6875BBD7C8C3EC642223E9CEF93869F5"},
    {"photoreal-cooked/meshes/fir-tree-01-c/fir-tree-01-c-lod2-node0-mesh0-primitive3-material3-fir-tree-01-trunk-c.aemesh","engine/assets/realistic-cc0-first-batch/cooked/meshes/fir-tree-01-c/fir-tree-01-c-lod2-node0-mesh0-primitive3-material3-fir-tree-01-trunk-c.aemesh",23u,Kind::Copy,7348ull,"E5B84998EA8631F2B8E0DB1B8C2C90A21A22EBC996DEFAD90B61D96F7FB80532"},
    {"photoreal-cooked/meshes/island-tree-01/island-tree-01-lod0-node0-mesh0-primitive0-material0-island-tree-01.aemesh","engine/assets/realistic-cc0-first-batch/cooked/meshes/island-tree-01/island-tree-01-lod0-node0-mesh0-primitive0-material0-island-tree-01.aemesh",23u,Kind::Copy,959120ull,"1154FE8D204BB22155447AE15689146F4BC7D0B46C99942269A0FCF491FE489F"},
    {"photoreal-cooked/meshes/island-tree-01/island-tree-01-lod0-node0-mesh0-primitive1-material1-island-tree-01-leaves.aemesh","engine/assets/realistic-cc0-first-batch/cooked/meshes/island-tree-01/island-tree-01-lod0-node0-mesh0-primitive1-material1-island-tree-01-leaves.aemesh",23u,Kind::Copy,38691184ull,"8177CDE93F9FD9923F2CF08A6A2784BD4AC95C77AAE3BF1B9BA4B9AE3EADE435"},
    {"photoreal-cooked/meshes/island-tree-01/island-tree-01-lod0-node0-mesh0-primitive2-material2-island-tree-01-branches.aemesh","engine/assets/realistic-cc0-first-batch/cooked/meshes/island-tree-01/island-tree-01-lod0-node0-mesh0-primitive2-material2-island-tree-01-branches.aemesh",23u,Kind::Copy,16052564ull,"518071FB54B7AC848CF203E64ACD1EF2108650483956F63CFAC6EAB5434A6822"},
    {"photoreal-cooked/meshes/island-tree-01/island-tree-01-lod1-node0-mesh0-primitive0-material0-island-tree-01.aemesh","engine/assets/realistic-cc0-first-batch/cooked/meshes/island-tree-01/island-tree-01-lod1-node0-mesh0-primitive0-material0-island-tree-01.aemesh",23u,Kind::Copy,493660ull,"D188C1FB3756E7614A14BFD909A31934B5598585FC8FCC29E5BD18D60B03E216"},
    {"photoreal-cooked/meshes/island-tree-01/island-tree-01-lod1-node0-mesh0-primitive1-material1-island-tree-01-leaves.aemesh","engine/assets/realistic-cc0-first-batch/cooked/meshes/island-tree-01/island-tree-01-lod1-node0-mesh0-primitive1-material1-island-tree-01-leaves.aemesh",23u,Kind::Copy,21375028ull,"1773F7705A43DA3B3673D0D34C66C2C4FEFC47952970CB84665A774D6A54F871"},
    {"photoreal-cooked/meshes/island-tree-01/island-tree-01-lod1-node0-mesh0-primitive2-material2-island-tree-01-branches.aemesh","engine/assets/realistic-cc0-first-batch/cooked/meshes/island-tree-01/island-tree-01-lod1-node0-mesh0-primitive2-material2-island-tree-01-branches.aemesh",23u,Kind::Copy,8328656ull,"C346382C0CABE2618DA9A699F412F0012D6DC6105489DBE91F273C49CDDCD316"},
    {"photoreal-cooked/meshes/island-tree-01/island-tree-01-lod2-node0-mesh0-primitive0-material0-island-tree-01.aemesh","engine/assets/realistic-cc0-first-batch/cooked/meshes/island-tree-01/island-tree-01-lod2-node0-mesh0-primitive0-material0-island-tree-01.aemesh",23u,Kind::Copy,159612ull,"B964D1F7B16FB3602FF038AE111C79D0BFC029185896E95DA97CC1ACB4F59145"},
    {"photoreal-cooked/meshes/island-tree-01/island-tree-01-lod2-node0-mesh0-primitive1-material1-island-tree-01-leaves.aemesh","engine/assets/realistic-cc0-first-batch/cooked/meshes/island-tree-01/island-tree-01-lod2-node0-mesh0-primitive1-material1-island-tree-01-leaves.aemesh",23u,Kind::Copy,78760ull,"77D898BD42F649FDFA062A8C6D820A24AB015405F50B43F5404848136F27CBB6"},
    {"photoreal-cooked/meshes/island-tree-01/island-tree-01-lod2-node0-mesh0-primitive2-material2-island-tree-01-branches.aemesh","engine/assets/realistic-cc0-first-batch/cooked/meshes/island-tree-01/island-tree-01-lod2-node0-mesh0-primitive2-material2-island-tree-01-branches.aemesh",23u,Kind::Copy,2709952ull,"5D32528C7604D041A0730755DDF765B111F82B43847C5F4AED0FE386E99D6188"},
    {"photoreal-cooked/meshes/tree-small-02/tree-small-02-lod0-node0-mesh0-primitive0-material0-tree-small-02-branches.aemesh","engine/assets/realistic-cc0-first-batch/cooked/meshes/tree-small-02/tree-small-02-lod0-node0-mesh0-primitive0-material0-tree-small-02-branches.aemesh",23u,Kind::Copy,1079312ull,"C620525628E5C8467EAEC0A1B7D230B6ECF36C2702A483258C85E9BCFD9F36B4"},
    {"photoreal-cooked/meshes/tree-small-02/tree-small-02-lod0-node0-mesh0-primitive1-material1-tree-small-02-leaves.aemesh","engine/assets/realistic-cc0-first-batch/cooked/meshes/tree-small-02/tree-small-02-lod0-node0-mesh0-primitive1-material1-tree-small-02-leaves.aemesh",23u,Kind::Copy,27725624ull,"50B27F41C183D16D4075985C37AC7711B2AC7F61875FE0F7844CAA68DFA3B65A"},
    {"photoreal-cooked/meshes/tree-small-02/tree-small-02-lod0-node0-mesh0-primitive2-material2-tree-small-02-trunk.aemesh","engine/assets/realistic-cc0-first-batch/cooked/meshes/tree-small-02/tree-small-02-lod0-node0-mesh0-primitive2-material2-tree-small-02-trunk.aemesh",23u,Kind::Copy,291600ull,"96C2758F7901B4FC07EC7D29BD56034B607A83EFAB1C62C1F873171BA52A1383"},
    {"photoreal-cooked/meshes/tree-small-02/tree-small-02-lod1-node0-mesh0-primitive0-material0-tree-small-02-branches.aemesh","engine/assets/realistic-cc0-first-batch/cooked/meshes/tree-small-02/tree-small-02-lod1-node0-mesh0-primitive0-material0-tree-small-02-branches.aemesh",23u,Kind::Copy,400332ull,"67575E73B02C9EA28F556477A519935CEDBB48CB850D444F2DABF179AF2D1FE9"},
    {"photoreal-cooked/meshes/tree-small-02/tree-small-02-lod1-node0-mesh0-primitive1-material1-tree-small-02-leaves.aemesh","engine/assets/realistic-cc0-first-batch/cooked/meshes/tree-small-02/tree-small-02-lod1-node0-mesh0-primitive1-material1-tree-small-02-leaves.aemesh",23u,Kind::Copy,10241152ull,"5701F5AE51A4E2B324D820FBA16CE32C7BDBCC8BC5B5AF584238BFA950FDD7C7"},
    {"photoreal-cooked/meshes/tree-small-02/tree-small-02-lod1-node0-mesh0-primitive2-material2-tree-small-02-trunk.aemesh","engine/assets/realistic-cc0-first-batch/cooked/meshes/tree-small-02/tree-small-02-lod1-node0-mesh0-primitive2-material2-tree-small-02-trunk.aemesh",23u,Kind::Copy,107540ull,"2CC4B6F336B67D835E49A6A5DB35ECEF523FAFD61569FB2C8C18723B72F53102"},
    {"photoreal-cooked/meshes/tree-small-02/tree-small-02-lod2-node0-mesh0-primitive0-material0-tree-small-02-branches.aemesh","engine/assets/realistic-cc0-first-batch/cooked/meshes/tree-small-02/tree-small-02-lod2-node0-mesh0-primitive0-material0-tree-small-02-branches.aemesh",23u,Kind::Copy,163400ull,"4182C1D2D3E726F5C77DD84ADF59EA5D4E022D49B1F211B34B488DBC165B68CC"},
    {"photoreal-cooked/meshes/tree-small-02/tree-small-02-lod2-node0-mesh0-primitive1-material1-tree-small-02-leaves.aemesh","engine/assets/realistic-cc0-first-batch/cooked/meshes/tree-small-02/tree-small-02-lod2-node0-mesh0-primitive1-material1-tree-small-02-leaves.aemesh",23u,Kind::Copy,3704316ull,"C46ECA29C84D6EA9DB320444D01ED4F16403A2A7F50729178E70C7886B28BC5B"},
    {"photoreal-cooked/meshes/tree-small-02/tree-small-02-lod2-node0-mesh0-primitive2-material2-tree-small-02-trunk.aemesh","engine/assets/realistic-cc0-first-batch/cooked/meshes/tree-small-02/tree-small-02-lod2-node0-mesh0-primitive2-material2-tree-small-02-trunk.aemesh",23u,Kind::Copy,57760ull,"1C765173EBC17A3BE167C35EF1B8DF8329FB4CF98F468F4DA13FD42533B677FD"},
    {"photoreal-cooked/runtime-render-manifest.json","engine/assets/realistic-cc0-first-batch/cooked/runtime-render-manifest.json",23u,Kind::Copy,129864ull,"3A28A543F02673E515164A002D000E6CAF8BAFD9849568E3EE9C024342F40D4D"},
};
inline constexpr ProducerInput ProducerInputs[]{
    {"games/mistwatch/tools/author_root_stem.py",9767ull,"BB9C019629A9CD1A66614DD1696EA1AA927741FFB31A4AD46FDCDFA7360AF176"},
    {"engine/assets/realistic-cc0-first-batch/cooked/meshes/island-tree-01/island-tree-01-lod2-node0-mesh0-primitive0-material0-island-tree-01.aemesh",159612ull,"B964D1F7B16FB3602FF038AE111C79D0BFC029185896E95DA97CC1ACB4F59145"},
    {"games/mistwatch/native/tools/export_boar.cpp",638ull,"9397681D09D61448126D55326FC52179A7C18CD38D0DE5628991FA35CFA1DA1A"},
    {"games/mistwatch/native/include/MistwatchBoar.h",10869ull,"CF2C617A135447EB5A342532283AF7A35D3C402B805633BEEAEB38003BECCBD4"},
    {"engine/native/src/SkinnedModelAsset.cpp",39060ull,"9656F5CAE752C586F40D1F764D4409F80F2E17E1300A0CD69356422230102B51"},
    {"engine/native/include/Aevrynd/SkinnedModelAsset.h",2818ull,"9B07F44670F72CA4BA554E82A2C2066599CE4F70451F858680FB1B3C6B9824BA"},
    {"engine/native/include/Aevrynd/SkinnedModel3D.h",2882ull,"BA44F124955E8D7AF68B42C43A43497E7BD6C97E3E2DDBCB506ED5599C85C2C3"},
    {"engine/native/include/Aevrynd/Math.h",6625ull,"00E44D164ADBBA33D0E41D3019E3391905BE26775D096E74AF2B74BCD81AFDF5"},
};
inline void Require(bool condition,std::string_view error) {
    if(!condition) throw std::runtime_error(std::string(error));
}
inline std::string_view ProfileName(Profile profile) {
    switch(profile) {
    case Profile::LegacyFullV1:return "legacy-full-v1";
    case Profile::HabitatFullV1:return "habitat-full-v1";
    case Profile::AuthoredNettleFullV1:return "authored-nettle-full-v1";
    case Profile::AuthoredMistwatchOnlyV1:return "authored-mistwatch-only-v1";
    case Profile::AuthoredBoulderFullV2:return "authored-boulder-full-v2";
    case Profile::AuthoredBoulderMistwatchOnlyV2:return "authored-boulder-mistwatch-only-v2";
    case Profile::AuthoredLeafClusterFullV3:return "authored-leaf-cluster-full-v3";
    case Profile::AuthoredForestSurfaceFullV4:return "authored-forest-surface-full-v4";
    case Profile::AuthoredCurrentForestFullV5:return "authored-current-forest-full-v5";
    }
    throw std::runtime_error("runtime_profile_invalid");
}
inline Profile ParseProfile(std::string_view name) {
    for(const auto profile:{Profile::LegacyFullV1,Profile::HabitatFullV1,Profile::AuthoredNettleFullV1,Profile::AuthoredMistwatchOnlyV1,Profile::AuthoredBoulderFullV2,Profile::AuthoredBoulderMistwatchOnlyV2,Profile::AuthoredLeafClusterFullV3,Profile::AuthoredForestSurfaceFullV4,Profile::AuthoredCurrentForestFullV5})
        if(ProfileName(profile)==name)return profile;
    throw std::runtime_error("runtime_profile_unrecognized");
}
inline unsigned ProfileMask(Profile profile) {
    (void)ProfileName(profile);
    return 1u << static_cast<unsigned>(profile);
}
inline std::size_t ExpectedCount(Profile profile) {
    constexpr std::array<std::size_t,9> counts{387,402,412,154,414,156,416,418,418};
    (void)ProfileName(profile);
    return counts[static_cast<std::size_t>(profile)];
}
inline std::string_view KindName(Kind kind) {
    switch(kind) {
    case Kind::Copy:return "copy"; case Kind::RootMesh:return "root-mesh";
    case Kind::RootReceipt:return "root-receipt"; case Kind::Boar:return "boar";
    }
    throw std::runtime_error("runtime_kind_invalid");
}
inline std::string Escape(std::string_view input) {
    std::string result;
    for(const char value:input) {
        Require(static_cast<unsigned char>(value)>=32,"runtime_json_control_character");
        if(value=='"' || value=='\\')result+='\\';
        result+=value;
    }
    return result;
}
// JSON 不接受外部指定任意 hash：它必須與編譯入目前來源的表逐位元組相同。
inline std::string ManifestText() {
    std::ostringstream out;out.imbue(std::locale::classic());
    out<<"{\n\"schema_version\":1,\n\"scope\":\"runtime-identity-only; authored_world_qualified=false\",\n"
        "\"profiles\":[[\"legacy-full-v1\",387],[\"habitat-full-v1\",402],[\"authored-nettle-full-v1\",412],[\"authored-mistwatch-only-v1\",154],[\"authored-boulder-full-v2\",414],[\"authored-boulder-mistwatch-only-v2\",156]],\n\"entries\":[\n";
    bool first=true;
    for(const auto& e:Entries) {
        if(!first)out<<",\n";first=false;
        out<<"[\""<<e.path<<"\",\""<<e.source<<"\","<<e.mask<<",\""<<KindName(e.kind)<<"\","<<e.bytes<<",\""<<e.sha256<<"\"]";
    }
    out<<"\n],\n\"producer_inputs\":[\n";first=true;
    for(const auto& p:ProducerInputs) {
        if(!first)out<<",\n";first=false;
        out<<"[\""<<p.path<<"\","<<p.bytes<<",\""<<p.sha256<<"\"]";
    }
    out<<"\n]\n}\n";return out.str();
}
// 新部署只疊加獨立審查的兩個來源；舊表、遮罩與舊 manifest 完全保留。
inline constexpr Entry LeafClusterEntries[]{
    {"mistwatch-leaf-clusters/leaves-v1.aemesh","games/mistwatch/assets/leaf-clusters/leaves-v1.aemesh",64u,Kind::Copy,6377336ull,"23E72E7CE41CCA9567F2843EE8602B2D910B984F4D93D644199FD44E539E7767"},
    {"mistwatch-leaf-clusters/PROVENANCE.txt","games/mistwatch/assets/leaf-clusters/PROVENANCE.txt",64u,Kind::Copy,1790ull,"839D5C2A041D6D4E63A081DC65D3D47080B743FF734D8E01A4500E4C35B4D884"},
};
inline std::string LeafClusterManifestText() {
    std::ostringstream out;out.imbue(std::locale::classic());
    out<<"{\n\"schema_version\":1,\n\"scope\":\"runtime-identity-only; authored_world_qualified=false\",\n"
        "\"profile\":\"authored-leaf-cluster-full-v3\",\n\"parent\":\"authored-boulder-full-v2\",\n"
        "\"parent_mapping_sha256\":\"D1CB0D00DAF1853E1A3F8393B6D15610223931C6EC313555E110FD495E6093F3\",\n"
        "\"count\":416,\n\"additions\":[\n";
    bool first=true;
    for(const auto& e:LeafClusterEntries) {
        if(!first)out<<",\n";first=false;
        out<<"[\""<<e.path<<"\",\""<<e.source<<"\","<<e.mask<<",\""<<KindName(e.kind)<<"\","<<e.bytes<<",\""<<e.sha256<<"\"]";
    }
    out<<"\n]\n}\n";return out.str();
}
// 418 僅表示完整部署映射；不表示目前世界幾何或原生矩陣已通過。
inline constexpr Entry ForestSurfaceEntries[]{
    {"mistwatch-materials/moss-height/Ground037_SurfaceOrmHeight_Level0.png","engine/assets/mistwatch-surface-height/moss-height/Ground037_SurfaceOrmHeight_Level0.png",128u,Kind::Copy,1531546ull,"012D2378AA6E62CCA05AD66D69B512966D5894694B7B24D39AFD0CA7CA655F38"},
    {"mistwatch-materials/leaf-height/ForestLeaves04_SurfaceOrmHeight_Level0.png","engine/assets/mistwatch-surface-height/leaf-height/ForestLeaves04_SurfaceOrmHeight_Level0.png",128u,Kind::Copy,9102955ull,"9F3259F4772F294EDCA1DE03FA4E8D7ED65C0924238B1A6D0B27B8FED7C0EF85"},
};
inline std::string ForestSurfaceManifestText() {
    std::ostringstream out;out.imbue(std::locale::classic());
    out<<"{\n\"schema_version\":1,\n\"scope\":\"runtime-and-surface-only; current_geometry_qualified=false\",\n"
        "\"profile\":\"authored-forest-surface-full-v4\",\n\"parent\":\"authored-leaf-cluster-full-v3\",\n"
        "\"parent_manifest_sha256\":\"DE5E956E5191F620301D8088471C5FCF4BCADF3071BA51AA808EA6832C49AC4A\",\n"
        "\"count\":418,\n\"additions\":[\n";
    bool first=true;for(const auto& e:ForestSurfaceEntries){if(!first)out<<",\n";first=false;
        out<<"[\""<<e.path<<"\",\""<<e.source<<"\","<<e.mask<<",\""<<KindName(e.kind)<<"\","<<e.bytes<<",\""<<e.sha256<<"\"]";}
    out<<"\n]\n}\n";return out.str();
}
// 同一418部署可以服務不同明示世界；身分別名本身不宣稱目前世界或原生矩陣合格。
inline std::string CurrentForestManifestText(){
    return "{\n\"schema_version\":1,\n\"scope\":\"runtime-identity-only; current_world_qualified=false\",\n"
        "\"profile\":\"authored-current-forest-full-v5\",\n\"parent\":\"authored-forest-surface-full-v4\",\n"
        "\"parent_manifest_sha256\":\"467BE56D0F3C5F1408E014DB50728DD1BCC9D28AB0366CE6B74957C74505F291\",\n"
        "\"count\":418,\n\"additions\":[]\n}\n";
}
inline bool SafeRelative(std::string_view name) {
    if(name.empty() || name.front()=='/' || name.back()=='/' || name.find('\\')!=name.npos || name.find(':')!=name.npos)return false;
    std::size_t start=0;
    while(start<name.size()) {
        const auto end=name.find('/',start);const auto part=name.substr(start,end==name.npos?name.size()-start:end-start);
        if(part.empty() || part=="." || part==".." || part.back()=='.' || part.back()==' ')return false;
        for(const char c:part)if(static_cast<unsigned char>(c)<32 || static_cast<unsigned char>(c)>126)return false;
        if(end==name.npos)break;start=end+1;
    }
    return true;
}
inline std::string Lower(std::string value) {
    for(char& c:value)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return value;
}
inline void ValidatePathAttributes(DWORD attributes) {
    Require(attributes!=INVALID_FILE_ATTRIBUTES && !(attributes&FILE_ATTRIBUTE_REPARSE_POINT),"runtime_missing_or_reparse_path");
}
inline void NoReparse(const std::filesystem::path& path) {
    Require(path.is_absolute(),"runtime_path_not_absolute");
    auto cursor=path;
    while(!cursor.empty()) {
        const auto attributes=GetFileAttributesW(cursor.c_str());
        ValidatePathAttributes(attributes);
        const auto parent=cursor.parent_path();if(parent==cursor)break;cursor=parent;
    }
}
inline std::vector<std::byte> Read(const std::filesystem::path& path) {
    NoReparse(path);
    Require(std::filesystem::is_regular_file(path),"runtime_not_regular_file");
    const auto size=std::filesystem::file_size(path);
    Require(size>0 && size<=128ull*1024ull*1024ull,"runtime_file_size_outside_limit");
    std::ifstream file(path,std::ios::binary);
    std::vector<std::byte> data(static_cast<std::size_t>(size));
    file.read(reinterpret_cast<char*>(data.data()),static_cast<std::streamsize>(size));
    Require(file.good() && file.peek()==std::char_traits<char>::eof(),"runtime_file_read_or_size_changed");
    return data;
}
inline std::string Sha256(std::span<const std::byte> data) {
    BCRYPT_ALG_HANDLE algorithm{};BCRYPT_HASH_HANDLE hash{};
    Require(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0,"runtime_sha_provider");
    // 雜湊 handle 必須先釋放，才能釋放 BCrypt 的工作緩衝區。
    std::vector<unsigned char> object;
    struct Cleanup {
        BCRYPT_ALG_HANDLE& a;BCRYPT_HASH_HANDLE& h;
        ~Cleanup(){if(h)BCryptDestroyHash(h);if(a)BCryptCloseAlgorithmProvider(a,0);}
    } cleanup{algorithm,hash};
    DWORD size{},copied{};
    Require(BCryptGetProperty(algorithm,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&size),sizeof(size),&copied,0)>=0,"runtime_sha_object");
    object.resize(size);
    Require(BCryptCreateHash(algorithm,&hash,object.data(),size,nullptr,0,0)>=0,"runtime_sha_create");
    Require(data.size()<=(std::numeric_limits<ULONG>::max)(),"runtime_sha_length");
    Require(BCryptHashData(hash,reinterpret_cast<PUCHAR>(const_cast<std::byte*>(data.data())),static_cast<ULONG>(data.size()),0)>=0,"runtime_sha_update");
    std::array<unsigned char,32> result{};
    Require(BCryptFinishHash(hash,result.data(),static_cast<ULONG>(result.size()),0)>=0,"runtime_sha_finish");
    constexpr char digits[]="0123456789ABCDEF";std::string value;
    for(const auto b:result){value+=digits[b>>4];value+=digits[b&15];}
    return value;
}
inline FileIdentity Identity(const std::filesystem::path& path,std::string name) {
    const auto bytes=Read(path);
    return {std::move(name),Sha256(bytes),static_cast<std::uint64_t>(bytes.size())};
}
inline std::string Text(std::span<const std::byte> bytes) {
    return {reinterpret_cast<const char*>(bytes.data()),bytes.size()};
}
inline void Match(const FileIdentity& actual,std::uint64_t bytes,std::string_view sha) {
    Require(actual.bytes==bytes && actual.sha256==sha,"runtime_raw_identity_drift: "+actual.path);
}
inline std::vector<const Entry*> Expected(Profile profile) {
    std::vector<const Entry*> result;
    const bool current=profile==Profile::AuthoredCurrentForestFullV5;
    const bool leaf=profile==Profile::AuthoredLeafClusterFullV3||profile==Profile::AuthoredForestSurfaceFullV4||current;
    const auto inherited=leaf?Profile::AuthoredBoulderFullV2:profile;
    for(const auto& entry:Entries)if(entry.mask&ProfileMask(inherited))result.push_back(&entry);
    if(leaf)
        for(const auto& entry:LeafClusterEntries)result.push_back(&entry);
    if(profile==Profile::AuthoredForestSurfaceFullV4||current)
        for(const auto& entry:ForestSurfaceEntries)result.push_back(&entry);
    Require(result.size()==ExpectedCount(profile),"runtime_compiled_membership_invalid");
    return result;
}
// 純資料負向測試入口；receipt 的身份使用完整正規化 metadata，而非任意通配 hash。
inline void ValidateIdentities(Profile profile,const std::vector<FileIdentity>& actual) {
    const auto expected=Expected(profile);
    Require(actual.size()==expected.size(),"runtime_membership_count_drift");
    std::map<std::string,const FileIdentity*> files;std::map<std::string,bool> insensitive;
    for(const auto& row:actual) {
        Require(SafeRelative(row.path),"runtime_unsafe_relative_path");
        Require(files.emplace(row.path,&row).second && insensitive.emplace(Lower(row.path),true).second,"runtime_duplicate_or_case_shadow");
    }
    std::uint64_t total{};
    for(const auto* row:expected) {
        const auto found=files.find(std::string(row->path));
        Require(found!=files.end(),"runtime_exact_path_missing");
        Match(*found->second,row->bytes,row->sha256);total+=row->bytes;
    }
    Require(total<=2ull*1024ull*1024ull*1024ull,"runtime_total_size_limit");
}
// 僅兩個已綁定的絕對路徑可重新定位；所有幾何映射、來源hash與算法metadata仍封印。
inline std::string NormalizeRootReceipt(std::string text,const std::filesystem::path& source,const std::filesystem::path& output) {
    for(const auto& item:std::array<std::pair<std::string,std::string>,2>{{{"source",source.string()},{"output",output.string()}}}) {
        const std::string key="\""+item.first+"\": \"";
        const auto begin=text.find(key);Require(begin!=text.npos && text.find(key,begin+key.size())==text.npos,"root_receipt_path_field_missing_or_duplicate");
        const auto valueBegin=begin+key.size();
        const auto escaped=Escape(std::filesystem::absolute(std::filesystem::path(item.second)).lexically_normal().make_preferred().string());
        Require(text.compare(valueBegin,escaped.size(),escaped)==0 && valueBegin+escaped.size()<text.size() && text[valueBegin+escaped.size()]=='"',"root_receipt_path_not_current_derivation");
        text.replace(valueBegin,escaped.size(),item.first=="source"?"@SOURCE@":"@OUTPUT@");
    }
    return text;
}
inline FileIdentity ValidateRootMetadata(const std::string& receipt,const std::filesystem::path& source,const std::filesystem::path& output) {
    const auto text=NormalizeRootReceipt(receipt,source,output);
    const auto bytes=std::as_bytes(std::span(text.data(),text.size()));
    FileIdentity identity{"root-stem-canonical",Sha256(bytes),static_cast<std::uint64_t>(bytes.size())};
    Match(identity,250893ull,"104607F9FA1E815A37583CCB8812A3982138B4F0F86230CDA91C494D50C649D6");
    return identity;
}
inline std::vector<FileIdentity> Validate(const std::filesystem::path& sourceRoot,const std::filesystem::path& runtimeRoot,Profile profile,const GeneratedEvidence& evidence={}) {
    NoReparse(sourceRoot);NoReparse(runtimeRoot);
    const auto expected=Expected(profile);
    const auto manifest=Read(sourceRoot/"games/mistwatch/native/tests/qualification/runtime-profiles-v1.json");
    Require(Text(manifest)==ManifestText(),"runtime_manifest_not_compiled_contract");
    const bool current=profile==Profile::AuthoredCurrentForestFullV5;
    if(profile==Profile::AuthoredLeafClusterFullV3||profile==Profile::AuthoredForestSurfaceFullV4||current) {
        const auto additions=Read(sourceRoot/"games/mistwatch/native/tests/qualification/runtime-leaf-cluster-full-v3.json");
        Require(Text(additions)==LeafClusterManifestText(),"runtime_leaf_manifest_not_compiled_contract");
        if(profile==Profile::AuthoredForestSurfaceFullV4||current){
            Match(Identity(sourceRoot/"games/mistwatch/native/tests/qualification/runtime-leaf-cluster-full-v3.json","leaf-parent-manifest"),
                656ull,"DE5E956E5191F620301D8088471C5FCF4BCADF3071BA51AA808EA6832C49AC4A");
            const auto surface=Read(sourceRoot/"games/mistwatch/native/tests/qualification/runtime-forest-surface-full-v4.json");
            Require(Text(surface)==ForestSurfaceManifestText(),"runtime_surface_manifest_not_compiled_contract");
            if(current){
                const auto version=Read(sourceRoot/"games/mistwatch/native/tests/qualification/runtime-current-forest-full-v5.json");
                Require(Text(version)==CurrentForestManifestText(),"runtime_current_manifest_not_compiled_contract");
            }
        }
    }
    for(const auto& pin:ProducerInputs)Match(Identity(sourceRoot/std::string(pin.path),std::string(pin.path)),pin.bytes,pin.sha256);
    std::map<std::string,const Entry*> wanted;for(const auto* e:expected)wanted.emplace(std::string(e->path),e);
    std::vector<FileIdentity> actual,canonical;
    std::filesystem::path rootDirectory=evidence.rootStemOutputDirectory;
    if(rootDirectory.empty())rootDirectory=runtimeRoot.parent_path().parent_path().parent_path()/"games/mistwatch/native/generated-root-stem";
    NoReparse(rootDirectory);
    const auto rootSource=sourceRoot/std::string(ProducerInputs[1].path);
    const auto rootOutput=rootDirectory/"island-tree-01-lod2-trunk-root-stem.aemesh";
    const auto generatedMesh=Identity(rootOutput,"generated-root-stem");
    Match(generatedMesh,165000ull,"96EA5581E6A5D2A0C3260DEEBE614A4FA9BCCA0A77772E5EAE42E0C88730BC7C");
    const auto generatedReceipt=Read(rootDirectory/"author-receipt.json");
    const auto canonicalMetadata=ValidateRootMetadata(Text(generatedReceipt),rootSource,rootOutput);
    const Mistwatch::Boar boar;
    std::vector<std::byte> boarBytes;std::string error;
    Require(Aevrynd::EncodeSkinnedModelAsset(boar.model,boarBytes,&error),"boar_current_source_encode_failed");
    Match({"boar-current-source",Sha256(boarBytes),static_cast<std::uint64_t>(boarBytes.size())},329520ull,"DFABA5E98F00D4CB86363A14E93B20A3DAF88B49C93A2972254C7FFC605DC8AC");
    for(const auto& item:std::filesystem::recursive_directory_iterator(runtimeRoot)) {
        NoReparse(item.path());
        if(item.is_directory())continue;
        Require(item.is_regular_file(),"runtime_special_file");
        const auto relative=std::filesystem::relative(item.path(),runtimeRoot).generic_string();
        Require(SafeRelative(relative),"runtime_unsafe_relative_path");
        const auto found=wanted.find(relative);Require(found!=wanted.end(),"runtime_unexpected_path: "+relative);
        const auto& e=*found->second;
        auto identity=Identity(item.path(),relative);actual.push_back(identity);
        if(e.kind==Kind::Copy) {
            Match(Identity(sourceRoot/std::string(e.source),std::string(e.source)),e.bytes,e.sha256);
        } else if(e.kind==Kind::RootReceipt) {
            const auto raw=Read(item.path());Require(raw==generatedReceipt,"root_receipt_not_current_generated_copy");
            identity.bytes=canonicalMetadata.bytes;identity.sha256=canonicalMetadata.sha256;
        } else if(e.kind==Kind::Boar) {
            Require(Read(item.path())==boarBytes,"boar_runtime_not_current_source_output");
        }
        canonical.push_back(identity);
        Require(actual.size()<=512,"runtime_entry_limit");
    }
    ValidateIdentities(profile,canonical);
    std::sort(actual.begin(),actual.end(),[](const auto& a,const auto& b){return a.path<b.path;});
    return actual;
}
}
