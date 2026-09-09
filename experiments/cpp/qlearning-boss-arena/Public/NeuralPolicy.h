#pragma once
#include "Arena.h"

namespace ArenaLab {
class NeuralPolicy {
public:
    static constexpr int ObservationCount=24, Hidden=64;
    using Observation=array<float,ObservationCount>;
    string algorithm;
    unsigned updates{};
    bool ready{};
    static Observation observe(const Arena& arena);
    bool load(const string& path);
    bool save(const string& path) const;
    array<float,ActionCount> forward(const Observation& input) const;
    int choose(const Arena& arena) const;
private:
    array<float,Hidden*ObservationCount> w1{};
    array<float,Hidden> b1{};
    array<float,Hidden*Hidden> w2{};
    array<float,Hidden> b2{};
    array<float,ActionCount*Hidden> w3{};
    array<float,ActionCount> b3{};
};
}
