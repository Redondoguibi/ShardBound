#pragma once
#include "assets/StudioPackage.h"
#include "core/Math.h"
#include <vector>
namespace game::studio {
using Json=nlohmann::json;
struct Interval { const Json* event; const Json* definition; double start,end; bool continuous; };
struct Particle { unsigned index,frame; double birth; Vec3 position; float size,opacity,rotation; Color color; };
Vec3 vec(const Json& value);
Color tint(const std::string& hex);
float curve(const Json& effect,const std::string& key,double age,float start,float end);
Color colorCurve(const Json& effect,double age);
std::vector<Interval> intervals(const StudioPackage& package,const Json& clip);
std::vector<Particle> particles(const Interval& interval,double time);
std::vector<double> trailTimes(const Interval& interval,double time);
}
