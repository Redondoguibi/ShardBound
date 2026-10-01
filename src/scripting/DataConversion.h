#pragma once
#include "core/Data.h"
#include "scripting/Value.h"
namespace game::script {
Data toData(const Value& value);
Value fromData(const Data& value);
}
