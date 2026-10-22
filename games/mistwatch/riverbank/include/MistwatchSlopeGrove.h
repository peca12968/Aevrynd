#pragma once
#include "MistwatchCanopyPopulation.h"

namespace Mistwatch::SlopeGrove {
inline constexpr std::uint64_t Namespace=0x4d57534c4f504531ull;
using WoodTriangle=std::array<Aevrynd::Vec3,3>;
struct Tree {
    Aevrynd::Transform3D pose;Aevrynd::Mesh3D root;Aevrynd::BoxCollider3D solid;
    std::vector<WoodTriangle> worldWood;
    std::array<std::uint64_t,64> leafTriangles{};
};
// 新林帶保留完整作者樹冠；只作者低根貼坡，不縮小葉片或放寬舊世界的植栽門。
class Resources {
public:
    std::vector<Tree> trees;
    std::vector<Aevrynd::BoxCollider3D> solids;
    std::vector<Aevrynd::BoxCollider3D> rootMasks;
    std::vector<Aevrynd::MeshResource3D> roots;
    std::array<Aevrynd::MeshResource3D,3> shared;
    std::array<Aevrynd::Material3D,3> materials;
    std::array<std::uint64_t,24> observed{};
    unsigned acquired{},released{},attempts{};
    bool playableContext{};
    std::uint64_t samples{},submittedFrames{};
    float minimumGap{10000};
    void Plan(const Aevrynd::HeightField3D& field,const std::array<Aevrynd::Mesh3D,3>& source,
              std::span<const Aevrynd::BoxCollider3D> existing){
        using namespace Aevrynd;
        if(!trees.empty()||!solids.empty())throw std::runtime_error("Slope grove plan already exists");
        const auto geometry=MeasureCanopy(source);
        constexpr std::array<Vec3,8> anchors{{{-23,0,50},{-27,0,60},{-37,0,77},{-22,0,76},
                                             {23,0,47},{32,0,57},{22,0,70},{39,0,78}}};
        for(unsigned id=0;id<anchors.size();++id){
            std::vector<Vec3> seats;
            const float side=anchors[id].x<0?-1.f:1.f;
            for(float z=45.f;z<=87.5f;z+=2.5f)for(float x=18.f;x<=45.f;x+=2.5f)
                seats.push_back({x*side,field.SampleHeight(x*side,z),z});
            std::stable_sort(seats.begin(),seats.end(),[&](Vec3 a,Vec3 b){
                return std::hypot(a.x-anchors[id].x,a.z-anchors[id].z)<std::hypot(b.x-anchors[id].x,b.z-anchors[id].z);});
            bool planted=false;
            for(auto root:seats){
                ++attempts;
                const float scale=id%2?1.15f:1.3f;
                Transform3D pose{{root.x,root.y-geometry.rootMinimum*scale,root.z},{0,float(id)*2.39996323f,0},{scale,scale,scale}};
                auto bounds=TransformBounds(MeshBounds3D{(geometry.minimum+geometry.maximum)*.5f,
                    Length((geometry.maximum-geometry.minimum)*.5f),(geometry.maximum-geometry.minimum)*.5f},pose);
                if(field.SampleNormal(root.x,root.z).y<.78f||!PropClearsObjectives({bounds.center,bounds.extents})||
                    !BroadleafClearsSettlement(bounds))continue;
                bool occupied=false;
                const auto clear=[&](const BoxCollider3D& box){return
                    std::abs(box.center.x-bounds.center.x)>=box.extents.x+bounds.extents.x+.03f||
                    std::abs(box.center.y-bounds.center.y)>=box.extents.y+bounds.extents.y+.03f||
                    std::abs(box.center.z-bounds.center.z)>=box.extents.z+bounds.extents.z+.03f;};
                for(const auto& box:existing)if(!clear(box)){occupied=true;break;}
                for(const auto& box:solids)if(!clear(box)){occupied=true;break;}
                if(occupied)continue;
                auto trunk=RootCanopyToTerrain(source[0],geometry,pose,field);
                Vec3 low=geometry.minimum,high=geometry.maximum;
                for(const auto& v:trunk.vertices)Expand(low,high,{v.x,v.y,v.z});
                const auto ex=(high-low)*.5f;
                bounds=TransformBounds(MeshBounds3D{(high+low)*.5f,Length(ex),ex},pose);
                for(const auto& box:existing)if(!clear(box)){occupied=true;break;}
                for(const auto& box:solids)if(!clear(box)){occupied=true;break;}
                if(occupied)continue;
                const auto matrix=pose.Matrix();float gap=10000;std::uint64_t checked{};
                for(unsigned slot=0;slot<3&&!occupied;++slot){const auto& mesh=slot?source[slot]:trunk;
                    const auto safe=[&](Vec3 local){const auto p=TransformPoint(matrix,local);++checked;
                        if(!Session::Finite(p)||!field.Contains(p.x,p.z))return false;
                        const float d=p.y-field.SampleHeight(p.x,p.z);gap=(std::min)(gap,d);
                        return d>=(slot==1?.04f:-.04f);};
                    for(const auto& v:mesh.vertices)if(!safe({v.x,v.y,v.z})){occupied=true;break;}
                    for(std::size_t f=0;f<mesh.indices.size()&&!occupied;f+=3){
                        const auto point=[&](unsigned j){const auto& v=mesh.vertices[mesh.indices[f+j]];return Vec3{v.x,v.y,v.z};};
                        const auto a=point(0),b=point(1),c=point(2);
                        for(const auto w:std::array<Vec3,4>{{{1.f/3,1.f/3,1.f/3},{.8f,.1f,.1f},{.1f,.8f,.1f},{.1f,.1f,.8f}}})
                            if(!safe(a*w.x+b*w.y+c*w.z)){occupied=true;break;}
                    }
                }
                if(occupied)continue;
                trees.push_back({pose,std::move(trunk),{bounds.center,bounds.extents}});
                solids.push_back(trees.back().solid);samples+=checked;minimumGap=(std::min)(minimumGap,gap);
                planted=true;break;
            }
            if(!planted)throw std::runtime_error("Slope grove complete seat unavailable id="+std::to_string(id));
        }
        // 碰撞只分解真木體，不把葉冠的空氣封成實體；每個完整三角面恰屬一個保守盒。
        // 固定每樹64葉盒，全部木三角面保留，兩態完整世界仍在原10000盒上限內。
        solids.clear();
        using Triangle=WoodTriangle;
        for(auto& tree:trees){
            std::vector<Triangle> triangles;
            const auto matrix=tree.pose.Matrix();
            const auto append=[&](const Mesh3D& mesh){for(std::size_t f=0;f<mesh.indices.size();f+=3){
                Triangle t;for(unsigned j=0;j<3;++j){const auto& v=mesh.vertices[mesh.indices[f+j]];t[j]=TransformPoint(matrix,{v.x,v.y,v.z});}
                triangles.push_back(t);}};
            append(tree.root);append(source[2]);
            if(triangles.size()<64)throw std::runtime_error("Slope grove wood partition scope");
            Vec3 low{10000,10000,10000},high{-10000,-10000,-10000};
            const float cutoff=tree.pose.position.y+1.85f;
            for(std::size_t f=0;f<tree.root.indices.size();f+=3){Triangle t;
                for(unsigned j=0;j<3;++j){const auto& v=tree.root.vertices[tree.root.indices[f+j]];t[j]=TransformPoint(matrix,{v.x,v.y,v.z});}
                for(unsigned j=0;j<3;++j){const auto a=t[j],b=t[(j+1)%3];
                    if(a.y<=cutoff)Expand(low,high,a);
                    if((a.y<=cutoff)!=(b.y<=cutoff))Expand(low,high,a+(b-a)*((cutoff-a.y)/(b.y-a.y)));}
            }
            if(low.x>high.x)throw std::runtime_error("Slope grove actual lower root empty");
            auto ext=(high-low)*.5f;ext.x=(std::max)(ext.x,.001f);ext.y=(std::max)(ext.y,.001f);ext.z=(std::max)(ext.z,.001f);
            rootMasks.push_back({(low+high)*.5f,ext});
            unsigned leafIndex{};
            std::function<void(std::size_t,std::size_t,unsigned)> partition;
            partition=[&](std::size_t first,std::size_t last,unsigned depth){
                Vec3 lo{10000,10000,10000},hi{-10000,-10000,-10000};
                for(auto i=first;i<last;++i)for(const auto p:triangles[i])Expand(lo,hi,p);
                if(depth==6){Vec3 e{.001f,.001f,.001f};
                    const auto centre=(lo+hi)*.5f;
                    for(auto i=first;i<last;++i)for(const auto p:triangles[i]){
                        e.x=(std::max)(e.x,std::abs(p.x-centre.x));e.y=(std::max)(e.y,std::abs(p.y-centre.y));e.z=(std::max)(e.z,std::abs(p.z-centre.z));}
                    const auto infinity=(std::numeric_limits<float>::infinity)();
                    e.x=std::nextafter(e.x,infinity);e.y=std::nextafter(e.y,infinity);e.z=std::nextafter(e.z,infinity);
                    for(auto i=first;i<last;++i)for(const auto p:triangles[i])if(std::abs(p.x-centre.x)>e.x||
                        std::abs(p.y-centre.y)>e.y||std::abs(p.z-centre.z)>e.z)throw std::runtime_error("Slope grove wood containment");
                    if(leafIndex>=64)throw std::runtime_error("Slope grove leaf identity");
                    tree.leafTriangles[leafIndex++]=last-first;
                    solids.push_back({centre,e});return;}
                const auto d=hi-lo;const unsigned axis=d.y>d.x?(d.z>d.y?2u:1u):(d.z>d.x?2u:0u);
                const auto coordinate=[&](const Triangle& t){const auto p=(t[0]+t[1]+t[2])*(1.f/3.f);return axis==0?p.x:axis==1?p.y:p.z;};
                std::stable_sort(triangles.begin()+first,triangles.begin()+last,[&](const Triangle& a,const Triangle& b){return coordinate(a)<coordinate(b);});
                const auto middle=first+(last-first)/2;
                if(middle==first||middle==last)throw std::runtime_error("Slope grove incomplete wood partition");
                partition(first,middle,depth+1);partition(middle,last,depth+1);
            };
            partition(0,triangles.size(),0);
            if(leafIndex!=64)throw std::runtime_error("Slope grove complete leaf count");
            tree.worldWood=std::move(triangles);
        }
        if(solids.size()!=512||rootMasks.size()!=8)throw std::runtime_error("Slope grove complete wood boxes");
    }
    void Upload(Aevrynd::Engine& e,const std::array<Aevrynd::MeshResource3D,3>& meshes,
                const std::array<Aevrynd::Material3D,3>& surfaces,bool playable=false){
        if(trees.size()!=8||!roots.empty())throw std::runtime_error("Slope grove complete upload scope");
        shared=meshes;materials=surfaces;playableContext=playable;
        try{for(const auto& t:trees){auto m=e.CreateMeshResource3D(t.root);
            if(!m)throw std::runtime_error("Slope grove root upload failed");roots.push_back(m);++acquired;
            if(!m.cpuMesh||m.cpuMesh->indices!=t.root.indices||m.cpuMesh->vertices.size()!=t.root.vertices.size()||
                std::memcmp(m.cpuMesh->vertices.data(),t.root.vertices.data(),t.root.vertices.size()*sizeof(Aevrynd::Vertex3D)))
                throw std::runtime_error("Slope grove actual root source mismatch");}}
        catch(...){Release(e);throw;}
    }
    void Submit(Aevrynd::Scene3D& scene){
        if(roots.size()!=8)throw std::runtime_error("Slope grove missing GPU roots");
        for(unsigned i=0;i<8;++i)for(unsigned slot=0;slot<3;++slot)
            if(!scene.Mesh(slot?shared[slot]:roots[i],trees[i].pose,materials[slot],{},{{Namespace,i*3+slot+1},1}))
                throw std::runtime_error("Slope grove scene insertion failed");
        ++submittedFrames;
    }
    void Observe(std::span<const Aevrynd::MeshInstance3D> items){
        std::array<bool,24> seen{};
        for(const auto& item:items)if(item.identity.id.high==Namespace){
            if(item.identity.id.low<1||item.identity.id.low>24||item.identity.generation!=1)throw std::runtime_error("Slope grove draw identity");
            const auto index=unsigned(item.identity.id.low-1),slot=index%3,tree=index/3;
            const auto& mesh=slot?shared[slot]:roots[tree];const auto& m=materials[slot];const auto matrix=trees[tree].pose.Matrix();
            if(seen[index]||!LandscapeTwigDraw::ExactResourceBindings(item,mesh.handle,m.textures)||
                item.tint!=Aevrynd::PackColorRgba8(m.tint)||item.surface!=Aevrynd::PackSurface3D(m.surface)||
                item.alphaMode!=m.alphaMode||item.alphaCutoff!=m.alphaCutoff||item.doubleSided!=m.doubleSided||item.materialClass!=m.materialClass||
                item.useUv!=m.useUv||item.textureScale!=m.textureScale||item.normalScale!=m.normalScale||
                std::memcmp(&item.transform,&matrix,sizeof(matrix)))throw std::runtime_error("Slope grove actual pose/material/resource");
            seen[index]=true;++observed[index];
        }
    }
    void Release(Aevrynd::Engine& e){for(auto& mesh:roots)if(mesh){e.DestroyMesh3D(mesh.handle);mesh={};++released;}}
    void Write(const std::filesystem::path& folder,bool detailed=true)const{
        if(std::filesystem::exists(folder/"slope-grove.txt"))throw std::runtime_error("Slope grove refuses overwrite");
        std::ofstream out(folder/"slope-grove.txt");out.precision(9);
        out<<"profile=young-canopy-slope-bands-v1\ninstances="<<trees.size()<<"\nconservative_actual_wood_boxes="<<solids.size()
            <<"\nactual_lower_root_masks="<<rootMasks.size()<<"\nall_rest_wood_triangles_partitioned=1"
            <<"\nsampled_ground_checks="<<samples<<"\nminimum_sampled_gap="<<minimumGap<<"\nattempts="<<attempts
            <<"\nacquired="<<acquired<<"\nreleased="<<released<<"\nsubmitted_frames="<<submittedFrames
            <<"\ncontinuous_ground_contact_qualified=0\nmanual_entry_allowed="<<playableContext<<"\nart_accepted=0\n";
        for(unsigned i=0;i<24;++i)out<<"observed_"<<i<<'='<<observed[i]<<'\n';
        out.close();if(!out)throw std::runtime_error("Slope grove receipt failed");
        std::ofstream poses(folder/"slope-grove-trees.tsv");poses.precision(9);
        poses<<"id\tx\ty\tz\tyaw\tscale\tbound_cx\tbound_cy\tbound_cz\tbound_ex\tbound_ey\tbound_ez\n";
        for(unsigned i=0;i<trees.size();++i){const auto& t=trees[i];const auto& p=t.pose;const auto& b=t.solid;
            poses<<i<<'\t'<<p.position.x<<'\t'<<p.position.y<<'\t'<<p.position.z<<'\t'<<p.rotation.y<<'\t'<<p.scale.x
                <<'\t'<<b.center.x<<'\t'<<b.center.y<<'\t'<<b.center.z<<'\t'<<b.extents.x<<'\t'<<b.extents.y<<'\t'<<b.extents.z<<'\n';}
        poses.close();if(!poses)throw std::runtime_error("Slope grove poses failed");
        if(!detailed)return;
        std::ofstream boxes(folder/"slope-grove-wood.tsv");boxes.precision(9);
        boxes<<"tree\tpart\ttriangles\tcx\tcy\tcz\tex\tey\tez\n";
        for(unsigned i=0;i<trees.size();++i){const auto& t=trees[i];
            for(unsigned part=0;part<64;++part){const auto& b=solids.at(i*64+part);
                boxes<<i<<'\t'<<part<<'\t'<<t.leafTriangles[part]<<'\t'<<b.center.x<<'\t'<<b.center.y<<'\t'<<b.center.z
                    <<'\t'<<b.extents.x<<'\t'<<b.extents.y<<'\t'<<b.extents.z<<'\n';}
            const auto name="slope-grove-"+std::to_string(i);
            std::ofstream world(folder/(name+".wood.f32tri"),std::ios::binary);
            static_assert(sizeof(WoodTriangle)==36);
            world.write(reinterpret_cast<const char*>(t.worldWood.data()),std::streamsize(t.worldWood.size()*sizeof(WoodTriangle)));
            world.close();if(!world)throw std::runtime_error("Slope grove actual world wood export");
            std::ofstream mesh(folder/(name+".root.aemesh"),std::ios::binary);
            static_assert(sizeof(Aevrynd::Vertex3D)==28);
            const std::uint32_t header[]{0x334d4541u,1u,std::uint32_t(t.root.vertices.size()),std::uint32_t(t.root.indices.size())};
            mesh.write(reinterpret_cast<const char*>(header),sizeof(header));
            mesh.write(reinterpret_cast<const char*>(t.root.vertices.data()),std::streamsize(t.root.vertices.size()*sizeof(Aevrynd::Vertex3D)));
            mesh.write(reinterpret_cast<const char*>(t.root.indices.data()),std::streamsize(t.root.indices.size()*sizeof(std::uint32_t)));
            mesh.close();if(!mesh)throw std::runtime_error("Slope grove actual root export");
            std::ofstream transform(folder/(name+".mat4.f32"),std::ios::binary);const auto matrix=t.pose.Matrix();
            for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c){const auto v=matrix[r][c];transform.write(reinterpret_cast<const char*>(&v),sizeof(v));}
            transform.close();if(!transform)throw std::runtime_error("Slope grove actual matrix export");
        }
        boxes.close();if(!boxes)throw std::runtime_error("Slope grove actual wood boxes export");
    }
};
}
