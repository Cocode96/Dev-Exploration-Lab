#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <string>
#include <vector>
#include "imgui.h"

namespace ArenaLab {
using namespace std;
class TrainingPanel {
public:
    int mode{};
    string loadRequested;
    ~TrainingPanel();
    void draw(ImVec2 position,ImVec2 size);
    bool running() const { return process!=nullptr; }
    void selectRun(const string& path);
private:
    HANDLE process{}, job{};
    string runPath,status="Training uses the C++ arena in a separate Python process.";
    vector<string> runs;
    int episodes{500},evalEvery{25},evalGames{20},batch{64},rollout{256},seed{42},patience{};
    float lr{.0003f},tableLr{.16f},gamma{.96f},epsilon{.3f},epsilonMin{.05f},decay{.995f},clip{.2f},lambda{.95f},entropy{.01f},alpha{.2f};
    bool resume{};
    ULONGLONG lastPoll{};
    vector<float> rewards,means,evalRewards,wins,losses,actorLosses,entropies;
    int latestEpisode{},updates{};
    float latestEpsilon{},bestWin{},latestWin{};
    void start();
    void poll();
    void refreshRuns();
};
}
