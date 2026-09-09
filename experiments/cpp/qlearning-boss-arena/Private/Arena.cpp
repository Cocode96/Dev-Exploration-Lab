#include "Arena.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <filesystem>

namespace ArenaLab {
float length(Vec v){return sqrt(v.x*v.x+v.y*v.y);}
Vec unit(Vec v){float n=length(v);return n>0.001f?v*(1.f/n):Vec{1,0};}
const char* actionName(int a){static const char* names[]={"APPROACH","ORBIT","BASIC SLASH","RAM CHARGE","SHOCK PULSE","FAN VOLLEY"};return names[a];}
const char* phaseName(Phase p){static const char* names[]={"DECIDE","WINDUP","ACTIVE","RECOVERY","DEAD"};return names[static_cast<int>(p)];}
int QTable::choose(int s,const array<bool,ActionCount>& mask,mt19937& rng,bool explore) const {
    vector<int> choices; float best=-numeric_limits<float>::max();
    bool random=explore && uniform_real_distribution<float>(0,1)(rng)<epsilon;
    for(int a=0;a<ActionCount;++a) if(mask[a]) {
        if(!random && values[s][a]>best){choices.clear();best=values[s][a];}
        if(random || values[s][a]==best) choices.push_back(a);
    }
    return choices.empty()?Approach:choices[uniform_int_distribution<size_t>(0,choices.size()-1)(rng)];
}
void QTable::learn(int s,int a,float r,int next,const array<bool,ActionCount>& mask,bool done,float seconds){
    float best=-numeric_limits<float>::max();
    for(int n=0;n<ActionCount;++n)if(mask[n])best=max(best,values[next][n]);
    values[s][a]+=learningRate*(r+(done?0.f:pow(gamma,seconds)*best)-values[s][a]); ++updates;
}
bool QTable::save(const string& path) const {
    ofstream file(path+".tmp"); if(!file)return false;
    file<<"CONTAINMENT_Q1 "<<updates<<'\n'<<setprecision(9);
    for(auto& row:values){for(float v:row)file<<v<<' ';file<<'\n';}
    file.close(); if(file.fail())return false;
    // 호출자가 기존 파일을 보존할 수 있도록 교체 실패를 반환한다.
    error_code error; filesystem::rename(path+".tmp",path,error);
    return !error;
}
bool QTable::load(const string& path){
    ifstream file(path);string magic; QTable candidate;
    if(!(file>>magic>>candidate.updates)||magic!="CONTAINMENT_Q1")return false;
    for(auto& row:candidate.values)for(float& v:row)if(!(file>>v)||!isfinite(v))return false;
    file>>ws;if(!file.eof())return false;*this=candidate;return true;
}
Arena::Arena(unsigned seed):rng(seed){reset();}
void Arena::reset(){
    player.position={155,320};player.hp=100;player.radius=13;
    boss.position={740,320};boss.hp=260;boss.radius=29;
    bullets.clear();phase=Phase::Decide;action=Approach;cooldown={};
    elapsed=phaseTime=timer=reward=decisionSeconds=dodgeTime=dodgeCooldown=shotCooldown=0;
    botClock=uniform_real_distribution<float>(0,6.28f)(rng);
    done=pending=hit=false;++episode;
}
int Arena::state() const {
    float d=length(player.position-boss.position);int range=d<110?0:d<270?1:2;
    int cd=(cooldown[0]>0?1:0)|(cooldown[1]>0?2:0)|(cooldown[2]>0?4:0);
    return ((range*2+(boss.hp<100?1:0))*2+(player.hp<40?1:0))*8+cd;
}
array<bool,ActionCount> Arena::legal() const {return {true,true,true,cooldown[0]<=0,cooldown[1]<=0,cooldown[2]<=0};}
void Arena::move(Actor& actor,Vec delta){
    Vec p=actor.position+delta;
    p.x=clamp(p.x,38+actor.radius,882-actor.radius);p.y=clamp(p.y,38+actor.radius,602-actor.radius);
    for(Vec c:pillars){Vec d=p-c;float n=length(d),r=32+actor.radius;if(n<r)p=c+unit(d)*r;}
    actor.position=p;
}
Input Arena::botInput(){
    Vec d=boss.position-player.position;float distance=length(d);Vec u=unit(d);
    Vec tangent{-u.y,u.x};float toward=distance>245?1.f:distance<175?-1.f:0.f;
    Input result;result.move=unit(u*toward+tangent*(sin(botClock)>0?0.65f:-0.65f));
    result.aim=boss.position;result.fire=true;
    result.dodge=phase==Phase::Windup && timer<0.2f && distance<280;
    return result;
}
void Arena::finish(QTable& q,bool terminal){
    if(pending){q.learn(decisionState,action,reward,state(),legal(),terminal,decisionSeconds);lastReward=reward;}
    pending=false;
}
void Arena::tick(QTable& q,const Input& input,bool learning,int forcedAction){
    if(done)return;
    constexpr float dt=1.f/60.f;elapsed+=dt;botClock+=dt*0.6f;decisionSeconds+=dt;
    const float oldPlayer=player.hp,oldBoss=boss.hp;
    for(float& cd:cooldown)cd=max(0.f,cd-dt);
    shotCooldown=max(0.f,shotCooldown-dt);dodgeCooldown=max(0.f,dodgeCooldown-dt);dodgeTime=max(0.f,dodgeTime-dt);
    if(input.dodge&&dodgeCooldown<=0){dodgeTime=.16f;dodgeCooldown=1.3f;dodgeDirection=length(input.move)>.01f?unit(input.move):unit(input.aim-player.position);}
    if(dodgeTime>0)move(player,dodgeDirection*(720*dt));
    else if(length(input.move)>.01f)move(player,unit(input.move)*(210*dt));
    if(input.fire&&shotCooldown<=0){bullets.push_back({player.position,unit(input.aim-player.position)*620,1.6f,false});shotCooldown=.24f;}

    if(phase==Phase::Decide){
        finish(q,false);decisionState=state();
        action=forcedAction>=0&&forcedAction<ActionCount&&legal()[forcedAction]?forcedAction:q.choose(decisionState,legal(),rng,learning);
        pending=learning;reward=decisionSeconds=0;lockedAim=unit(player.position-boss.position);hit=false;
        if(action<Slash){phase=Phase::Active;timer=.4f;}
        else{phase=Phase::Windup;timer=action==Slash?.3f:action==Charge?.65f:action==Pulse?.9f:.55f;}
        phaseTime=timer;
    }
    timer-=dt;
    if(phase==Phase::Windup&&timer<=0){
        phase=Phase::Active;timer=action==Charge?.42f:.12f;phaseTime=timer;
        if(action>=Charge)cooldown[action-Charge]=action==Charge?3.5f:action==Pulse?5.f:3.f;
        if(action==Fan){float angle=atan2(lockedAim.y,lockedAim.x);for(int i=-3;i<=3;++i){float a=angle+i*.17f;bullets.push_back({boss.position+lockedAim*35,{cos(a)*310,sin(a)*310},2.4f,true});}}
    }
    if(phase==Phase::Active){
        Vec toward=unit(player.position-boss.position);
        if(action==Approach)move(boss,toward*(145*dt));
        if(action==Orbit)move(boss,Vec{-toward.y,toward.x}*(145*dt));
        if(action==Charge)move(boss,lockedAim*(650*dt));
        float d=length(player.position-boss.position);
        bool intersects=action==Slash?d<91 && (toward.x*lockedAim.x+toward.y*lockedAim.y)>.25f:action==Pulse?d<170:action==Charge?d<player.radius+boss.radius:false;
        if(!hit&&intersects&&dodgeTime<=0){player.hp=max(0.f,player.hp-(action==Slash?12.f:20.f));hit=true;}
        if(timer<=0){phase=Phase::Recovery;timer=action<Slash?.05f:.5f;phaseTime=timer;}
    }else if(phase==Phase::Recovery&&timer<=0)phase=Phase::Decide;
    for(auto& b:bullets){
        b.position=b.position+b.velocity*dt;b.life-=dt;
        if(b.position.x<38||b.position.x>882||b.position.y<38||b.position.y>602)b.life=0;
        for(Vec pillar:pillars)if(length(b.position-pillar)<36)b.life=0;
        Actor& target=b.enemy?player:boss;
        if(b.life>0&&length(b.position-target.position)<target.radius+5){
            if(!b.enemy||dodgeTime<=0)target.hp=max(0.f,target.hp-(b.enemy?9.f:5.f));b.life=0;
        }
    }
    erase_if(bullets,[](const Bullet& b){return b.life<=0;});
    reward+=(oldPlayer-player.hp)*.12f-(oldBoss-boss.hp)*.1f-dt*.008f;
    if(player.hp<=0||boss.hp<=0||elapsed>=35){
        done=true;reward+=player.hp<=0?20.f:boss.hp<=0?-20.f:0.f;
        finish(q,true);phase=Phase::Dead;
    }
}
}
