# Plataforma de mods: implementação e validação

Data: 2026-10-17. Este relatório descreve o estado local validado, incluindo as
alterações anteriores do projeto. Não representa uma release publicada nem a
conclusão de todos os sistemas possíveis de gameplay.

Contratos: [28-MOD-PLATFORM-FOUNDATIONS.md](28-MOD-PLATFORM-FOUNDATIONS.md).
Referência e exemplos: [modding/07-MOD-PLATFORM.md](modding/07-MOD-PLATFORM.md).

## Funcionalidades implementadas

| Área | Recursos integrados |
| --- | --- |
| Sistemas de gameplay | Dependências entre sistemas, detecção de ciclos, intervalos, desativação opcional após erro, ordem determinística e orçamento independente por callback externo. Índice de componentes personalizados evita varreduras de todas as entidades. |
| Geração e edição | Regras volumétricas opcionais por ruído, filtros de bloco/bioma/altura, extensão por mods e persistência no snapshot do gerador. Edição em lote, aplicação de estruturas com rotação, undo/redo e preparação transacional de meshes/colisores. |
| IA e entidades | A* sobre terreno editado, árvores de comportamento com memória serializável, máquinas de estados com entrada/saída, raycasts e projéteis persistentes com gravidade e eventos. Raios consultam terreno além do streaming. |
| Equipamentos | Instâncias individuais, durabilidade, afixos determinísticos por seed, atributos, reservas de inventário, hooks, eventos, validação de saves e sockets de modelos animados. O mod foundation integra dano, armadura, desgaste, arma quebrada e restauração do visual. |
| Interfaces | Telas com namespace, layouts, botões, labels, painéis, toggles, sliders e campos UTF-8. Foco por teclado, eventos, captura modal e composição visual com cache. |

As regras de geração são opcionais. As planícies oficiais continuam com pouco
relevo e sem cavernas. Os contratos anteriores de slots, sistemas e geração foram
mantidos; novos componentes usam dados versionados. Não foi adicionada uma
migração obrigatória para saves antigos válidos.

## Compilação e testes finais

Executados no Windows, com os presets e dependências do próprio projeto:

```powershell
./tools/build.ps1 -Preset debug
./tools/build.ps1 -Preset release
```

| Configuração | Compilação | CTest | Tempo do CTest |
| --- | --- | --- | --- |
| Debug | Concluída | 38/38 aprovados, zero falhas | 112,64 s |
| Release | Concluída | 38/38 aprovados, zero falhas | 67,09 s |

Logs locais: `build/mod-platform-debug.log` e
`build/mod-platform-release.log`. Estes arquivos são artefatos de build e podem
ser substituídos em execuções futuras. Os logs finais não apresentam diagnósticos
MSVC `warning C...` ou `error C...`. A última rodada foi incremental: os alvos
alterados foram recompilados e a suíte CTest completa foi executada em cada preset.

A suíte inclui o novo `mod_platform`, além dos testes existentes de scripts,
gameplay, mundos procedurais, geração, apresentação, equipamentos, Studio,
extensões, saves e smoke tests. A cobertura adicionada verifica:

- Ordenação e ciclos de sistemas, temporização, isolamento de orçamento e erro.
- Retomada de árvores de comportamento e transições de estado.
- Ciclo de vida do índice de componentes, incluindo troca de mundo.
- Volumes com ar e blocos acima da altura original; snapshots e colisores.
- Validação integral de lotes, undo/redo e falha de preparação com o limite de
  4.096 corpos físicos ocupado, preservando terreno e entidades anteriores.
- Caminhos com obstáculos editados e limite de busca; raios em terreno distante.
- Impacto de projéteis rápidos, expiração, destruição e persistência da origem.
- Sorteio de afixos, reservas de cópias, slots antigos e únicos, atributos,
  durabilidade, arma quebrada e rejeição de save inconsistente sem trocar o mundo.
- Carregamento de arma única quebrada pelo mod oficial em `ProceduralTests.cpp`.
- Sockets em poses de modelos e igualdade entre referências à mesma entidade.
- Captura de movimento/ataque por UI, clique retido até soltar, bloqueio por widget
  superior desabilitado, edição UTF-8, slider, namespace e cache de textura.

Também foi inspecionada a imagem renderizada pelo teste de widgets, produzida com
o renderer real, verificando texto UTF-8, foco e slider. Essa inspeção não equivale
a uma sessão manual prolongada de todas as funcionalidades.

## Problemas encontrados e corrigidos

| Problema | Correção e evidência |
| --- | --- |
| Edição podia deixar terreno e física inconsistentes ao falhar a reconstrução | Preparação dos recursos antes da troca; teste com capacidade física esgotada. |
| Colisão baseada apenas na altura não representava vazios ou volumes adicionais | Agrupamento pela ocupação sólida das colunas; teste de ocupação e raycasts. |
| Raios dependiam do terreno já carregado | Consulta procedural contínua combinada com Jolt; teste fora dos chunks ativos. |
| Equipar podia usar um item consumido por um hook | Releitura e validação das reservas após o hook; testes de integração de inventário. |
| Referências distintas para a mesma entidade não comparavam como iguais | Igualdade por mundo e EntityId, validada em teste. |
| Varredura de entidades custosa mesmo sem projéteis | Índice por componente e retorno imediato quando vazio; testes de manutenção do índice. |
| Novo handler de projéteis rejeitava delta legado mesmo sem projéteis ativos | Validação após a consulta de ativos; regressão de apresentação corrigida e suíte repetida. |
| Clique podia atingir gameplay após fechar a UI, ou atravessar widget desabilitado | Captura até MouseUp e bloqueio pela camada superior; testes de entrada. |
| Foco podia permanecer em mais de uma tela | Foco exclusivo entre telas ao clicar, navegar ou definir foco programaticamente. |

Durante a implementação também foram corrigidos um conflito de nome com o AST
e avisos de sombreamento do compilador. As rodadas finais acima foram executadas
após as correções de código. Não restou falha conhecida nos testes executados.

## Desempenho e estabilidade

O índice de componentes elimina a enumeração global para consultar projéteis e
componentes por tipo. Consultas de mundo reaproveitam colunas e um cache limitado
de decorações. Lotes reconstruem cada chunk afetado uma vez, atualizando também
as faces vizinhas. Widgets só recriam a textura quando seus comandos visuais
mudam; a composição evita rasterizar o mesmo canvas duas vezes.

Essas são melhorias estruturais verificadas por testes e revisão do código. Os
tempos de CTest não são benchmarks de FPS, latência de frame ou uso de memória.
Não foi realizado benchmark comparativo em várias máquinas nem teste prolongado
de exploração por horas.

## Limitações restantes

- Mundo: altura de 48 blocos, até 4.096 alterações persistentes e 16 transações de
  histórico apenas na sessão. Uma edição pode ser rejeitada com segurança se não
  houver capacidade temporária para preparar novos colisores. Não há fluidos
  dinâmicos, iluminação volumétrica ou geração assíncrona.
- Navegação: agentes de uma célula, busca limitada a 4.096 expansões e raio de
  128 células em X/Z; não trata atores móveis como obstáculos nem substitui navmesh.
  Callbacks de comportamentos não são transações que desfazem efeitos no mundo.
- Projéteis: colisão pontual, até 512 ativos e delta de atualização até 0,25 s.
  Raycasts procedurais têm limites de alcance/coordenadas documentados no guia.
- Equipamentos: até 256 instâncias por ator, 32 slots, oito afixos e 32 atributos
  por fonte; ainda não há transferência atômica de instâncias entre proprietários.
- UI: o menu inicial de escolher/criar mundo continua em C++; os widgets atuam
  dentro da sessão. Não há integração com leitores de tela, seleção/copiar/colar,
  scroll automático, editor visual ou arraste contínuo de slider.
- Validação: a compatibilidade foi exercitada pelos fixtures e testes existentes,
  sem promessa de cobertura de todo mod externo ou de todo save já produzido.
  Saves com reservas de equipamento inconsistentes são rejeitados intencionalmente.

Estas limitações são trabalho restante, não funcionalidades concluídas. As cinco
áreas receberam APIs reais e testes de integração; sistemas específicos de jogo
continuam sendo responsabilidade dos mods.
