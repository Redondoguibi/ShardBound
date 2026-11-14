# ShardBound

A linguagem `.sl` oferece `async func`/`await`, esperas em tempo de jogo e cancelamento, com tarefas cooperativas limitadas por atualização. [Sintaxe e limites](docs/modding/13-ASYNC-AWAIT.md) · [Exemplo de mod](examples/async-tasks/README.md).

Mods podem estender ou substituir fases da engine por scripts e DLLs: movimento/física, atualização, streaming e desenho da cena. Há configuração de gravidade, inclinação, iluminação e ordem das etapas fixas, com versão de API, autoria e conflitos explícitos. [Guia da API](docs/modding/12-ENGINE-EXTENSIONS.md) · [Implementação e validação](docs/38-ENGINE-EXTENSIONS.md).

**Configurações → Diagnóstico de desempenho** ativa o painel de CPU/GPU e a exportação de relatórios. O menu também oferece controles personalizáveis, sensibilidade, escala de interface e canais de áudio. **Mods e perfis** permite escolher conjuntos por mundo, com backup antes de alterar a seleção de um mundo existente. [Como usar](docs/PLAYING.md) · [Implementação e validação](docs/36-PLAYER-READINESS.md).

Para gerar um ZIP com dependências de execução e verificações: `./tools/package.ps1`. Instalações novas usam os dados locais do usuário; pastas `user` existentes continuam sendo utilizadas, e `--portable` mantém os dados ao lado do executável.

Novas partidas usam o **mapa finito do ShardBound**, com dois biomas conectados
e carregamento por chunks. Abra `build/release/game.exe`, clique em **Jogar** e
dê um nome à partida. Saves procedurais existentes continuam disponíveis.
Configurações estão na tela inicial e na pausa.

Abra `build/release/shardbound_editor.exe` para editar o mapa por camadas,
importar recortes de saves ou abrir uma partida de teste.
[Guia do editor](docs/editor/02-EDITOR-E-BLOCKBENCH.md).

Todo conteúdo do modo padrão está nos mods `content/foundation` (gameplay,
modelos, sons e equipamentos) e `content/plains` (blocos, texturas e geração).
[Guia do conteúdo](content/README.md) · [Contrato dos mundos procedurais legados](docs/27-PROCEDURAL-WORLDS.md).

Para trocar os placeholders, edite `content/plains/textures/*.png`, compile
novamente e reinicie. A aventura fixa anterior foi preservada em
`examples/legacy/adventure`, fora da inicialização padrão.

Projeto experimental de um jogo 3D com estética pixel-art, construído sem uma engine de jogos tradicional e pensado desde o início para ser extremamente modificável.

A ideia central não é começar produzindo um jogo cheio de conteúdo. Primeiro será criado o **núcleo/runtime** do jogo, junto de um **sistema de mods** e uma **linguagem própria** capaz de descrever e estender conteúdo e comportamento.

## Conceito

O jogo base será pequeno e funcionará como a fundação do ecossistema — conceitualmente próximo de um hub.

O restante poderá ser adicionado por mods:

- áreas e mapas;
- inimigos;
- bosses;
- equipamentos;
- habilidades;
- NPCs;
- sistemas;
- alterações em sistemas existentes;
- novos tipos de conteúdo;
- extensões profundas do jogo.

Os mods serão híbridos. A linguagem própria será o principal meio de criação, mas um mod também poderá conter C++, JSON, modelos 3D, imagens, áudio e outros recursos.

## Workflow desejado

```text
Criar mod
   ↓
Adicionar scripts + dados + assets
   ↓
Colocar o mod no jogo
   ↓
Inicialização do jogo
   ↓
Resolver mods e dependências
   ↓
Compilar/carregar conteúdo
   ↓
Integrar ao runtime
   ↓
Jogar
```

Remover um mod e reiniciar o jogo remove o conteúdo fornecido por ele, respeitando as regras futuras de save e compatibilidade.

## Princípios

1. **Modding é arquitetura central**, não recurso posterior.
2. **C++ no núcleo**, com bibliotecas externas permitidas.
3. **Sem engine pronta**, mas bibliotecas especializadas são permitidas.
4. **Linguagem própria** equilibrada entre legibilidade humana e geração por IA.
5. **Mods híbridos**, não limitados à linguagem própria.
6. **Extensibilidade profunda**: sistemas existentes poderão ser estendidos e, em casos avançados, substituídos.
7. **Carregamento no startup** inicialmente; hot reload não é prioridade.
8. **IA-first**: a estrutura deve favorecer geração, revisão e manutenção de mods por agentes de IA.

## Documentação

Em um mundo aberto, **Page Up + Page Down** alterna o modo de desenvolvedor; **/** abre o console. Use `/help` para consultar os comandos, `/camera thirdperson` para a câmera orbital e `/camera free` para a câmera livre. Veja [comandos e atributos](docs/modding/11-DEVELOPER-CONSOLE-AND-ATTRIBUTES.md).

Veja [docs/README.md](docs/README.md).

Para criar mods, comece pelo [Guia oficial de modding](docs/modding/README.md).

> Estado atual: implementação iniciada. Veja [build e progresso](docs/18-IMPLEMENTATION-STATUS.md).

Primeiro ciclo legado da Etapa 13: [abrigo, floresta e combate](examples/legacy/adventure/README.md).
Após `./tools/build.ps1 debug`, a aventura legada é aberta com
`./build/debug/game.exe --mods examples/legacy`.
Inclui WASD/salto, mapa/waystones, quatro armas, três tipos de inimigo, loot
persistente no chão, morte e save/load. A revisão atual adiciona iluminação
e sombras, abrigo detalhado, materiais em pixel art, mira pelo cursor, câmera
suave, menu inicial, pausa, inventário e mapa clicáveis.
Veja [a revisão de apresentação](docs/26-PLAYABLE-QUALITY-PASS.md).
A [galeria de assets](examples/asset-gallery/README.md) permanece disponível com
`--mods examples/asset-gallery`. A Etapa 13 completa continua em desenvolvimento.


## Jogando

Para jogar o jogo, rode o comando no terminal:
```
.\build\release\game.exe
```
## Conteúdo do ShardBound Studio

O jogo importa os pacotes v1 exportados pelo plugin no Blockbench 5.2.2. [Como importar e usar modelos, animações, equipamentos, VFX e áudio](docs/shardbound-studio/05-RUNTIME-E-USO.md). Demonstração: `build/release/game.exe --mods examples/studio`.

## Shaders de mods

Mods podem adicionar efeitos HLSL de superfície e pós-processamento, com parâmetros por script e vínculos em entidades/blocos. [Guia](docs/modding/09-SHADERS.md) · [Exemplo executável](examples/shaders/README.md) · [Validação Debug/Release](docs/31-MOD-SHADERS.md).

## Compatibilidade e recuperação de mundos

A seleção de mundos oferece backup, restauração e limpeza segura. Mods podem declarar versões de saves compatíveis e migrações isoladas de schema. [Guia e limites](docs/modding/10-SAVES-COMPATIBILITY-AND-RECOVERY.md).
