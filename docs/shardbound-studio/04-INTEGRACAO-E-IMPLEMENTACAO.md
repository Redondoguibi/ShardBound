# 04 — Integração com o ShardBound e plano de implementação

## 1. Separação de responsabilidades

**Blockbench + plugin:** experiência de autoria, projetos `.bbmodel`, templates, cubos, pastas, pivôs, texturas/UV, clips, keyframes de eventos, parâmetros VFX, preview e exportação/validação.

**Asset pipeline:** verificar estrutura, versões e dependências, converter/importar GLB e dados auxiliares, registrar assets públicos declarados, cache incremental e erros diagnósticos.

**Runtime ShardBound:** renderizar modelos, aplicar animações hierárquicas, reproduzir efeitos, anexar armaduras/armas, executar eventos, resolver referências e integrar com gameplay/scripting.

```text
Blockbench/ShardBound Studio (source .bbmodel)
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

- Armas: player padrão sem skin no ambiente de edição, posicionamento na mão, alterações de poses/partes do jogador e animações de ataque ali mesmo.
- Armaduras: sobreposição ao corpo do player, peças organizadas por grupos (inclusive lados esquerdo e direito), seguindo animação corporal; animações adicionais opcionais; sem exigir poses do player durante a autoria.
- Monstros: grupos e hierarquias arbitrários, sem obrigar uma estrutura de seis membros.
- Modelos: projetos com objeto único ou modelos independentes relacionados, conforme o caso.
- Blocos: componentes recorrentes da arquitetura visual do cenário, não reduzidos a adereços.
- GeckoLib: familiaridade da interface, não compatibilidade de formatos.
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
- Criação de skin, item, arma melee/ranged, bloco de cenário, armadura e monstro;
- **Editor contextual de armas com player padrão sem skin**, empunhadura ajustável, poses do player e clips de ataque no mesmo ambiente;
- **Preview/edição de armadura** sobre o corpo, vinculada a pastas por regiões corporais, sem exigir poses do player; animações próprias opcionais;
- **Hierarquias inteiramente livres** para monstros;
- Projetos de um modelo ou vários modelos independentes relacionados;
- Editor/trilhas para partículas, sons e eventos por frame integrados à animação;
- Validação, persistência `.bbmodel` e exportação em contratos propostos/versionados;
- Exemplos, testes da UI, pré-visualização e reabertura/roundtrip do projeto.

A divisão da fase A em versões menores e a profundidade do editor de partículas ainda estão **em aberto**. A intenção do autor é terminar o conjunto de ferramentas do plugin **antes** de implementar a integração completa ao runtime, não necessariamente fazer uma entrega pública única.

### Fase B — Integração posterior ao ShardBound

Somente após o editor estar construído, desenvolver/testar:

- Importação dos modelos, texturas e metadados exportados;
- Reprodução de hierarquias de grupos e clips;
- Composição de clips de armas com animações do player;
- Equipamentos que seguem grupos do player, com animações opcionais;
- Partículas e eventos vinculados a frames e partes móveis;
- Carregamento de modelos relacionados e blocos usados na montagem de cenários;
- Validação de compatibilidade, integração aos mods/registries e performance.

### Compatibilidade com GeckoLib — definida

**Somente a experiência de edição e animação é requisito.** Não é exigido importar arquivos de projetos GeckoLib/Minecraft nem exportar para formatos GeckoLib. A saída continua dirigida ao ShardBound.

### Blocos — direção versus lacuna

**Definido:** a maior parte do cenário do ShardBound utiliza **blocos** em estilo de construção semelhante a Minecraft Dungeons II. O plugin deve poder produzir esses blocos e peças modulares para o mundo. **Não decidido:** se o jogador coloca/remove/quebra blocos durante o gameplay. A escolha da ordem das fases não resolve essa questão.

## 6. Critérios de aceite por fase

**Fase A — plugin:** testar no próprio Blockbench criação/edição de armas com player de referência, posicionamento em mão, animação de ataque envolvendo pastas do player, armadura separada por pastas, hierarquia livre de monstro, modelos independentes relacionados, blocos de cenário e keyframes de efeitos. Confirmar salvar/reabrir/exportar projetos e validar diagnósticos **sem exigir execução in-game**.

**Fase B — integração ao jogo:** os testes abaixo continuam como metas para quando o runtime for implementado. Sua existência na documentação não bloqueia o aceite da fase A.

### Testes integrados futuros

- Uma espada com animação por grupo e partícula disparada na ponta durante `attack_1`.
- Um monstro com pastas de membros, `idle`/`walk`/`attack`, reproduzindo sem mudança inesperada de pivô.
- Uma arma de longa distância com efeito no ponto visual de saída no frame marcado (sem implicar que o keyframe sozinho crie projétil de gameplay).
- Uma peça de armadura que acompanha a pasta correspondente do player.
- Uma skin que mantém UV/alinhamento no template adotado.
- Um bloco animado e texturizado sem regressão de colisão.
- Reimportação determinística, erros claros e nenhum dado aceito silenciosamente quando não suportado.

## 7. Observação sobre ferramentas existentes

O repositório já documenta fontes `.bbmodel` e um fluxo anterior de exportação Blockbench (`assets/core/models/README.md` menciona "plugin ShardBound Blockbench"). A implementação desse fluxo **não foi auditada aqui** para reutilização, migração ou compatibilidade. Investigar o existente antes de criar um plugin inteiramente novo, sem assumir que atende aos requisitos de timeline/VFX.
