#include "NeuralPolicy.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <iomanip>
#include <filesystem>

namespace ArenaLab {
NeuralPolicy::Observation NeuralPolicy::observe(const Arena& a){
    Vec d=a.player.position-a.boss.position;
    Observation x{a.player.position.x/920,a.player.position.y/640,a.boss.position.x/920,a.boss.position.y/640,
        d.x/920,d.y/640,length(d)/1100,a.player.hp/100,a.boss.hp/260,
        a.cooldown[0]/3.5f,a.cooldown[1]/5,a.cooldown[2]/3,a.dodgeCooldown/1.3f,
        a.dodgeTime/.16f,a.shotCooldown/.24f,a.elapsed/35,0,0,0,0,0,0,0,0};
    float nearest=numeric_limits<float>::max();
    for(const auto& b:a.bullets)if(!b.enemy){
        Vec delta=b.position-a.boss.position;float distance=length(delta);
        if(distance<nearest){nearest=distance;x[16]=delta.x/920;x[17]=delta.y/640;x[18]=b.velocity.x/620;x[19]=b.velocity.y/620;}
    }
    x[20]=min(1.f,float(a.bullets.size())/32);x[21]=nearest<numeric_limits<float>::max()?1.f:0.f;
    float cover=1100;for(Vec pillar:a.pillars)cover=min(cover,length(pillar-a.boss.position));
    x[22]=cover/1100;x[23]=1;
    return x;
}
bool NeuralPolicy::load(const string& path){
    ifstream file(path);NeuralPolicy candidate;string magic;int in,h,out;
    if(!(file>>magic>>candidate.algorithm>>in>>h>>out>>candidate.updates)||magic!="ARENA_NN_V1"||in!=ObservationCount||h!=Hidden||out!=ActionCount)return false;
    if(candidate.algorithm!="DQN"&&candidate.algorithm!="PPO"&&candidate.algorithm!="SAC")return false;
    auto read=[&](auto& values){for(float& v:values)if(!(file>>v)||!isfinite(v)||abs(v)>1e6f)return false;return true;};
    if(!read(candidate.w1)||!read(candidate.b1)||!read(candidate.w2)||!read(candidate.b2)||!read(candidate.w3)||!read(candidate.b3))return false;
    file>>ws;if(!file.eof())return false;
    candidate.ready=true;*this=candidate;return true;
}
bool NeuralPolicy::save(const string& path) const {
    if(!ready)return false;
    ofstream file(path+".tmp");if(!file)return false;
    file<<"ARENA_NN_V1 "<<algorithm<<' '<<ObservationCount<<' '<<Hidden<<' '<<ActionCount<<' '<<updates<<'\n'<<setprecision(9);
    auto write=[&](const auto& values){for(float v:values)file<<v<<' ';file<<'\n';};
    write(w1);write(b1);write(w2);write(b2);write(w3);write(b3);file.close();if(file.fail())return false;
    error_code error;filesystem::rename(path+".tmp",path,error);return !error;
}
array<float,ActionCount> NeuralPolicy::forward(const Observation& x) const {
    array<float,Hidden> a{},b{};array<float,ActionCount> out{};
    for(int i=0;i<Hidden;++i){a[i]=b1[i];for(int j=0;j<ObservationCount;++j)a[i]+=w1[i*ObservationCount+j]*x[j];a[i]=max(0.f,a[i]);}
    for(int i=0;i<Hidden;++i){b[i]=b2[i];for(int j=0;j<Hidden;++j)b[i]+=w2[i*Hidden+j]*a[j];b[i]=max(0.f,b[i]);}
    for(int i=0;i<ActionCount;++i){out[i]=b3[i];for(int j=0;j<Hidden;++j)out[i]+=w3[i*Hidden+j]*b[j];}
    return out;
}
int NeuralPolicy::choose(const Arena& arena) const {
    auto values=forward(observe(arena));auto mask=arena.legal();int best=Approach;
    for(int a=0;a<ActionCount;++a)if(mask[a]&&values[a]>values[best])best=a;
    return best;
}
}
