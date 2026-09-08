#pragma once
#include "real_dx_api.h"
#include <vector>
namespace Lab
{
using namespace std;
class BenchmarkRunner final
{
  public:
    void start(ExperimentControls& controls);
    bool prepare(ExperimentControls& controls);
    void record(ExperimentControls& controls, double cpu_ms);
    void finish(ExperimentControls& controls, const char* message);

  private:
    struct Phase
    {
        int layout{}, backend{}, count{}, repeat{};
    };
    vector<Phase> m_phases;
    vector<double> m_cpu, m_gpu;
    vector<SwitchMetrics> m_switch;
    BenchmarkOptions m_saved{};
    unsigned m_index{}, m_frame{};
    bool m_new_phase{};
    void save(const ExperimentControls& controls);
};
} // namespace Lab
