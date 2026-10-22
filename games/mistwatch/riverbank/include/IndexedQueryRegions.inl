    bool QueryRegions(bool repaired,std::span<const Aevrynd::ClosedAabb3D> regions,std::vector<Aevrynd::BoxCollider3D>& output)const {
        if(regions.empty()||regions.size()>3)return false;
        for(const auto& q:regions)for(unsigned a=0;a<3;++a)if(!std::isfinite(q.minimum[a])||!std::isfinite(q.maximum[a])||q.minimum[a]>q.maximum[a])return false;
        std::vector<std::uint32_t> scratch(10000),ordinals(10000),merged;merged.reserve(10000);
        const auto& source=worlds_[unsigned(repaired)];std::vector<unsigned char> seen(source.size());
        for(const auto& q:regions){std::size_t count{};
            if(!indexes_[unsigned(repaired)]->QueryOrdinals(q,scratch,ordinals,count))return false;
            for(std::size_t i=0;i<count;++i){const auto id=ordinals[i];if(id>=source.size())return false;
                if(!seen[id]){if(merged.size()==10000)return false;seen[id]=1;merged.push_back(id);}}
        }
        std::sort(merged.begin(),merged.end());std::vector<Aevrynd::BoxCollider3D> next;next.reserve(merged.size());
        for(auto id:merged)next.push_back(source[id]);output.swap(next);return true;
    }
    Aevrynd::ClosedAabb3D Rectangle(double lx,double hx,double lz,double hz)const{return {{lx,lowY_,lz},{hx,highY_,hz}};}
    bool Query(bool repaired,double lx,double hx,double lz,double hz,std::vector<Aevrynd::BoxCollider3D>& output)const {
        const auto q=Rectangle(lx,hx,lz,hz);return QueryRegions(repaired,std::span<const Aevrynd::ClosedAabb3D>(&q,1),output);
    }
