# Partidas com mapas, conteúdo e salvamento

## Criar e abrir uma partida

Instale um conjunto de mods que forneça um mapa e crie o jogador. Abra **Jogar → Criar mundo**, informe o nome e clique em **Criar e jogar**. A mesma revisão do mapa fornece o mesmo terreno inicial a todas as partidas. Cada partida tem seu próprio progresso e aparece em **Jogar**.

Pela linha de comando, use `game.exe --mods PASTA --create-world NOME`. Use `--open-world ID` para continuar uma partida salva. Para testar um mapa direto do editor, use **Testar no jogo** ou `game.exe --mods PASTA --test-map ARQUIVO --user-dir PASTA_DE_TESTE`.

## Publicar o mapa

No editor, crie um mapa, coloque pelo menos um bloco sólido no ponto inicial e use **Projeto → Exportar mapa e mods**. A exportação contém os dados do terreno e os mods da revisão. Cada mapa usa os blocos, modelos e texturas do catálogo declarado pelos mods.

Um mod pode fornecer o mapa inicial no campo `maps` de `world.json`. Outros mods podem acrescentar colocações de estruturas por `map_placements` ou substituir o mapa por `map_overrides`, declarando as dependências correspondentes. O formato e os controles estão no [guia de mundo e mods](editor/01-MUNDO-E-MODS.md) e no [guia do editor](editor/02-EDITOR-E-BLOCKBENCH.md).

## Persistência

Cada partida copia o mapa inicial para seus dados. Durante o jogo, as alterações de terreno são registradas em regiões; o jogo carrega os chunks próximos do jogador. Instalar uma revisão nova do mapa afeta as novas partidas. Partidas existentes mantêm o terreno salvo.

Mundos existentes com mapa continuam disponíveis se seus mods forem encontrados. Para recuperar terreno de um mundo antigo que não tenha mapa próprio, abra o editor, use **Projeto → Importar região de um save** e selecione o `checkpoint.json`. A importação cria um mapa do recorte escolhido com as alterações salvas de terreno.

[Controles, configurações e perfis](PLAYING.md) · [API de terreno](modding/06-WORLD-GENERATION.md)
