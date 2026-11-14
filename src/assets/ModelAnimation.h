#pragma once
#include "core/Math.h"
#include <map>
#include <string>
#include <vector>
namespace game {
struct ModelNode { std::string name, uuid, role; int parent=-1; Matrix local; };
struct ModelTrack { size_t node=0; int path=1; bool step=false; std::vector<float> times; std::vector<std::array<float,4>> values; };
struct ModelClip { std::string name; float length=0; std::vector<ModelTrack> tracks; };
struct ModelHierarchy { std::vector<ModelNode> nodes; std::vector<ModelClip> clips; };
Matrix composeTRS(Vec3 position,std::array<float,4> quaternion,Vec3 scale);
Vec3 transformPoint(Vec3 point,const Matrix& matrix);
Matrix inverse(const Matrix& matrix);
std::array<float,4> sampleTrack(const ModelTrack& track,float time);
void validateHierarchy(const ModelHierarchy& hierarchy);
std::vector<Matrix> sampleModel(const ModelHierarchy& model,const std::string& clip,float time,
    const std::map<size_t,Matrix>& localOverrides={},const std::map<size_t,Matrix>& worldOverrides={});
}
