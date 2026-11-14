#pragma once
#include "world/ChunkWorld.h"
#include "world/WorldStorage.h"
#include "scripting/Runtime.h"
#include "persistence/SaveGame.h"
namespace game {
BlockEdits readLegacyBlockEdits(const Data& state,const WorldDefinition& definition);
void bindWorldApi(script::Runtime& runtime,ChunkWorld& world,const WorldRecord& record);
void bindWorldPersistence(SaveGame& saves,ChunkWorld& world);
}
