#include "assets/CoreAssets.h"
#include "assets/AssetManager.h"
namespace game {
void registerCoreModels(AssetManager& assets) {
    assets.registerModel({AssetId("core:cube"),"models/cube.glb"});
    assets.registerModel({AssetId("core:sword"),"models/sword.glb"});
    assets.registerModel({AssetId("core:tree"),"models/tree.glb"});
    assets.registerModel({AssetId("core:waystone"),"models/waystone.glb"});
    assets.registerModel({AssetId("core:player"),"models/player.glb"});
    assets.registerModel({AssetId("core:solid"),"models/solid.glb"});
    assets.registerSound({AssetId("core:hit"),"audio/hit.wav",false});
}
}
