# Conteúdo oficial por mods

A inicialização normal usa esta pasta. O jogo começa no menu e só cria entidades
de gameplay depois que um mundo é criado ou aberto.

- `foundation`: jogador, regras, inimigos, combate, inventário, equipamentos,
  loot, interface de gameplay e os modelos/sons utilizados por esse conteúdo.
- `plains`: gerador `plains:gentle`, blocos, texturas provisórias, árvores,
  ruínas, flores e regras de distribuição dos encontros.

Para jogar: `build/release/game.exe` → **Jogar** → **Criar mundo** → nome e seed.
Seed vazia recebe um valor aleatório. Mundos existentes aparecem na mesma lista.

O relevo das planícies varia de 3 a 5 blocos, em transições espaçadas; não há
cavernas. O terreno é gerado por chunks conforme o jogador explora.

As texturas substituíveis estão em `plains/textures/*.png`. Seus IDs e caminhos
ficam em `plains/assets.json`; dimensões 16×16 são apenas o placeholder inicial.
Edite a fonte, execute `tools/build.ps1 release` e reinicie o jogo. O script
`tools/generate-plains.py` reproduz os placeholders originais, portanto não o
execute depois de substituir a arte, a menos que queira restaurá-los.

Configurações de terreno e estruturas estão em `plains/world.json`. Definições
de equipamentos e regras estão em `foundation/definitions.sl`. Novos mods podem
adicionar conteúdo com o próprio namespace e dependências declaradas.

A aventura anterior foi preservada em `examples/legacy/adventure` para exemplos
e regressão; ela não participa da inicialização padrão. Saves antigos continuam
em `user/saves`, sem conversão automática para um mundo procedural.

Contrato completo e limites: [mundos procedurais](../docs/27-PROCEDURAL-WORLDS.md).

As planícies agora usam os biomas Pradaria e Bosque aberto e permitem várias
estruturas por chunk. Para criar ou estender geradores sem editar o mod original,
consulte o [guia de geração](../docs/modding/06-WORLD-GENERATION.md) e o exemplo
`examples/worldgen-addons/gardens`. Mundos antigos preservam a geração salva.
