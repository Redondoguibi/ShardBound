# Galeria de assets — primeiro marco da Etapa 13

Após `./tools/build.ps1 debug`, execute a partir da raiz do repositório:

```powershell
./build/debug/game.exe --mods examples/asset-gallery
```

A janela exibe espada, árvore e waystone com câmera fixa. Escape fecha a janela.
A cena é definida por `gallery/main.sl`, usando a API pública de entidades e
os modelos `core:sword`, `core:tree` e `core:waystone`. Não há spawn desses
objetos codificado na Application. Os modelos são carregados sob demanda.

As escalas individuais são de apresentação; a escala definitiva de jogo e o
rig ainda serão definidos. Esta galeria verifica assets/materiais, não contém
combate, personagem controlável, hub ou viagem rápida.

O importador aceita GLB/GLTF estático autocontido com `OPAQUE` ou `MASK`.
`alphaCutoff` e `doubleSided` são preservados no cache e no renderer. O recorte
considera alfa da textura multiplicado pelo alfa de `baseColorFactor`.
Pixels descartados não escrevem cor nem profundidade. Faces duplas recebem
iluminação com a normal invertida no verso. `BLEND` continua não suportado e
gera um diagnóstico explícito.

Fontes da waystone: `assets/core/models/waystone.bbmodel` e `waystone.png`.
Para regenerar seu GLB: `python tools/export-waystone.py`.
