#include "assets/CoreAssets.h"
#include "assets/AssetManager.h"
namespace game {
void registerCoreModels(AssetManager& assets) {
    assets.registerModel({AssetId("core:cube"),"models/cube.glb"});
    assets.registerModel({AssetId("core:sword"),"models/sword.glb"});
    assets.registerModel({AssetId("core:tree"),"models/tree.glb"});
    assets.registerModel({AssetId("core:waystone"),"models/waystone.glb"});
}
}
