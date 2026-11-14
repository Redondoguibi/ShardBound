# Plataforma de mods: fundamentos e extensões (2026-10-17)

Este documento registra os contratos implementados e seus limites. Os contratos de
`21-EXTENSIBILITY-V1.md` continuam válidos. O guia de uso das APIs novas está em
[modding/07-MOD-PLATFORM.md](modding/07-MOD-PLATFORM.md).

## 1. Sistemas de gameplay extensíveis

`@Events`, hooks, componentes versionados, migrações e registros de definições
continuam disponíveis. Sistemas nomeados mantêm suas assinaturas anteriores:

- `register_system(id, phase, callback, priority?)`: namespace do mod; fases `update`
  e `fixed`; prioridade inteira entre -100 e 100; 64 sistemas/mod e 256/sessão.
- `system_enabled`, `replace_system`, `unregister_system`, `list_systems` e
  `mod_profile` permanecem compatíveis.
- `configure_system(id, options)` acrescenta dependências `after`, `interval` e
  `disable_on_error`. Valida ciclos antes de modificar o registro. Intervalos usam
  tempo de simulação, acumulam o delta e não executam rajadas para compensar atrasos.

Sistemas executam depois de `@Events`. Dependências têm precedência sobre prioridade;
entre sistemas prontos, prioridade e ID determinam a ordem. A lista é fotografada
antes dos callbacks: mudanças passam a valer no próximo despacho. Dependências
inativas ou fora de seu intervalo não bloqueiam outros sistemas. Remover uma
dependência remove a barreira até ela ser registrada novamente.

Cada callback de um despacho externo recebe seu orçamento de instruções. Eventos
recursivos continuam compartilhando o orçamento e os limites de recursão. Um mod
que esgota seu orçamento não impede a execução dos seguintes. O profiler mede
callbacks e erros; não atribui tempo de GPU, física ou geração aos mods.

## 2. Geração e edição de mundos

Biomas, solos, ruídos de altura, estruturas, herança de geradores e extensões seguem
[modding/06-WORLD-GENERATION.md](modding/06-WORLD-GENERATION.md).

Geradores agora aceitam até oito `voxel_rules`: regras ordenadas de substituição
por ruído 3D, com filtros de bloco, bioma, altura absoluta e distância da superfície.
Podem adicionar depósitos, preencher volumes e remover blocos. São opcionais: as
planícies oficiais continuam com pouco relevo e **sem cavernas**. Regras entram no
snapshot do mundo; carregar um save antigo não aplica regras novas retroativamente.
`generator_extensions` também pode acrescentar regras volumétricas.

`terrain_edit` e `terrain_stamp` validam o lote inteiro e preparam meshes e colisores
antes de substituir o terreno ativo. Cada chunk afetado é reconstruído uma vez;
faces vizinhas são atualizadas. Falha na validação ou na preparação conserva o lote
anterior. `terrain_undo`/`terrain_redo` mantêm 16 transações de histórico na sessão.
O histórico é descartado ao carregar um save; os blocos finais continuam em `core.blocks`.

`terrain_surface` consulta o topo sólido editado. `terrain_height` continua retornando
a altura original do gerador, preservando seu contrato. Colisões agrupam colunas de
mesma ocupação e respeitam vazios, estruturas e blocos não sólidos. Consultas mantêm
um cache limitado de decorações para não regenerar estruturas por voxel.

Limites: Y de 0 a 47 e 4.096 entradas por lote. As edições persistentes agora são
paginadas por chunk, sem o teto global anterior; consulte o guia 08. O
preparo transacional requer espaço temporário para os colisores antigos e novos.
A geração de chunks visuais distantes executa em segundo plano, com filas e
publicação limitadas. Não há simulação hidrodinâmica ou iluminação volumétrica. Um
bloco não sólido gerado num volume não adquire comportamento de líquido automaticamente.

## 3. IA e comportamentos de entidades

`entities_near` mantém a consulta 3D de entidades ativas, ordenada por distância/ID,
com até 2.048 resultados. Os novos recursos incluem:

- `terrain_path`: A* determinístico sobre voxels, com altura livre, subida, queda,
  limite de expansão e estados de resultado explícitos. Considera edições e volumes.
- `behavior_tick`: árvores com ações, sequências, seletores, inversão e modo reativo;
  memória retornada como dados serializáveis.
- `state_tick`: estados com callbacks de entrada, execução e saída, úteis também
  para fases de chefes. A seleção das fases pertence ao mod.
- `physics_raycast`: segmento contínuo, normal, posição e lista de entidades ignoradas.
  Inclui terreno procedural fora dos chunks carregados, sem forçar streaming.
- `launch_projectile`/`stop_projectile`: movimento com gravidade, colisão varrida,
  duração, origem persistente e eventos de impacto/expiração. Não impõem regras de dano.

Projéteis são entidades visuais não físicas, com `core:projectile` v1 persistente.
A integração usa subpassos de no máximo 1/120 s; impacto/expiração remove o componente,
notifica o mod e destrói a entidade. Não são balística de corpos com volume: a
colisão é um raio. Navegação é para agentes de uma célula de largura, não navmesh,
e pode considerar corpos móveis com a opção `avoid_entities`. Mods devem recalcular caminhos
quando o ambiente mudar. Árvores e estados não movem atores automaticamente.

## 4. Equipamentos avançados

Os slots antigos continuam em `core:equipment` v1. As APIs novas acrescentam
`core:item_instances` v1, com IDs monotônicos por proprietário, durabilidade, bônus,
afixos e slots. Transformar uma cópia em instância não duplica seu inventário:
`take_item` respeita tanto as reservas de slots antigos quanto todas as instâncias.

- Criação, consulta, enumeração e liberação de instâncias.
- Equipar/desequipar com veto por hook e eventos posteriores à gravação.
- Durabilidade e reparo; instâncias quebradas deixam de contribuir bônus.
- Afixos sorteados por seed, pesos e faixas, sem reposição.
- Atributos calculados por `(base + soma_aditiva) * produto_multiplicativo`.
- Visuais anexados a nós do modelo com `attach_socket`, acompanhando pose e hierarquia.

`equipped_item` e `equipment_slots` também enxergam instâncias. Um slot não pode
conter simultaneamente uma cópia antiga e uma instância. Os saves validam reservas,
slots e dados antes da troca do mundo ativo; saves antigos válidos não exigem migração.
Hooks de equipar não podem deixar uma referência a um item consumido durante o hook.

O mod `foundation` aplica `damage` e `armor` ao combate, desgasta a arma única a cada
acerto com dano e impede dano de arma quebrada. Também sincroniza seu visual e
retoma armas únicas ao carregar. Outros atributos e regras pertencem aos mods.

Limites: 256 instâncias por entidade, 32 slots no total, 32 atributos por fonte e oito
afixos por instância. O socket usa um nome/UUID único do modelo; se o modelo estiver
indisponível, o visual acompanha a raiz até o nó voltar a existir. `transfer_item_instance`
transfere instâncias desequipadas e seus inventários de forma atômica.

## 5. Interfaces personalizadas

`show_canvas`, `show_ui`, `canvas_hit_test` e `pointer_position` permanecem disponíveis.
`show_widgets` acrescenta telas com namespace e widgets retidos: botão, texto,
painel, toggle, slider e entrada UTF-8. Layout em linha, coluna ou posições relativas
à tela, com escala pelo viewport lógico 1280×720 e letterbox.

As telas têm foco por Tab, ativação por Enter, ajuste por setas, Backspace UTF-8,
e eventos `on_ui`. `focus_widget`, `widget_value` e `hide_widgets` permitem controle
pelo mod. Escape gera `cancel`. Modais e campos de texto capturam entrada antes da
simulação. Um clique de UI permanece capturado até soltar o botão, mesmo se fechar
sua tela. Callbacks são entregues após a travessia dos widgets, permitindo removê-los.

Texturas são recriadas somente quando os comandos visuais mudam. Quando há canvas
sob widgets, a composição evita rasterizar o mesmo fundo duas vezes.

Limites: quatro telas/mod, 16/sessão, 128 widgets/tela, 512 no total e 1.024 bytes por
texto. Há seleção e clipboard UTF-8, rolagem com recorte, arraste de sliders e drag/drop
por eventos. Não há leitor de tela/UI Automation ou editor visual. O menu inicial, antes da criação do runtime do mundo,
continua em C++; estas telas personalizam a sessão carregada.

## Validação e compatibilidade

`tests/ModPlatformTests.cpp` cobre sistemas, isolamento de orçamento, comportamentos,
volumes, edição atômica, histórico, caminhos, raios, projéteis, afixos, instâncias,
saves, sockets e widgets. `tests/ProceduralTests.cpp` também verifica o carregamento
de arma única quebrada pelo mod oficial, além dos testes anteriores de mundos.

Os resultados da primeira rodada de Debug/Release estão em
[29-MOD-PLATFORM-VALIDATION.md](29-MOD-PLATFORM-VALIDATION.md); a evolução desta etapa
está em [30-MOD-PLATFORM-EXPANSION.md](30-MOD-PLATFORM-EXPANSION.md). Não confundir cobertura
funcional da plataforma com implementação de todos os sistemas finais de um RPG.

A evolução de streaming, persistência paginada, ferramentas de diagnóstico e fluxos
de UI/equipamentos está em [modding/08-STREAMING-AND-DEVELOPER-TOOLS.md](modding/08-STREAMING-AND-DEVELOPER-TOOLS.md).
O relatório 29 registra a rodada anterior; a validação desta etapa está no relatório 30.
