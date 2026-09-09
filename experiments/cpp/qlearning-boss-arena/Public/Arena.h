#pragma once
#include <array>
#include <vector>
#include <random>
#include <string>
#include "GameObject.h"

namespace ArenaLab {
using namespace std;
struct Vec { float x{}, y{}; Vec operator+(Vec b) const {return {x+b.x,y+b.y};} Vec operator-(Vec b) const {return {x-b.x,y-b.y};} Vec operator*(float s) const {return {x*s,y*s};} };
float length(Vec v);
Vec unit(Vec v);
struct Actor : Engine::GameObject { Vec position; float hp{}, radius{}; };
struct Bullet { Vec position, velocity; float life; bool enemy; };
struct Input { Vec move, aim; bool fire{}, dodge{}; };
enum Action { Approach, Orbit, Slash, Charge, Pulse, Fan, ActionCount };
enum class Phase { Decide, Windup, Active, Recovery, Dead };

class QTable {
public:
    using Row = array<float,ActionCount>;
    array<Row,96> values{};
    unsigned updates{};
    float learningRate{.16f}, epsilon{.18f}, gamma{.96f};
    int choose(int state, const array<bool,ActionCount>& legal, mt19937& rng, bool explore) const;
    void learn(int state,int action,float reward,int next,const array<bool,ActionCount>& legal,bool done,float seconds);
    bool save(const string& path) const;
    bool load(const string& path);
};

class Arena {
public:
    Actor player, boss;
    vector<Bullet> bullets;
    const array<Vec,4> pillars{{{260,185},{660,185},{260,455},{660,455}}};
    Phase phase{Phase::Decide};
    int action{Approach};
    array<float,3> cooldown{};
    Vec lockedAim{1,0};
    float phaseTime{}, elapsed{}, dodgeTime{}, dodgeCooldown{}, shotCooldown{};
    bool done{};
    float lastReward{};
    unsigned episode{};
    explicit Arena(unsigned seed=17);
    void reset();
    void tick(QTable& q, const Input& input, bool learning, int forcedAction=-1);
    void reseed(unsigned seed) { rng.seed(seed); reset(); }
    Input botInput();
    int state() const;
    array<bool,ActionCount> legal() const;
    void cancelSample() { pending=false; }
private:
    mt19937 rng;
    float timer{}, decisionSeconds{}, reward{}, botClock{};
    int decisionState{};
    bool pending{}, hit{};
    Vec dodgeDirection;
    void move(Actor& actor,Vec displacement);
    void finish(QTable& q,bool terminal);
};
const char* actionName(int action);
const char* phaseName(Phase phase);
}
