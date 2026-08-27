#pragma once
#include "Mesh3D.h"
#include <filesystem>
#include <string>
#include <string_view>

namespace Aevrynd {
struct ObjLoadOptions3D {
    Color defaultColor{1,1,1,1};
    bool convertRightHandedToLeftHanded{true};
};

struct MeshLoadResult3D {
    Mesh3D mesh;
    std::string error;
    explicit operator bool()const{return error.empty()&&mesh.Valid();}
};

MeshLoadResult3D LoadObjMesh(std::string_view source,ObjLoadOptions3D options={});
MeshLoadResult3D LoadObjMeshFile(const std::filesystem::path& path,ObjLoadOptions3D options={});
MeshLoadResult3D LoadAevryndMeshFile(const std::filesystem::path& path);
}
