#include "NeuralPolicy.h"
#include <algorithm>

using namespace ArenaLab;
namespace {
Arena arenas[2];
QTable unused;
void output(int index,float* observation,int* mask){
    auto x=NeuralPolicy::observe(arenas[index]);copy(x.begin(),x.end(),observation);
    auto legal=arenas[index].legal();for(int i=0;i<ActionCount;++i)mask[i]=legal[i]?1:0;
}
}
extern "C" {
__declspec(dllexport) int arena_version(){return 1;}
__declspec(dllexport) int arena_reset(int index,unsigned seed,float* observation,int* mask){
    if(index<0||index>1||!observation||!mask)return -1;
    arenas[index].reseed(seed);output(index,observation,mask);return 0;
}
// 한 step은 한 프레임이 아니라 FSM 행동이 끝날 때까지 진행한다.
__declspec(dllexport) int arena_step(int index,int action,float* observation,int* mask,float* result){
    if(index<0||index>1||!observation||!mask||!result||action<0||action>=ActionCount)return -1;
    auto& a=arenas[index];if(a.done||!a.legal()[action])return -2;
    float hp=a.player.hp,bhp=a.boss.hp,time=a.elapsed;
    do{a.tick(unused,a.botInput(),false,action);}while(!a.done&&a.phase!=Phase::Decide);
    float seconds=a.elapsed-time;
    result[0]=(hp-a.player.hp)*.12f-(bhp-a.boss.hp)*.1f-seconds*.008f;
    if(a.done)result[0]+=a.player.hp<=0?20.f:a.boss.hp<=0?-20.f:0.f;
    result[1]=a.done?1.f:0.f;result[2]=seconds;result[3]=a.player.hp<=0?1.f:0.f;
    result[4]=a.boss.hp<=0?1.f:0.f;result[5]=a.state();
    output(index,observation,mask);return 0;
}
__declspec(dllexport) int policy_forward(const char* path,const float* observation,float* values){
    if(!path||!observation||!values)return -1;NeuralPolicy policy;if(!policy.load(path))return -2;
    NeuralPolicy::Observation x;copy_n(observation,x.size(),x.begin());auto out=policy.forward(x);copy(out.begin(),out.end(),values);return 0;
}
}
