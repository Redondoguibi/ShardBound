# ShardBound — primeiro ciclo jogável

Na raiz do repositório:

```powershell
./tools/build.ps1 debug
./build/debug/game.exe
```

O executável abre `content/` por padrão. `--mods` continua selecionando outro
conjunto de pacotes, como os sandboxes existentes. Caminhos desse argumento são
resolvidos a partir da pasta do executável; caminhos absolutos também funcionam.

| Entrada | Ação |
| --- | --- |
| WASD | Movimento relativo à câmera |
| Espaço | Salto, apenas quando apoiado |
| Mouse esquerdo | Ataque na direção do cursor; direção travada durante o golpe |
| M | Mapa; Q seleciona destino, E viaja; 1/2/3 são atalhos |
| I | Inventário; WASD/Q selecionam, E equipa; clique seleciona para comparar |
| Q | Fora dos menus, alterna entre instâncias de armas |
| F5 / F9 | Salvar / carregar o slot `adventure` |
| Escape | Fechar painel / abrir ou fechar pausa |
| W / S, E ou clique | Selecionar e confirmar nos menus |

Saia do abrigo pela trilha para explorar a floresta. A aproximação das waystones
as ativa. Derrotar um inimigo concede ouro e deixa sua arma no chão.
Aproxime-se para coletar; inventário cheio mantém o drop no mundo. Há inimigos
comuns, um explorador leve e um adversário blindado, com ataques diferentes;
o inimigo reaparece após 25 segundos. A preparação do ataque inimigo muda sua
cor para amarelo. Ataques recebidos ou causados impedem viagem por seis segundos.
No abrigo, a vida se recupera. Na morte, perde-se 15% do ouro, arredondado para
baixo, e retorna-se ao abrigo com vida cheia e os equipamentos preservados.

O save fica em `build/<configuração>/user/saves/adventure.json`. F5 substitui
esse slot mantendo `.bak`; não há autosave nem carregamento automático.
O menu inicial oferece Explorar, Carregar jogo e Sair. Explorar entra no mundo
novo; Carregar jogo restaura o slot existente. A pausa também permite salvar,
carregar e ajustar o volume da sessão. Não há autosave ao sair.

## Organização

- `definitions.sl`: definições públicas de entidades, itens e componente de IA.
- `equipment.sl`: identidade, atributos por cópia, seleção e reconciliação de contagens.
- `regions.sl`: definições públicas de regiões/waystones, segurança e descoberta.
- `combat_visuals.sl`: preparação, empunhadura, alcance e impacto.
- `world.sl`: composição do abrigo/floresta e posicionamento de conteúdo.
- `exploration.sl`: descoberta, viagem, combate recente e morte.
- `combat.sl`: ataques, IA, recompensas e poses rígidas.
- `loot.sl`: itens persistentes no chão e coleta por proximidade.
- `interface.sl`: input e seleção de equipamento.
- `ui_canvas.sl`: HUD, mapa, menus e feedback de dano.
- `ui_inventory.sl`: grade, filtros, comparação e ações sobre cópias de itens.
- `scenery.sl`: decoração oficial, materiais e iluminação.
- `persistence.sl`: schema de estado, validação e restauração de referências.
- `main.sl`: montagem e eventos do pacote.

O exemplo em `examples/adventure-extension/trailcraft` demonstra `modify`,
hooks de dano/atributos, uma terceira arma e uma região/waystone ao norte. Para experimentá-lo, copie a pasta `trailcraft` para
`build/debug/content/` depois do build e inicie o jogo. Remova essa cópia para
desativá-lo. Seus arquivos-fonte e o pacote oficial ficam separados.

## Limites atuais

É um protótipo jogável, não a Etapa 13 completa. O conteúdo oficial tem três
waystones, hub/floresta pequenos, cinco inimigos de três tipos, quatro definições
de arma e um slot equipado. Regiões/destinos/armas são extensíveis por mods.
Cada cópia possui UID, nível, raridade e atributos persistidos; não há sorteio
de atributos, armaduras, catálogo amplo, streaming ou campanha. O limite
atual é 256 instâncias e 63 waystones no catálogo. Saves antigos são migrados.
A apresentação usa materiais em pixel art, volumes em cubos/cuboids, casas
simples, árvores originais do autor, iluminação, sombras, HUD e menus. A direção visual
e as poses procedurais ainda precisam de refinamento e avaliação jogando.
O importador continua sem skinning/clips glTF; a pose rígida opera sobre partes
nomeadas. Ainda não há partículas completas ou medição de desempenho
representativa de muitos mods. Contratos: `docs/23-PLAYABLE-SLICE.md`, `docs/24-EQUIPMENT-AND-WORLD-CONTENT.md`
e `docs/25-COMBATANTS-LOOT-AND-RUNTIME-CACHES.md`. A revisão atual está em
`docs/26-PLAYABLE-QUALITY-PASS.md`.

Para gerar capturas verificáveis de título, abrigo, floresta, combate, pausa,
mapa e inventário, execute `presentation_tests.exe --preview` dentro de
`build/release/`. As imagens PPM ficam em `user/previews/`.

## Inventário

A grade mostra 12 entradas por página. Clique em um item para comparar com a
arma em mãos; clique em Equipar ou pressione E para confirmar. WASD navega pela
grade, Q avança uma entrada; Anterior/Próxima mudam a página. Os filtros Todos,
Armas e Outros itens não alteram o conteúdo salvo. Cópias de armas continuam
independentes, identificadas pelo UID mostrado no painel. Dano, alcance e
intervalo total do golpe incluem os atributos da cópia. Valores verdes indicam
melhoria; para o intervalo, um valor menor significa um ataque mais rápido.
Itens de mods ausentes conservam identidade e atributos, mas não podem ser
equipados. Itens empilhados mostram sua quantidade. Não foram adicionados slots
de armadura, venda ou descarte; continuam valendo os contratos de equipamento.

## Prioridade atual (2026-10-09)

Corrigir funcionamento, estabilidade, colisões e legibilidade preservando a
identidade em cubos. A correção v2 mantém áreas, encontros, itens e destinos;
usa grade regular no piso/caminho e volumes alinhados, com menos detalhes finos.
Saves da apresentação v1 recebem essa correção ao carregar. Casas têm colisão
compatível com o volume visível; seus interiores não fazem parte do espaço
jogável. Abrir mapa/inventário/pausa cancela ataques aguardando no buffer.
O personagem fica sob responsabilidade do autor.
