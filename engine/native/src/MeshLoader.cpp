#include "Aevrynd/MeshLoader.h"
#include <array>
#include <cctype>
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace Aevrynd {
namespace {
struct SourceVertex {Vec3 position{};Color color{};};
struct FaceReference {int position{-1},normal{-1};};
struct VertexKey {int position{},normal{},smoothing{};bool operator==(const VertexKey&)const=default;};
struct VertexKeyHash {std::size_t operator()(const VertexKey& key)const{std::size_t value=std::size_t(uint32_t(key.position))*73856093u;value^=std::size_t(uint32_t(key.normal))*19349663u;value^=std::size_t(uint32_t(key.smoothing))*83492791u;return value;}};

int ResolveIndex(std::string_view token,std::size_t count){if(token.empty())return-1;try{std::size_t consumed{};const int value=std::stoi(std::string(token),&consumed);if(consumed!=token.size()||value==0)return-1;const int resolved=value>0?value-1:int(count)+value;return resolved>=0&&resolved<int(count)?resolved:-1;}catch(...){return-1;}}

bool ParseFaceReference(std::string_view token,std::size_t positionCount,std::size_t normalCount,FaceReference& result){const std::size_t first=token.find('/'),second=first==std::string_view::npos?std::string_view::npos:token.find('/',first+1);const std::string_view positionToken=token.substr(0,first);result.position=ResolveIndex(positionToken,positionCount);if(result.position<0)return false;if(second!=std::string_view::npos){const std::string_view normalToken=token.substr(second+1);if(!normalToken.empty()){result.normal=ResolveIndex(normalToken,normalCount);if(result.normal<0)return false;}}return true;}

MeshLoadResult3D Failure(std::size_t line,std::string message){return{{},"OBJ line "+std::to_string(line)+": "+std::move(message)};}
}

MeshLoadResult3D LoadObjMesh(std::string_view source,ObjLoadOptions3D options){
    std::vector<SourceVertex> positions;std::vector<Vec3> normals;Mesh3D mesh;std::vector<Vec3> generatedNormals;std::vector<bool> needsGeneratedNormal;std::unordered_map<VertexKey,uint32_t,VertexKeyHash> vertices;std::istringstream stream{std::string(source)};std::string line;std::size_t lineNumber{};int smoothingGroup{},faceNumber{};
    auto vertexIndex=[&](const FaceReference& reference,int smoothingKey){const VertexKey key{reference.position,reference.normal,reference.normal>=0?0:smoothingKey};auto found=vertices.find(key);if(found!=vertices.end())return found->second;const SourceVertex& sourceVertex=positions[reference.position];const uint32_t index=uint32_t(mesh.vertices.size());const uint32_t packedNormal=reference.normal>=0?PackNormalSnorm8(normals[reference.normal]):0;mesh.vertices.push_back({sourceVertex.position.x,sourceVertex.position.y,sourceVertex.position.z,packedNormal,PackColorRgba8(sourceVertex.color)});generatedNormals.push_back({});needsGeneratedNormal.push_back(reference.normal<0);vertices.emplace(key,index);return index;};
    while(std::getline(stream,line)){
        ++lineNumber;const std::size_t comment=line.find('#');if(comment!=std::string::npos)line.erase(comment);std::istringstream row(line);std::string keyword;if(!(row>>keyword))continue;
        if(keyword=="v"){
            std::vector<float> values;float value{};while(row>>value)values.push_back(value);if(values.size()<3)return Failure(lineNumber,"vertex requires three coordinates");Vec3 position{values[0],values[1],options.convertRightHandedToLeftHanded?-values[2]:values[2]};Color color=options.defaultColor;if(values.size()>=6){const float scale=(values[3]>1.f||values[4]>1.f||values[5]>1.f)?1.f/255.f:1.f;const float alpha=values.size()>=7?values[6]*(values[6]>1.f?1.f/255.f:1.f):1.f;color={values[3]*scale,values[4]*scale,values[5]*scale,alpha};}positions.push_back({position,color});
        }else if(keyword=="vn"){
            Vec3 normal{};if(!(row>>normal.x>>normal.y>>normal.z))return Failure(lineNumber,"normal requires three coordinates");if(options.convertRightHandedToLeftHanded)normal.z=-normal.z;normal=Normalize(normal);if(Length(normal)<.0001f)return Failure(lineNumber,"normal cannot be zero");normals.push_back(normal);
        }else if(keyword=="f"){
            if(positions.empty())return Failure(lineNumber,"face has no source vertices");std::vector<FaceReference> face;std::string token;while(row>>token){FaceReference reference;if(!ParseFaceReference(token,positions.size(),normals.size(),reference))return Failure(lineNumber,"face contains an invalid index");face.push_back(reference);}if(face.size()<3)return Failure(lineNumber,"face requires at least three vertices");++faceNumber;const int smoothingKey=smoothingGroup?smoothingGroup:-faceNumber;
            for(std::size_t corner=1;corner+1<face.size();++corner){std::array<FaceReference,3> triangle={face[0],face[corner],face[corner+1]};if(options.convertRightHandedToLeftHanded)std::swap(triangle[1],triangle[2]);std::array<uint32_t,3> indices={vertexIndex(triangle[0],smoothingKey),vertexIndex(triangle[1],smoothingKey),vertexIndex(triangle[2],smoothingKey)};const auto point=[&](uint32_t index){const Vertex3D& vertex=mesh.vertices[index];return Vec3{vertex.x,vertex.y,vertex.z};};const Vec3 faceNormal=Normalize(Cross(point(indices[1])-point(indices[0]),point(indices[2])-point(indices[0])));if(Length(faceNormal)<.0001f)return Failure(lineNumber,"face is degenerate");for(uint32_t index:indices)if(needsGeneratedNormal[index])generatedNormals[index]=generatedNormals[index]+faceNormal;mesh.indices.insert(mesh.indices.end(),indices.begin(),indices.end());}
        }else if(keyword=="s"){
            std::string group;if(!(row>>group)||group=="off"||group=="0")smoothingGroup=0;else{try{smoothingGroup=(std::max)(1,std::stoi(group));}catch(...){smoothingGroup=1;}}
        }
    }
    if(mesh.indices.empty())return{{},"OBJ contains no faces"};for(std::size_t index=0;index<mesh.vertices.size();++index)if(needsGeneratedNormal[index])mesh.vertices[index].normal=PackNormalSnorm8(Normalize(generatedNormals[index]));return mesh.Valid()?MeshLoadResult3D{std::move(mesh),{}}:MeshLoadResult3D{{},"OBJ produced an invalid mesh"};
}

MeshLoadResult3D LoadObjMeshFile(const std::filesystem::path& path,ObjLoadOptions3D options){std::ifstream file(path,std::ios::binary);if(!file)return{{},"Unable to open OBJ file: "+path.string()};std::ostringstream source;source<<file.rdbuf();return LoadObjMesh(source.str(),options);}
}
