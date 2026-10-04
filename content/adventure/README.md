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
| Mouse esquerdo | Ataque na direção do último movimento |
| M | Mapa; 1/2/3 viajam para as waystones descobertas |
| I | Inventário resumido |
| Q | Alternar entre as duas armas disponíveis |
| F5 / F9 | Salvar / carregar o slot `adventure` |
| Escape | Sair |

Saia do abrigo pela trilha para explorar a floresta. A aproximação das waystones
as ativa. Derrotar um inimigo concede ouro e a primeira espada da floresta;
o inimigo reaparece após 25 segundos. A preparação do ataque inimigo muda sua
cor para amarelo. Ataques recebidos ou causados impedem viagem por seis segundos.
No abrigo, a vida se recupera. Na morte, perde-se 15% do ouro, arredondado para
baixo, e retorna-se ao abrigo com vida cheia e os equipamentos preservados.

O save fica em `build/<configuração>/user/saves/adventure.json`. F5 substitui
esse slot mantendo `.bak`; não há autosave nem carregamento automático.
Abrir o executável inicia um mundo novo; use F9 para restaurar o save.

## Organização

- `definitions.sl`: definições públicas de entidades, itens e componente de IA.
- `world.sl`: composição do abrigo/floresta e posicionamento de conteúdo.
- `exploration.sl`: descoberta, viagem, combate recente e morte.
- `combat.sl`: ataques, IA, recompensas e poses rígidas.
- `interface.sl`: input, HUD, mapa e seleção de equipamento.
- `persistence.sl`: schema de estado, validação e restauração de referências.
- `main.sl`: montagem e eventos do pacote.

O exemplo em `examples/adventure-extension/trailcraft` demonstra `modify` e
hook de dano. Para experimentá-lo, copie a pasta `trailcraft` para
`build/debug/content/` depois do build e inicie o jogo. Remova essa cópia para
desativá-lo. Seus arquivos-fonte e o pacote oficial ficam separados.

## Limites atuais

É um protótipo jogável, não a Etapa 13 completa. Tem três waystones fixas, uma
região pequena, três inimigos do mesmo tipo, duas definições de espada e um
slot equipado. Níveis e raridades vêm das definições; ainda não há atributos
aleatórios por instância, armaduras, catálogo amplo, streaming ou campanha.
Os modelos/cenários, poses procedurais e painel de texto são provisórios.
O importador continua sem skinning/clips glTF; a pose rígida opera sobre partes
nomeadas. Ainda não há sombras, partículas completas ou medição de desempenho
representativa de muitos mods. Contratos: `docs/23-PLAYABLE-SLICE.md`.
