#pragma once
#include "Lab_Struct.h"
#include <vector>
namespace Lab
{
using namespace std;
class HumanoidModel final
{
  public:
    void append(vector<SceneVertex>& vertices, const XMMATRIX& view_projection, float time) const;
};
} // namespace Lab
