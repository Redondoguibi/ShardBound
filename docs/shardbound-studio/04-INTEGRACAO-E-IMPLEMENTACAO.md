# 04 — Integração com o ShardBound e plano de implementação

## 1. Separação de responsabilidades

**Blockbench + plugin (novo):** ferramentas de autoria visual, formato editável próprio preferido (não congelado), templates, grupos, cubos, texturas/UV, animações do player com armas, editor completo de partículas do zero, eventos, preview visual idêntico ao jogo e exportação/validação. **Não inclui parâmetros de gameplay**.

**Asset pipeline:** verificar estrutura, versões e dependências, converter/importar GLB e dados auxiliares, registrar assets públicos declarados, cache incremental e erros diagnósticos.

**Runtime ShardBound:** renderizar modelos, aplicar animações hierárquicas, reproduzir efeitos, anexar armaduras/armas, executar eventos, resolver referências e integrar com gameplay/scripting.

```text
Blockbench/ShardBound Studio (projeto editável, formato próprio preferido)
   |
   v
Export + validate
   |             |
   v             v
GLB + textures   event/VFX metadata
   \             /
    v           v
  mod files / assets.json / .sl
            |
       mod loader + asset compiler
            |
   animations + VFX + equipment runtime
            |
          gameplay
```

O formato intermediário acima é **proposto**; os registries específicos e a sintaxe exata de `.sl` devem respeitar as versões executáveis do ShardBound, não os exemplos deste documento.

## 2. Compatibilidade com decisões existentes

- IDs públicos `namespace:path`; evitar referenciar caminhos físicos na lógica de gameplay.
- Assets precisam de declaração pública explícita; não registrar toda textura/modelo automaticamente apenas por existir em uma pasta.
- GLB/GLTF como fonte principal, preferência por texturas de modelo embutidas no GLB.
- PNG continua válido para UI/VFX/texturas independentes.
- Source editável preservado; importador compila/cacheia sem reescrever os originais.
- Cubos/cuboids/pixel art como caminho principal, mas modelos especiais podem usar outras geometrias.
- O runtime deve respeitar mods ausentes, referências inválidas e descarregamento de recursos.

## 3. Situação do jogo na documentação atual

De acordo com `docs/14-ASSET-PIPELINE.md`, `docs/modding/03-ASSETS-AND-MODELS.md` e `docs/24-EQUIPMENT-AND-WORLD-CONTENT.md`, na revisão de 2026-10-11:

| Área | Estado documentado |
| --- | --- |
| Importação estática GLB/GLTF com texturas embutidas | Implementada para o subconjunto suportado |
| Material `OPAQUE` e `MASK` | Implementados |
| `BLEND` | Rejeitado pelo importador atual |
| Clips glTF executados no runtime | Ainda não suportados |
| Animação atual de personagem | Poses rígidas/procedurais, sem skinning ou clips glTF |
| Modelos `.bbmodel` de player/scout/brute | Existem; seis grupos de player e pivôs preservados |
| Equipamentos e inventário | Existem parcialmente; atual gameplay não pressupõe armaduras completas |
| Contrato final de efeitos/partículas para mods | Ainda não especificado |

**Conclusão:** o plugin pode ser criado agora, mas não se deve declarar concluído o suporte de animações, armaduras ou partículas só porque o Blockbench consegue exportar arquivos.

## 3.1 Novas regras funcionais definidas em 2026-10-11

- Armas melee/ranged: player padrão sem skin, posição na mão e **animações de corrida, pulo, ataque e disparo com a arma equipada** no mesmo editor.
- Armaduras: sobreposição ao corpo do player, peças organizadas por grupos (inclusive lados esquerdo e direito), seguindo animação corporal; animações adicionais opcionais; sem exigir poses do player durante a autoria.
- Monstros: grupos e hierarquias arbitrários, sem obrigar uma estrutura de seis membros.
- Modelos: projetos com objeto único ou modelos independentes relacionados, conforme o caso.
- Blocos: componentes da construção visual do cenário; **não** incluir colocação, mineração ou quebra pelo jogador.
- GeckoLib: familiaridade da interface, não compatibilidade de formatos.
- Partículas criadas **do zero** no editor, por keyframes; **sons também em keyframes**, além de **rastros, flashes e brilhos**; GLB exportado com **arquivos auxiliares**.
- Preview deve ter aparência **idêntica à do ShardBound**; comprovar isso na fase de integração.
- Skins no padrão Minecraft, com **PNG 64×64**, UV, camadas e braços aplicáveis.
- Flecha padrão compartilhada pelas armas que usam flechas, com personalização possível por mods.
- Sem formulários de dano, vida, velocidade, IA e outros parâmetros de gameplay.
- Criar **outro plugin**, independente do exportador histórico.
- Sequência: ferramentas do plugin primeiro; integração ao jogo posteriormente.

**Nota:** essas regras são requisitos do produto ainda não implementados. Não são evidência de suporte no engine.

## 4. Contratos necessários antes de produção ampla

1. **Formato de projeto/metadata:** versão, tipo de conteúdo, IDs, template, configuração de exportação.
2. **Hierarquia de grupos:** regras de pivôs, nomes, transformações, parentagem e escala; sem workflow separado de ossos.
3. **Animações:** clips, curvas/interpolações aceitas, unidades de tempo, loops, blending/transições e nomenclatura.
4. **Efeitos:** eventos de timeline, locators, resource IDs, emissão e ciclo de vida.
5. **Texturas/materiais:** embutidos no GLB, formato da imagem, transparência, filtro e UV.
6. **Equipamento/skin:** template de player, grupos compatíveis, pontos de empunhadura e mapeamento de peças.
7. **Mod package:** como gerar/atualizar `assets.json`, `.sl` e demais declarações **sem sobrescrever trabalho manual**.
8. **Versão de compatibilidade:** plugin, export schema, importer e runtime precisam informar versões e migrações.

## 5. Ordem de desenvolvimento — decisão do autor (2026-10-11)

**Definido:** **construir primeiro todas as ferramentas de autoria do plugin no Blockbench e realizar a integração com o ShardBound depois.** Esta decisão substitui a proposta anterior de desenvolver plugin e runtime recurso por recurso.

A separação de fases deve ser explícita: **editor funcionando no Blockbench** não significa **asset animado/efeito funcionando no jogo**. Já na fase de autoria, projetar contratos exportáveis e versionados para não criar arquivos impossíveis de importar.

### Fase A — Ferramentas do plugin, antes da integração

Construir, testar e aperfeiçoar dentro do Blockbench:

- Modelagem por cubos, pastas, pivôs, textura/UV e timeline familiar ao fluxo GeckoLib;
- Criação de **player com animações gerais no próprio projeto do player** (sem categoria separada para animar o jogador), skin no padrão Minecraft (PNG 64×64), item, arma melee/ranged, bloco de cenário, armadura e monstro;
- **Editor contextual de armas melee e ranged com player padrão sem skin**, empunhadura ajustável e animações do jogador de correr, pular, atacar/disparar com a arma na mão, no mesmo ambiente. **As animações gerais sem arma permanecem no projeto de criação do player**;
- **Preview/edição de armadura** sobre o corpo, vinculada a pastas por regiões corporais, sem exigir poses do player; animações próprias opcionais;
- **Hierarquias inteiramente livres** para monstros;
- Projetos de um modelo ou vários modelos independentes relacionados;
- **Editor completo para criar partículas do zero**, **rastros de armas, flashes e brilhos**, além de **sons e eventos em keyframes da timeline**, tudo integrado à animação;
- Validação, persistência do projeto editável (**formato próprio preferido, ainda sem extensão final**) e exportação de **GLB acompanhado de arquivos de efeitos/eventos** em contratos versionados;
- Exemplos, testes de UI, preview com meta de **fidelidade visual idêntica** ao jogo e testes de salvar/reabrir o projeto; comparação direta com o runtime na fase B.
- Não configurar estatísticas ou regras de gameplay. Reutilizar flechas padrão, personalizáveis por mods.

A divisão da fase A em versões menores continua técnica; **não há dúvida sobre a profundidade do editor de partículas: ele deverá permitir criação do zero**. O autor quer concluir as ferramentas do plugin antes da integração com o runtime.

### Fase B — Integração posterior ao ShardBound

Somente após o editor estar construído, desenvolver/testar:

- Importação dos modelos, texturas e metadados exportados;
- Reprodução de hierarquias de grupos e clips;
- Composição de clips de armas com animações do player;
- Equipamentos que seguem grupos do player, com animações opcionais;
- Partículas, rastros/flashes/brilhos, **sons e eventos** vinculados a frames e partes móveis;
- Carregamento de modelos relacionados e blocos usados na montagem de cenários;
- Validação de compatibilidade, integração aos mods/registries e performance.

### Compatibilidade com GeckoLib — definida

**Somente a experiência de edição e animação é requisito.** Não é exigido importar arquivos de projetos GeckoLib/Minecraft nem exportar para formatos GeckoLib. A saída continua dirigida ao ShardBound.

### Blocos — direção versus lacuna

**Definido:** a maior parte do cenário do ShardBound utiliza blocos e peças modulares, com Minecraft Dungeons II como referência visual. **Definido:** não implementar colocação, remoção, quebra ou mineração de blocos pelo jogador como requisito do plugin; trata-se de autoria de cenário.

## 6. Critérios de aceite por fase

**Fase A — plugin:** validar a criação do **player e de suas animações gerais no mesmo projeto**, sem categoria de animação separada; armas melee/ranged com player de referência e clips de **corrida, pulo, ataque/disparo com arma equipada**; skin PNG 64×64 estilo Minecraft; armaduras por pastas; monstros livres; múltiplos modelos; blocos de cenário; partículas **criadas do zero**, **rastros/flashes/brilhos** e **eventos de som** com keyframes e preview. Confirmar salvar/reabrir e exportar GLB com arquivos acompanhantes, sem exigir execução in-game nesta fase.

**Fase B — integração ao jogo:** os testes abaixo continuam como metas quando o runtime for implementado, incluindo **sincronização de sons**, reprodução de rastros/flashes/brilhos e **comparação de efeitos no Blockbench e no jogo** para confirmar visual idêntico. Sua existência não bloqueia o aceite da fase A.

### Testes integrados futuros

- Uma espada com animação por grupo e partícula disparada na ponta durante `attack_1`.
- Um monstro com pastas de membros, `idle`/`walk`/`attack`, reproduzindo sem mudança inesperada de pivô.
- Uma arma de longa distância com efeito no ponto visual de saída no frame marcado (sem implicar que o keyframe sozinho crie projétil de gameplay).
- Uma peça de armadura que acompanha a pasta correspondente do player.
- Uma skin que mantém UV/alinhamento no template adotado.
- Um bloco animado e texturizado sem regressão de colisão.
- Reimportação determinística, erros claros e nenhum dado aceito silenciosamente quando não suportado.

## 7. Novo plugin — decidido

**Definido:** o ShardBound Studio será **um novo plugin**, e não a evolução obrigatória do antigo exportador Blockbench. Os arquivos existentes no repositório podem servir para verificar contratos com o jogo, mas não alteram essa decisão.

## 8. Destino operacional da exportação — padrão sugerido

O autor não entendeu a pergunta sobre copiar arquivos diretamente a mods, e não precisa respondê-la agora. **Proposta conservadora:** ao exportar, escolher pasta de destino para o GLB e os arquivos acompanhantes, sem editar automaticamente scripts ou mods existentes. Só reconsiderar caso exista uma necessidade concreta.
