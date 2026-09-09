#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <chrono>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cmath>
#include <algorithm>
#include "Arena.h"
#include "NeuralPolicy.h"
#include "TrainingPanel.h"
#include "InputManager.h"
#include "DebugUiManager.h"
#include "imgui.h"

using namespace ArenaLab;
using Microsoft::WRL::ComPtr;
namespace {
Engine::InputManager inputManager;
Engine::DebugUiManager ui;
ComPtr<ID3D11Device> device;
ComPtr<ID3D11DeviceContext> context;
ComPtr<IDXGISwapChain> swapchain;
ComPtr<ID3D11RenderTargetView> target;
bool resizePending=false;
LRESULT CALLBACK WindowProc(HWND window,UINT message,WPARAM w,LPARAM l){
    if(message==WM_DESTROY){PostQuitMessage(0);return 0;}
    if(message==WM_KILLFOCUS)inputManager.release_active_input();
    if(message==WM_SIZE){resizePending=true;return 0;}
    inputManager.handle_message(window,message,w,l);
    if(ui.handle_window_message(window,message,w,l))return 1;
    return DefWindowProcW(window,message,w,l);
}
bool renderTarget(){ComPtr<ID3D11Texture2D> back;return SUCCEEDED(swapchain->GetBuffer(0,IID_PPV_ARGS(&back)))&&SUCCEEDED(device->CreateRenderTargetView(back.Get(),nullptr,&target));}
bool capture(const char* path){
    ComPtr<ID3D11Texture2D> back,staging;
    if(FAILED(swapchain->GetBuffer(0,IID_PPV_ARGS(&back))))return false;
    D3D11_TEXTURE2D_DESC d{};back->GetDesc(&d);d.Usage=D3D11_USAGE_STAGING;d.BindFlags=0;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;d.MiscFlags=0;
    if(FAILED(device->CreateTexture2D(&d,nullptr,&staging)))return false;
    context->CopyResource(staging.Get(),back.Get());D3D11_MAPPED_SUBRESOURCE mapped{};
    if(FAILED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped)))return false;
    BITMAPFILEHEADER header{};BITMAPINFOHEADER info{};header.bfType=0x4d42;header.bfOffBits=sizeof(header)+sizeof(info);header.bfSize=header.bfOffBits+d.Width*d.Height*4;
    info.biSize=sizeof(info);info.biWidth=d.Width;info.biHeight=-static_cast<LONG>(d.Height);info.biPlanes=1;info.biBitCount=32;
    ofstream file(path,ios::binary);file.write(reinterpret_cast<char*>(&header),sizeof(header));file.write(reinterpret_cast<char*>(&info),sizeof(info));
    for(UINT y=0;y<d.Height;++y){auto* row=static_cast<unsigned char*>(mapped.pData)+y*mapped.RowPitch;for(UINT x=0;x<d.Width;++x){char pixel[]={char(row[x*4+2]),char(row[x*4+1]),char(row[x*4]),char(255)};file.write(pixel,4);}}
    context->Unmap(staging.Get(),0);return static_cast<bool>(file);
}
int evaluate(QTable q,unsigned seed){Arena arena(seed);int wins=0;for(int e=0;e<100;++e){arena.reset();while(!arena.done)arena.tick(q,arena.botInput(),false);wins+=arena.player.hp<=0;}return wins;}
void check(bool condition,const char* name){if(!condition)throw runtime_error(name);}
bool validModelName(const string& name){
    return !name.empty()&&name.size()<=64&&all_of(name.begin(),name.end(),[](unsigned char c){
        return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_';
    });
}
vector<string> modelFiles(const filesystem::path& root){
    vector<string> files;error_code error;
    filesystem::directory_iterator it(root,error),end;
    while(!error&&it!=end){
        if(it->is_regular_file(error)&&(it->path().extension()==".qtable"||it->path().extension()==".nn"))files.push_back(it->path().generic_string());
        it.increment(error);
    }
    sort(files.begin(),files.end());return files;
}
bool saveModel(const QTable& policy,const filesystem::path& root,const string& name,string& status){
    if(!validModelName(name)){status="Use 1-64 letters, digits, - or _.";return false;}
    error_code error;filesystem::create_directories(root,error);
    if(error){status="Cannot create model folder.";return false;}
    auto path=root/(name+".qtable");
    if(filesystem::exists(path,error)||error){status="Name already exists. Choose a new name.";return false;}
    if(!policy.save(path.string())){status="Save failed.";return false;}
    status="Saved: "+name;return true;
}
int tests(){
    QTable q;array<bool,ActionCount> mask{true,true,true,false,false,false};mt19937 random(4);
    q.values[0][Charge]=100;
    for(int n=0;n<100;++n)check(q.choose(0,mask,random,true)<Charge,"cooldown mask");
    q.learn(1,Slash,10,0,mask,true,1);check(abs(q.values[1][Slash]-1.6f)<.001f,"terminal update");
    q.learn(2,Slash,0,0,mask,false,1);check(q.values[2][Slash]==0,"masked bootstrap");
    Arena arena;unsigned before=q.updates;while(!arena.done)arena.tick(q,arena.botInput(),false);
    check(q.updates==before,"evaluation freezes table");
    for(int e=0;e<30;++e){arena.reset();int frames=0;while(!arena.done){arena.tick(q,arena.botInput(),true);check(++frames<=2102,"episode termination");check(arena.player.position.x>=38&&arena.player.position.x<=882,"map boundary");}}
    check(q.updates>before,"live FSM updates Q");
    for(auto& row:q.values)for(float value:row)check(isfinite(value),"finite Q values");
    auto root=filesystem::temp_directory_path()/("containment-test-"+to_string(chrono::steady_clock::now().time_since_epoch().count()));
    string status;QTable first,second,loaded;first.values[0][Slash]=3;first.updates=10;second.values[0][Fan]=7;second.updates=20;
    check(saveModel(first,root,"boss_500",status),"save first model");
    check(saveModel(second,root,"boss_1000",status),"save second model");
    check(modelFiles(root).size()==2,"list multiple models");
    check(!saveModel(second,root,"boss_500",status),"duplicate preserves model");
    check(!saveModel(second,root,"../escape",status)&&!validModelName("")&&!validModelName("a/b"),"reject unsafe names");
    check(loaded.load((root/"boss_500.qtable").string())&&loaded.values==first.values&&loaded.updates==10,"load first snapshot");
    check(loaded.load((root/"boss_1000.qtable").string())&&loaded.values==second.values&&loaded.updates==20,"switch snapshot");
    check(!loaded.load((root/"missing.qtable").string())&&loaded.values==second.values,"failed load preserves policy");
    filesystem::remove(root/"boss_500.qtable");filesystem::remove(root/"boss_1000.qtable");filesystem::remove(root);
    cout<<"PASS: named snapshots, listing, duplicate protection, name validation, switching, failed load preservation\n";
    cout<<"PASS: action mask, terminal update, masked bootstrap, evaluation freeze, episode bounds, Q updates, finite values\n";
    return 0;
}
}

int main(int argc,char** argv){
    try{
    if(argc>1&&string(argv[1])=="--test")return tests();
    if(argc>2&&string(argv[1])=="--eval-model"){
        NeuralPolicy model;if(!model.load(argv[2]))throw runtime_error("Neural model load failed");
        int games=argc>3?clamp(stoi(argv[3]),1,1000):20;unsigned seed=argc>4?stoul(argv[4]):1000000;
        Arena arena;QTable unused;int wins=0;float total=0;
        for(int e=0;e<games;++e){
            arena.reseed(seed+e);
            while(!arena.done)arena.tick(unused,arena.botInput(),false,arena.phase==Phase::Decide?model.choose(arena):-1);
            wins+=arena.player.hp<=0;
            total+=(100-arena.player.hp)*.12f-(260-arena.boss.hp)*.1f-arena.elapsed*.008f+(arena.player.hp<=0?20.f:arena.boss.hp<=0?-20.f:0.f);
        }
        cout<<"mode="<<model.algorithm<<" games="<<games<<" wins="<<wins<<" mean_reward="<<total/games<<'\n';return 0;
    }
    if(argc>1&&string(argv[1])=="--eval"){
        QTable q;if(!q.load("policy.qtable"))throw runtime_error("Policy load failed");
        cout<<"loaded_updates="<<q.updates<<" wins_per_100="<<evaluate(q,9000)<<'\n';return 0;
    }
    if(argc>1&&string(argv[1])=="--train"){
        int count=argc>2?clamp(stoi(argv[2]),1,100000):1000;QTable q;Arena arena(42);
        int baseline=evaluate(q,9000);
        for(int e=0;e<count;++e){arena.reset();while(!arena.done)arena.tick(q,arena.botInput(),true);}
        cout<<"episodes="<<count<<" updates="<<q.updates<<" baseline_wins_per_100="<<baseline<<" learned_wins_per_100="<<evaluate(q,9000)<<'\n';
        return q.save("policy.qtable")?0:2;
    }
    bool trainerSmoke=argc>1&&string(argv[1])=="--trainer-smoke";
    bool smoke=trainerSmoke||(argc>1&&string(argv[1])=="--smoke");
    if(!smoke)FreeConsole();
    HINSTANCE instance=GetModuleHandleW(nullptr);
    WNDCLASSW wc{};wc.lpfnWndProc=WindowProc;wc.hInstance=instance;wc.lpszClassName=L"ContainmentArena";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);RegisterClassW(&wc);
    HWND window=CreateWindowW(wc.lpszClassName,L"CONTAINMENT / Reinforcement Learning Lab",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,1440,1000,nullptr,nullptr,instance,nullptr);
    DXGI_SWAP_CHAIN_DESC desc{};desc.BufferCount=2;desc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.OutputWindow=window;desc.SampleDesc.Count=1;desc.Windowed=TRUE;desc.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
    HRESULT hr=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&desc,&swapchain,&device,nullptr,&context);
    if(FAILED(hr)||!renderTarget()||!ui.initialize(window,device.Get(),context.Get()))throw runtime_error("DirectX initialization failed");
    if(!smoke)ShowWindow(window,SW_SHOW);
    Arena arena;QTable policy;bool automatic=true,learning=true,paused=false;int trainingRemaining=0;int trained=0;string status="Observe the bot, train, then take control.";
    NeuralPolicy neural;TrainingPanel training;
    if(trainerSmoke){
        string algorithm=argc>2?argv[2]:"DQN";
        if(algorithm!="DQN"&&algorithm!="PPO"&&algorithm!="SAC"&&algorithm!="QTABLE")throw runtime_error("Invalid smoke algorithm");
        training.startSmoke(algorithm=="DQN"?1:algorithm=="PPO"?2:algorithm=="SAC"?3:0);
        if(!training.running())throw runtime_error("Trainer launch failed");
    }else if(smoke&&argc>2){training.selectRun(argv[2]);if(argc>3)training.loadRequested=argv[3];}
    char modelName[65]="boss_500";string selectedModel,activeModel="Unsaved";vector<string> models;
    auto refreshModels=[&]{models=modelFiles("models");error_code error;if(filesystem::is_regular_file("policy.qtable",error))models.insert(models.begin(),"policy.qtable");};
    refreshModels();
    ImVec2 origin{18,90};float scale=1;
    auto previous=chrono::steady_clock::now();auto smokeStart=previous;double accumulator=0;int rendered=0;
    MSG message{};bool running=true;
    while(running){
        while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){if(message.message==WM_QUIT)running=false;TranslateMessage(&message);DispatchMessageW(&message);}if(!running)break;
        if(IsIconic(window)){Sleep(16);previous=chrono::steady_clock::now();continue;}
        if(resizePending){context->OMSetRenderTargets(0,nullptr,nullptr);target.Reset();if(FAILED(swapchain->ResizeBuffers(0,0,0,DXGI_FORMAT_UNKNOWN,0))||!renderTarget())throw runtime_error("Resize failed");resizePending=false;}
        ui.begin_frame();auto now=chrono::steady_clock::now();accumulator+=min(.1,chrono::duration<double>(now-previous).count());previous=now;
        auto display=ImGui::GetIO().DisplaySize;scale=min((display.x-315.f)/920.f,(display.y-400.f)/640.f);scale=max(.1f,scale);
        origin.x=max(18.f,(display.x-315.f-920*scale)*.5f);
        int oldMode=training.mode;
        float panelY=origin.y+640*scale+12;
        training.draw({18,panelY},{display.x-320,display.y-panelY-12});
        if(training.mode!=oldMode){learning=false;paused=true;trainingRemaining=0;arena.reset();accumulator=0;status="Mode selected. Train or load a matching model.";}
        auto loadPolicy=[&](const string& path){
            bool success=false;
            if(filesystem::path(path).extension()==".nn"){
                success=neural.load(path);
                if(success)training.mode=neural.algorithm=="DQN"?1:neural.algorithm=="PPO"?2:3;
            }else{success=policy.load(path);if(success)training.mode=0;}
            if(success){activeModel=filesystem::path(path).stem().string();trainingRemaining=0;trained=0;learning=false;paused=true;arena.reset();accumulator=0;status="Loaded. Unpause to observe, or fight.";}
            else status="Load failed. Current policy retained.";
        };
        if(!training.loadRequested.empty()){loadPolicy(training.loadRequested);training.loadRequested.clear();}
        bool neuralMode=training.mode!=0;
        bool policyReady=!neuralMode||(neural.ready&&neural.algorithm==(training.mode==1?"DQN":training.mode==2?"PPO":"SAC"));
        ImGui::SetNextWindowPos({display.x-285,86});ImGui::SetNextWindowSize({270,display.y-106});
        ImGui::Begin("CONTROL ROOM",nullptr,ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoCollapse);
        ImGui::TextColored({.3f,.9f,.85f,1},"CONTAINMENT / SECTOR 07");ImGui::Separator();
        if(ImGui::Checkbox("Automatic player",&automatic)){arena.reset();accumulator=0;}
        ImGui::BeginDisabled(neuralMode);
        if(ImGui::Checkbox("Live Q-table learning",&learning))arena.cancelSample();
        ImGui::EndDisabled();
        if(neuralMode)ImGui::TextWrapped("Neural training: use TRAINING LAB. This arena runs inference only.");
        ImGui::Checkbox("Pause",&paused);
        ImGui::BeginDisabled(neuralMode);
        if(ImGui::Button("Quick Q-table 500 (no curves)",{-1,32})){trainingRemaining=500;automatic=true;learning=true;paused=false;arena.reset();}
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!policyReady);
        if(ImGui::Button("Fight the trained boss",{-1,32})){automatic=false;learning=false;trainingRemaining=0;paused=false;arena.reset();}
        ImGui::EndDisabled();
        if(!policyReady)ImGui::TextColored({1,.65f,.3f,1},"Load a model for this mode first.");
        if(ImGui::Button("Restart round"))arena.reset();
        ImGui::Separator();ImGui::TextUnformatted("MODEL LIBRARY");
        ImGui::SetNextItemWidth(-1);ImGui::InputTextWithHint("##modelName","Snapshot name",modelName,sizeof(modelName));
        if(ImGui::Button("Save new model")){
            if(neuralMode){
                string path="models/"+string(modelName)+".nn";error_code error;filesystem::create_directories("models",error);
                if(!validModelName(modelName))status="Use letters, digits, - or _.";
                else if(!policyReady)status="Load a matching model first.";
                else if(error||filesystem::exists(path))status="Cannot save. Check folder or choose a new name.";
                else if(neural.save(path)){activeModel=modelName;selectedModel=path;status="Inference snapshot saved. Training checkpoints are in runs/.";refreshModels();}
                else status="Save failed.";
            }else if(saveModel(policy,"models",modelName,status)){activeModel=modelName;selectedModel="models/"+string(modelName)+".qtable";refreshModels();}
        }
        ImGui::SameLine();if(ImGui::Button("Refresh"))refreshModels();
        ImGui::SetNextItemWidth(-1);
        if(ImGui::BeginCombo("##models",selectedModel.empty()?"Select saved model":filesystem::path(selectedModel).filename().string().c_str())){
            for(const auto& file:models)if(ImGui::Selectable(file.c_str(),file==selectedModel))selectedModel=file;
            ImGui::EndCombo();
        }
        ImGui::BeginDisabled(selectedModel.empty());
        if(ImGui::Button("Load selected model",{-1,0})){
            loadPolicy(selectedModel);
        }
        ImGui::EndDisabled();ImGui::TextWrapped("Current: %s (%s)",activeModel.c_str(),training.mode==0?"Q-table":training.mode==1?"DQN":training.mode==2?"PPO":"SAC");
        ImGui::TextWrapped("%s",status.c_str());
        if(ImGui::Button("Clear current policy")){if(neuralMode){neural={};paused=true;}else policy={};arena.reset();trainingRemaining=0;activeModel="Unsaved";}
        ImGui::Separator();ImGui::Text("Episode: %u   trained: %d",arena.episode,trained);
        ImGui::Text("Model updates: %u",neuralMode?neural.updates:policy.updates);ImGui::Text("FSM: %s",phaseName(arena.phase));ImGui::TextWrapped("Action: %s",actionName(arena.action));ImGui::Text("Last Q reward: %.2f",arena.lastReward);
        if(trainingRemaining)ImGui::Text("Training left: %d",trainingRemaining);
        if(ImGui::CollapsingHeader("CURRENT ACTION VALUES / LOGITS")){
            auto values=neuralMode?neural.forward(NeuralPolicy::observe(arena)):policy.values[arena.state()];
            for(int a=0;a<ActionCount;++a){ImGui::TextDisabled("%s: %.2f%s",actionName(a),values[a],arena.legal()[a]?"":" [cooldown]");}
        }
        ImGui::Separator();ImGui::TextWrapped("WASD: move\nLeft mouse: fire at cursor\nSPACE: dodge (invulnerable)\nPillars block bullets and movement.");
        ImGui::End();
        POINT cursor{};GetCursorPos(&cursor);ScreenToClient(window,&cursor);
        Input user;user.move={float(inputManager.is_down('D')-inputManager.is_down('A')),float(inputManager.is_down('S')-inputManager.is_down('W'))};user.aim={(cursor.x-origin.x)/scale,(cursor.y-origin.y)/scale};
        user.fire=(GetAsyncKeyState(VK_LBUTTON)&0x8000)!=0&&!ImGui::GetIO().WantCaptureMouse;user.dodge=inputManager.is_down(VK_SPACE);
        if(ImGui::GetIO().WantCaptureKeyboard){user.move={};user.dodge=false;}
        if(GetForegroundWindow()!=window){user={};inputManager.release_active_input();}
        if(trainingRemaining){
            for(int steps=0;steps<3000&&trainingRemaining;++steps){arena.tick(policy,arena.botInput(),true);if(arena.done){--trainingRemaining;++trained;arena.reset();}}
            accumulator=0;
        }else{
            while(accumulator>=1.0/60){
                if(!paused&&!arena.done&&policyReady)arena.tick(policy,automatic?arena.botInput():user,learning&&!neuralMode,neuralMode&&arena.phase==Phase::Decide?neural.choose(arena):-1);
                accumulator-=1.0/60;
            }
        }
        auto* draw=ImGui::GetBackgroundDrawList();
        auto point=[&](Vec v){return ImVec2(origin.x+v.x*scale,origin.y+v.y*scale);};
        auto color=[](int r,int g,int b,int a=255){return IM_COL32(r,g,b,a);};
        auto circle=[&](Vec p,float r,ImU32 c){draw->AddCircleFilled(point(p),r*scale,c,32);};
        auto line=[&](Vec a,Vec b,ImU32 c,float width=1){draw->AddLine(point(a),point(b),c,width*scale);};
        auto box=[&](Vec a,Vec b,ImU32 c,float round=0){draw->AddRectFilled(point(a),point(b),c,round*scale);};
        draw->AddRectFilled({0,0},display,color(7,13,20));
        draw->AddText(nullptr,28,{22,17},color(230,242,245),"CONTAINMENT");draw->AddText({24,52},color(99,140,151),"07 / ADAPTIVE COMBAT TRIAL     -     Q-TABLE / DQN / PPO / DISCRETE SAC");
        box({0,0},{920,640},color(15,24,34),12);box({38,38},{882,602},color(25,39,48));
        for(float x=38;x<883;x+=40)line({x,38},{x,602},color(32,49,58));
        for(float y=38;y<603;y+=40)line({38,y},{882,y},color(32,49,58));
        draw->AddCircle(point({460,320}),210*scale,color(42,65,71),64,2);
        draw->AddCircle(point({460,320}),140*scale,color(35,54,62),64,1);
        for(int i=0;i<18;++i){float x=45+i*46.f;box({x,24},{x+21,30},color(189,127,55));box({x,610},{x+21,616},color(189,127,55));}
        box({10,270},{30,370},color(38,196,177));box({890,270},{910,370},color(217,113,52));
        draw->PushClipRect(point({38,38}),point({882,602}),true);
        if(arena.phase==Phase::Windup||arena.phase==Phase::Active){
            Vec b=arena.boss.position,u=arena.lockedAim,v{-u.y,u.x};ImU32 warning=color(255,114,47,arena.phase==Phase::Active?125:48);
            if(arena.action==Pulse||arena.action==Slash){float radius=arena.action==Pulse?170.f:90.f;circle(b,radius,warning);draw->AddCircle(point(b),radius*scale,color(255,155,68),64,2);}
            if(arena.action==Charge){ImVec2 quad[]={point(b+v*34),point(b+u*290+v*34),point(b+u*290-v*34),point(b-v*34)};draw->AddConvexPolyFilled(quad,4,warning);line(b,b+u*290,color(255,170,77),3);}
            if(arena.action==Fan)for(int i=-3;i<=3;++i){float a=atan2(u.y,u.x)+i*.17f;line(b,b+Vec{cos(a),sin(a)}*290,color(255,150,70,110),2);}
        }
        for(Vec p:arena.pillars){circle(p+Vec{5,8},39,color(7,13,19,160));box(p-Vec{32,32},p+Vec{32,32},color(58,74,83),6);box(p-Vec{23,23},p+Vec{23,23},color(33,47,57),4);line(p-Vec{20,20},p+Vec{20,-20},color(83,125,129),3);circle(p,12,color(20,32,42));circle(p,5,color(99,191,177));}
        for(auto& b:arena.bullets){line(b.position-unit(b.velocity)*16,b.position,b.enemy?color(239,135,50):color(52,231,227),3);circle(b.position,5,b.enemy?color(255,198,107):color(176,255,246));}
        Vec p=arena.player.position,b=arena.boss.position;
        circle(p+Vec{3,5},19,color(5,10,17,160));circle(p,17,color(28,100,113));circle(p,12,color(61,196,192));box(p-Vec{7,7},p+Vec{7,6},color(210,247,236),3);
        Vec aim=unit((automatic?b:user.aim)-p);line(p+aim*6,p+aim*26,color(177,255,236),6);if(arena.dodgeTime>0)draw->AddCircle(point(p),25*scale,color(174,255,242),24,3);
        circle(b+Vec{4,8},40,color(5,9,12,180));circle(b,33,color(151,69,38));circle(b,26,color(63,58,55));circle(b,15,color(239,133,56));circle(b,7,color(255,224,162));
        for(int i=0;i<4;++i){float a=i*1.570796f;Vec o{cos(a)*32,sin(a)*32};box(b+o-Vec{9,9},b+o+Vec{9,9},color(191,102,56),4);}
        draw->PopClipRect();
        box({48,49},{255,60},color(8,17,22));box({48,49},{48+207*arena.player.hp/100,60},color(72,224,206));
        box({590,49},{867,60},color(8,17,22));box({590,49},{590+277*arena.boss.hp/260,60},color(242,140,65));
        draw->AddText(point({48,66}),color(155,211,211),"SCOUT / HP");draw->AddText(point({590,66}),color(228,184,147),"WARDEN-07 / HP");
        if(arena.done){box({240,250},{680,375},color(7,13,22,240),12);draw->AddText(nullptr,26,point({275,276}),color(237,226,203),arena.boss.hp<=0?"WARDEN DEFEATED":arena.player.hp<=0?"SCOUT DOWN":"TRIAL COMPLETE");draw->AddText(point({275,325}),color(160,185,195),"Restart round to continue. Q-table retained.");}
        ui.end_frame();float clear[]={.025f,.04f,.06f,1};context->OMSetRenderTargets(1,target.GetAddressOf(),nullptr);context->ClearRenderTargetView(target.Get(),clear);ui.render_draw_data();
        bool finishSmoke=smoke&&(trainerSmoke?!training.running():rendered==59);
        if(trainerSmoke&&chrono::duration<double>(now-smokeStart).count()>60)throw runtime_error("Trainer smoke timed out");
        if(finishSmoke&&trainerSmoke&&training.exitCode!=0)throw runtime_error("Trainer process failed");
        if(finishSmoke&&!capture("build/preview.bmp"))throw runtime_error("Capture failed");
        if(FAILED(swapchain->Present(smoke?0:1,0)))throw runtime_error("Present failed");inputManager.end_frame();
        ++rendered;
        if(finishSmoke){cout<<rendered<<" native DX11 frames rendered"<<(trainerSmoke?"; Python trainer completed":"")<<'\n';running=false;}
        if(trainerSmoke)Sleep(1);
    }
    ui.shutdown();DestroyWindow(window);return 0;
    }catch(const exception& e){cerr<<e.what()<<'\n';return 1;}
}
