#include "TrainingPanel.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace ArenaLab {
namespace {
const char* modes[]={"Q-table","DQN","PPO","SAC (discrete)"};
const char* ids[]={"QTABLE","DQN","PPO","SAC"};
}
TrainingPanel::~TrainingPanel(){
    if(process){ofstream(runPath+"/STOP");CloseHandle(process);}
    if(job)CloseHandle(job);
}
void TrainingPanel::refreshRuns(){
    runs.clear();error_code error;filesystem::directory_iterator it("runs",error),end;
    while(!error&&it!=end){if(it->is_directory(error))runs.push_back(it->path().generic_string());it.increment(error);}
    sort(runs.rbegin(),runs.rend());
}
void TrainingPanel::selectRun(const string& path){
    runPath=path;lastPoll=0;
    ifstream file(runPath+"/config.json");string content((istreambuf_iterator<char>(file)),istreambuf_iterator<char>());
    for(int i=0;i<4;++i)if(content.find(string("\"mode\": \"")+ids[i]+"\"")!=string::npos)mode=i;
}
void TrainingPanel::startSmoke(int algorithm){mode=algorithm;episodes=8;evalEvery=4;evalGames=4;start();}
void TrainingPanel::start(){
    auto python=filesystem::absolute(".venv/Scripts/python.exe");
    if(!filesystem::exists(python)||!filesystem::exists("build/Release/ArenaTraining.dll")){status="Run setup-training.cmd and build.cmd first.";return;}
    if(process)return;
    string source=runPath+"/latest.pt";
    if(resume&&!filesystem::exists(source)){status="No latest.pt in selected run.";return;}
    // 학습 실행은 충돌하지 않는 별도 폴더를 사용한다.
    SYSTEMTIME time;GetLocalTime(&time);char stamp[80];
    sprintf_s(stamp,"runs/%04u%02u%02u-%02u%02u%02u-%llu-%s",time.wYear,time.wMonth,time.wDay,time.wHour,time.wMinute,time.wSecond,GetTickCount64(),ids[mode]);
    runPath=stamp;error_code error;filesystem::create_directories(runPath,error);
    if(error){status="Cannot create run folder.";return;}
    ostringstream cmd;cmd<<'"'<<python.string()<<"\" -u training/train.py --run \""<<runPath<<"\" --mode "<<ids[mode]
        <<" --episodes "<<episodes<<" --lr "<<lr<<" --table-lr "<<tableLr<<" --gamma "<<gamma
        <<" --epsilon "<<epsilon<<" --epsilon-min "<<epsilonMin<<" --epsilon-decay "<<decay
        <<" --batch "<<batch<<" --rollout "<<rollout<<" --clip "<<clip<<" --gae-lambda "<<lambda
        <<" --entropy "<<entropy<<" --sac-alpha "<<alpha<<" --eval-every "<<evalEvery<<" --eval-games "<<evalGames<<" --seed "<<seed<<" --patience "<<patience;
    if(resume)cmd<<" --resume \""<<source<<'"';
    SECURITY_ATTRIBUTES sa{sizeof(sa),nullptr,TRUE};
    HANDLE log=CreateFileW(filesystem::path(runPath+"/console.log").c_str(),GENERIC_WRITE,FILE_SHARE_READ,&sa,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    HANDLE input=CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&sa,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(log==INVALID_HANDLE_VALUE||input==INVALID_HANDLE_VALUE){if(log!=INVALID_HANDLE_VALUE)CloseHandle(log);if(input!=INVALID_HANDLE_VALUE)CloseHandle(input);status="Cannot open trainer log.";return;}
    STARTUPINFOA si{};si.cb=sizeof(si);si.dwFlags=STARTF_USESTDHANDLES;si.hStdOutput=si.hStdError=log;si.hStdInput=input;
    PROCESS_INFORMATION pi{};string command=cmd.str();
    job=CreateJobObjectW(nullptr,nullptr);JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    bool jobOk=job&&SetInformationJobObject(job,JobObjectExtendedLimitInformation,&limits,sizeof(limits));
    bool created=jobOk&&CreateProcessA(python.string().c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW|CREATE_SUSPENDED,nullptr,nullptr,&si,&pi);
    CloseHandle(log);CloseHandle(input);
    if(!created){if(job)CloseHandle(job);job=nullptr;status="Could not launch trainer.";return;}
    if(!AssignProcessToJobObject(job,pi.hProcess)){TerminateProcess(pi.hProcess,1);CloseHandle(pi.hThread);CloseHandle(pi.hProcess);CloseHandle(job);job=nullptr;status="Could not attach trainer lifecycle.";return;}
    process=pi.hProcess;exitCode=STILL_ACTIVE;ResumeThread(pi.hThread);CloseHandle(pi.hThread);
    status="Training in Python. Load a checkpoint to preview in this arena.";lastPoll=0;refreshRuns();
}
void TrainingPanel::poll(){
    if(GetTickCount64()-lastPoll<500)return;lastPoll=GetTickCount64();
    if(process&&WaitForSingleObject(process,0)==WAIT_OBJECT_0){GetExitCodeProcess(process,&exitCode);CloseHandle(process);process=nullptr;CloseHandle(job);job=nullptr;status=exitCode==0?"Training finished. Best and latest checkpoints are available.":"Training failed. See selected run/error.txt or console.log.";}
    rewards.clear();means.clear();evalRewards.clear();wins.clear();losses.clear();actorLosses.clear();entropies.clear();latestEpisode=updates=0;bestWin=latestWin=0;
    ifstream file(runPath+"/metrics.tsv");string line;getline(file,line);
    while(getline(file,line)){
        istringstream row(line);string token;vector<float> v;while(getline(row,token,'\t'))v.push_back(strtof(token.c_str(),nullptr));
        if(v.size()!=12)continue;
        latestEpisode=static_cast<int>(v[0]);updates=static_cast<int>(v[11]);latestEpsilon=v[10];
        if(v[0]>0){rewards.push_back(v[1]);means.push_back(v[2]);losses.push_back(v[6]);actorLosses.push_back(v[7]);entropies.push_back(v[8]);}
        if(isfinite(v[4])){wins.push_back(v[4]*100);evalRewards.push_back(v[3]);latestWin=v[4]*100;bestWin=max(bestWin,latestWin);}
    }
}
void TrainingPanel::draw(ImVec2 position,ImVec2 size){
    poll();ImGui::SetNextWindowPos(position);ImGui::SetNextWindowSize(size);
    ImGui::Begin("TRAINING LAB / Python + C++",nullptr,ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoCollapse);
    ImGui::BeginDisabled(running());ImGui::SetNextItemWidth(145);ImGui::Combo("Mode",&mode,modes,4);
    ImGui::SameLine();if(ImGui::Button("Start training"))start();ImGui::SameLine();ImGui::Checkbox("Resume selected latest.pt",&resume);ImGui::EndDisabled();
    ImGui::SameLine();ImGui::BeginDisabled(!running());if(ImGui::Button("Stop")){ofstream(runPath+"/STOP");status="Stopping after current episode/evaluation...";}ImGui::EndDisabled();
    if(ImGui::CollapsingHeader("Hyperparameters (applied to next run)")){
        ImGui::BeginDisabled(running());
        ImGui::Columns(3,"parameters",false);
        ImGui::InputInt("Episodes",&episodes);ImGui::InputInt("Eval every",&evalEvery);ImGui::InputInt("Eval games",&evalGames);ImGui::InputInt("Seed",&seed);ImGui::InputInt("Early stop checks",&patience);
        ImGui::NextColumn();ImGui::InputFloat("NN learning rate",&lr,0,0,"%.6f");ImGui::SliderFloat("Q learning rate",&tableLr,.001f,1);ImGui::SliderFloat("Gamma / second",&gamma,.8f,1,"%.3f");ImGui::SliderFloat("Epsilon start",&epsilon,0,1);ImGui::SliderFloat("Epsilon min",&epsilonMin,0,1);ImGui::SliderFloat("Epsilon decay/episode",&decay,.9f,1,"%.4f");
        ImGui::NextColumn();ImGui::InputInt("Batch",&batch);ImGui::InputInt("PPO rollout",&rollout);ImGui::SliderFloat("PPO clip",&clip,.05f,.4f);ImGui::SliderFloat("GAE lambda",&lambda,0,1);ImGui::SliderFloat("PPO entropy",&entropy,0,.1f);ImGui::SliderFloat("SAC alpha (fixed)",&alpha,.001f,1);
        ImGui::Columns(1);ImGui::EndDisabled();
        episodes=clamp(episodes,1,100000);evalEvery=clamp(evalEvery,1,10000);evalGames=clamp(evalGames,1,1000);seed=max(0,seed);patience=max(0,patience);batch=clamp(batch,8,512);rollout=clamp(rollout,32,4096);lr=clamp(lr,.000001f,.1f);epsilonMin=min(epsilonMin,epsilon);
    }
    ImGui::BeginDisabled(running());ImGui::SetNextItemWidth(320);
    if(ImGui::BeginCombo("Run",runPath.empty()?"Select run":runPath.c_str())){if(runs.empty())refreshRuns();for(auto& path:runs)if(ImGui::Selectable(path.c_str(),path==runPath))selectRun(path);ImGui::EndCombo();}
    ImGui::SameLine();if(ImGui::Button("Refresh runs"))refreshRuns();ImGui::EndDisabled();
    auto request=[&](const char* name){auto nn=runPath+"/"+name+".nn",q=runPath+"/"+name+".qtable";if(filesystem::exists(nn))loadRequested=nn;else if(filesystem::exists(q))loadRequested=q;else status="Checkpoint not available yet.";};
    ImGui::SameLine();if(ImGui::Button("Load best"))request("best");ImGui::SameLine();if(ImGui::Button("Load latest"))request("latest");
    ImGui::Text("Episode %d | updates %d | eval %.1f%% / best %.1f%%",latestEpisode,updates,latestWin,bestWin);
    if(isfinite(latestEpsilon))ImGui::Text("Epsilon %.3f (Q-table/DQN only)",latestEpsilon);
    if(!means.empty())ImGui::Text("Mean return %.2f | critic loss %.4f | evaluation sample count: see run config.json",means.back(),losses.back());
    float width=(ImGui::GetContentRegionAvail().x-24)/3;
    ImGui::BeginGroup();ImGui::TextUnformatted("Train reward (20-episode mean)");ImGui::PlotLines("##reward",means.data(),static_cast<int>(means.size()),0,nullptr,FLT_MAX,FLT_MAX,{width,68});ImGui::EndGroup();ImGui::SameLine();
    ImGui::BeginGroup();ImGui::TextUnformatted("Eval win % (fixed-seed checks)");ImGui::PlotLines("##wins",wins.data(),static_cast<int>(wins.size()),0,nullptr,0,100,{width,68});ImGui::EndGroup();ImGui::SameLine();
    ImGui::BeginGroup();ImGui::TextUnformatted("Critic loss / Q TD error");ImGui::PlotLines("##loss",losses.data(),static_cast<int>(losses.size()),0,nullptr,FLT_MAX,FLT_MAX,{width,68});ImGui::EndGroup();
    if(latestWin+5<bestWin)ImGui::TextColored({1,.65f,.3f,1},"Evaluation below best. Compare checkpoints; this may also be sampling noise.");
    ImGui::TextWrapped("%s",status.c_str());
    if(ImGui::CollapsingHeader("More curves / interpretation")){
        ImGui::TextWrapped("X: episodes for train/loss, evaluation checkpoints for win/reward. Loss is not skill. Early stop 0 = disabled. Validation uses fixed seeds, not a final held-out test.");
        ImGui::PlotLines("Raw return",rewards.data(),static_cast<int>(rewards.size()),0,nullptr,FLT_MAX,FLT_MAX,{-1,60});
        ImGui::PlotLines("Eval reward",evalRewards.data(),static_cast<int>(evalRewards.size()),0,nullptr,FLT_MAX,FLT_MAX,{-1,60});
        ImGui::PlotLines("Actor loss",actorLosses.data(),static_cast<int>(actorLosses.size()),0,nullptr,FLT_MAX,FLT_MAX,{-1,60});
        ImGui::PlotLines("Entropy",entropies.data(),static_cast<int>(entropies.size()),0,nullptr,FLT_MAX,FLT_MAX,{-1,60});
    }
    ImGui::End();
}
}
