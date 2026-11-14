"""Original temporary 16x16 block textures and mod-authored voxel structures.
Run again to reproduce the initial placeholders; gameplay reads the exported PNG/JSON.
"""
from pathlib import Path
import json, random, struct, zlib

ROOT = Path(__file__).resolve().parents[1]

def png(path, base, seed):
    rng = random.Random(seed)
    pixels = bytearray()
    for y in range(16):
        pixels.append(0)
        for x in range(16):
            shade = rng.choice([-12, -6, 0, 0, 4, 8])
            pixels.extend(max(0, min(255, c + shade)) for c in base)
            pixels.append(255)
    def chunk(tag, data):
        return struct.pack('>I', len(data)) + tag + data + struct.pack('>I', zlib.crc32(tag + data) & 0xffffffff)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB',16,16,8,6,0,0,0)) + chunk(b'IDAT', zlib.compress(bytes(pixels))) + chunk(b'IEND',b''))

def main():
    root = ROOT / 'content/plains'
    colors = {'grass': (112,145,75), 'dirt': (119,89,63), 'stone': (118,124,119),
              'wood': (106,79,51), 'leaves': (70,112,62), 'flower': (189,162,80), 'brick': (143,135,113)}
    assets = {'textures': {}}
    blocks = {}
    for i, (name, color) in enumerate(colors.items()):
        png(root / f'textures/{name}.png', color, 104+i)
        assets['textures'][f'plains:{name}'] = {'source': f'textures/{name}.png'}
        blocks[f'plains:{name}'] = {'texture': f'plains:{name}', 'solid': name not in ('leaves', 'flower')}
    tree = [[0,y,0,'plains:wood'] for y in range(4)]
    tree += [[x,y,z,'plains:leaves'] for y in (3,4,5) for x in range(-2,3) for z in range(-2,3)
             if abs(x)+abs(z) <= (2 if y==5 else 3) and (x,y,z)!=(0,3,0)]
    ruin = []
    for x in range(-2,3):
        for z in range(-2,3):
            ruin.append([x,0,z,'plains:brick'])
            if (abs(x)==2 or z==2) and not (z==-2 and x==0):
                for y in range(1,2+(x+z)%2):
                    ruin.append([x,y,z,'plains:brick'])
    flowers = [[x,0,z,'plains:flower'] for x,z in ((0,0),(1,1),(-1,1),(2,-1))]
    world = {'version': 1, 'blocks': blocks,
             'structures': {'plains:oak':tree, 'plains:ruin':ruin, 'plains:flowers':flowers},
             'biomes': {
                 'plains:meadow': {'name':'Pradaria', 'rock':'plains:stone', 'layers':[{'block':'plains:grass','depth':1},{'block':'plains:dirt','depth':2}]},
                 'plains:grove': {'name':'Bosque aberto', 'rock':'plains:stone', 'layers':[{'block':'plains:grass','depth':1},{'block':'plains:dirt','depth':3}]}},
             'generators': {'plains:gentle': {'name':'Planicies suaves', 'surface':'plains:grass', 'fill':'plains:dirt',
                 'rock':'plains:stone', 'base_height':4, 'relief':1, 'period':64,
                 'structure_mode':'scatter', 'safe_radius':0, 'biome_period':192,
                 'biomes':[{'biome':'plains:meadow','weight':0.65},{'biome':'plains:grove','weight':0.35}],
                 'structures':[
                     {'id':'plains:ruins','structure':'plains:ruin','chance':0.06,'max_slope':0,'spacing':6},
                     {'id':'plains:trees','structure':'plains:oak','chance':0.22,'attempts':3,'biomes':['plains:grove'],'max_slope':0,'spacing':5},
                     {'id':'plains:flowers','structure':'plains:flowers','chance':0.18,'attempts':4,'biomes':['plains:meadow'],'alignment':'surface','spacing':3}]}}}
    for path, data in ((root/'assets.json',assets),(root/'world.json',world)):
        path.write_text(json.dumps(data, indent=2)+'\n', encoding='utf-8')

if __name__ == '__main__':
    main()
