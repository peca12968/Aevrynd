    bool Nearby(bool repaired,Aevrynd::Vec3 feet,float radius,std::vector<Aevrynd::BoxCollider3D>& output)const {
        if(!Session::Finite(feet)||!std::isfinite(radius)||radius<=0)return false;
        return Query(repaired,double(feet.x)-radius,double(feet.x)+radius,double(feet.z)-radius,double(feet.z)+radius,output);
    }
    bool Body(bool repaired,Aevrynd::Vec3 feet,std::vector<Aevrynd::BoxCollider3D>& output)const{return Nearby(repaired,feet,4,output);}
    bool Enemy(bool repaired,Aevrynd::Vec3 feet,Aevrynd::Vec3 player,Aevrynd::Vec3 strike,bool windup,std::vector<Aevrynd::BoxCollider3D>& output)const {
        if(!Session::Finite(feet)||!Session::Finite(player)||!Session::Finite(strike))return false;
        std::array<Aevrynd::ClosedAabb3D,3> regions{};regions[0]=Rectangle(double(feet.x)-4,double(feet.x)+4,double(feet.z)-4,double(feet.z)+4);unsigned count=1;
        if(windup)for(const auto target:{player,strike})regions[count++]=Rectangle((std::min)(double(feet.x),double(target.x)),(std::max)(double(feet.x),double(target.x)),(std::min)(double(feet.z),double(target.z)),(std::max)(double(feet.z),double(target.z)));
        return QueryRegions(repaired,std::span<const Aevrynd::ClosedAabb3D>(regions.data(),count),output);
    }
    bool Camera(bool repaired,Aevrynd::Vec3 feet,float distance,const Aevrynd::Camera3D* previous,std::vector<Aevrynd::BoxCollider3D>& output)const {
        if(!Session::Finite(feet)||!std::isfinite(distance)||distance<2.5f||distance>10||(previous&&!Session::Finite(previous->position)))return false;
        const double r=double(distance)+2;double lx=double(feet.x)-r,hx=double(feet.x)+r,lz=double(feet.z)-r,hz=double(feet.z)+r;
        if(previous){lx=(std::min)(lx,double(previous->position.x)-1);hx=(std::max)(hx,double(previous->position.x)+1);lz=(std::min)(lz,double(previous->position.z)-1);hz=(std::max)(hz,double(previous->position.z)+1);}
        return Query(repaired,lx,hx,lz,hz,output);
    }
    bool Camera(bool repaired,Aevrynd::Vec3 feet,float distance,std::vector<Aevrynd::BoxCollider3D>& output)const{return Camera(repaired,feet,distance,nullptr,output);}
