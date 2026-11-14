#include "assets/CoreAssets.h"
#include "assets/AssetManager.h"
namespace game {
void registerCoreModels(AssetManager& assets) {
    assets.registerModel({AssetId("core:arrow"),"models/arrow.glb"});
    assets.registerModel({AssetId("core:cube"),"models/cube.glb"});
    assets.registerModel({AssetId("core:sword"),"models/sword.glb"});
    assets.registerModel({AssetId("core:grass"),"models/grass_block.glb"});
    assets.registerModel({AssetId("core:earth"),"models/earth_block.glb"});
    assets.registerModel({AssetId("core:plaster"),"models/plaster_block.glb"});
    assets.registerModel({AssetId("core:roof"),"models/roof_block.glb"});
    assets.registerModel({AssetId("core:stone"),"models/stone_block.glb"});
    assets.registerModel({AssetId("core:pine"),"models/woodland_pine.glb"});
    assets.registerModel({AssetId("core:tree"),"models/tree.glb"});
    assets.registerModel({AssetId("core:waystone"),"models/waystone.glb"});
    assets.registerModel({AssetId("core:player"),"models/player.glb"});
    assets.registerModel({AssetId("core:solid"),"models/solid.glb"});
    assets.registerModel({AssetId("core:scout"),"models/scout.glb"});
    assets.registerModel({AssetId("core:brute"),"models/brute.glb"});
    assets.registerModel({AssetId("core:iron_sword"),"models/iron_sword.glb"});
    assets.registerSound({AssetId("core:swing"),"audio/swing.wav",false});
    assets.registerSound({AssetId("core:waystone"),"audio/waystone.wav",false});
    assets.registerSound({AssetId("core:pickup"),"audio/pickup.wav",false});
    assets.registerSound({AssetId("core:hit"),"audio/hit.wav",false});
}
}
