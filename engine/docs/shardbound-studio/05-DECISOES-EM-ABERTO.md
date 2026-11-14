# 05 — Decisões confirmadas e pendências reais

> Revisado em 2026-10-12 após as respostas do autor às perguntas 1–13.
>
> **IMPORTANTE:** este arquivo não é um convite a refazer perguntas já respondidas. O objetivo é impedir reabertura de decisões e separar requisitos de produto das escolhas de implementação. **Definido** não significa implementado.

## 1. Decisões confirmadas anteriormente (manter)

1. Plugin próprio do **Blockbench** especializado para o ShardBound.
2. Experiência de modelagem e animação como no Minecraft com GeckoLib: **pastas/grupos, cubos, pivôs, timeline e keyframes**. **Não** exigir editor de ossos distinto nem pintura de pesos de vértices.
3. Suporte à autoria de **skins de player, itens, blocos, armaduras, monstros, armas corpo a corpo, armas de longa distância, partículas e efeitos**, com texturas/animações aplicáveis.
4. Partículas/efeitos configuráveis na **mesma sessão/timeline** da animação de um modelo, com disparo em frames escolhidos.
5. Modelos GLB/GLTF, preferencialmente GLB com texturas embutidas, conforme pipeline do jogo; cenários e modelos predominantemente em cubos/cuboids.
6. Editor de armas com **player padrão sem skin**, para configurar posição da arma na mão e animar movimentos/poses do player no mesmo ambiente.
7. Projetos com um modelo ou vários modelos independentes relacionados, conforme o caso.
8. Monstros com **hierarquia de pastas totalmente livre**, sem estrutura anatômica obrigatória.
9. Armaduras como **peças sobrepostas** ao player, organizadas em pastas por região do corpo (braço esquerdo/direito etc.) e acompanhando seus movimentos pelo encaixe, sem clipes próprios.
10. O mundo utiliza amplamente **blocos como construção visual dos cenários**, inspirado em Minecraft Dungeons II.
11. A referência a GeckoLib é apenas a **experiência de autoria**, não compatibilidade obrigatória de formatos.
12. Desenvolver **primeiro as ferramentas do plugin no Blockbench** e integrar ao runtime ShardBound **depois**.

## 2. Respostas do autor às 13 perguntas — 2026-10-12

| Nº | Assunto | Decisão / status |
| --- | --- | --- |
| 1 | Configuração de gameplay | **DEFINIDO — Não.** O plugin cuida de autoria visual, modelos, texturas, animações, skins e VFX, **sem formulários de dano, vida, velocidade, IA ou lógica de gameplay**. Gameplay permanece nos sistemas próprios do ShardBound. |
| 2 | Partículas | **DEFINIDO — Editor completo do zero.** Criar partículas do zero dentro do Blockbench, em vez de depender apenas de uma biblioteca de efeitos predefinidos. |
| 3 | Exportação | **DEFINIDO — Arquivos acompanhantes.** Exportar GLB e arquivos auxiliares vinculados de efeitos/eventos; **não exigir** partículas embutidas literalmente num único GLB. |
| 4 | Skins | **DEFINIDO — Padrão Minecraft.** Suportar formato padrão de skins como PNG 64×64, UV/camadas e variantes do player pertinentes. Utilizar template e assets de referência próprios, sem redistribuir texturas proprietárias. |
| 5 | Animações das armas | **DEFINIDO — Criador anima as ações com a arma na mão:** corrida, pulo e ataque, além dos movimentos do jogador exigidos por cada ação. Não depender apenas de uma animação de arma desconectada do personagem. |
| 6 | Blocos colocáveis/quebráveis | **DEFINIDO — Não.** Blocos compõem os cenários, como na referência Minecraft Dungeons II. **Não transformar esta ferramenta em sistema de construção/mineração/remoção de blocos pelo jogador**. |
| 7 | Tipos de projeto | **PREFERÊNCIA, ainda não congelada:** o autor inclina-se a um **formato próprio de projeto** para o plugin. Não decidir extensão ou conversão definitiva sem projeto técnico. |
| 8 | Instalação automática em mods | **NÃO RESPONDIDO / NÃO PRIORITÁRIO.** O autor não entendeu a distinção. A questão é apenas o destino dos arquivos exportados: salvar em uma pasta escolhida versus escrever diretamente num mod instalado. **Não voltar a fazer essa pergunta agora**. Adotar como proposta inicial exportar para pasta escolhida e não alterar mods sem ação explícita. |
| 9 | Preview | **DEFINIDO — Idêntico.** A aparência dos efeitos dentro do Blockbench deve corresponder à aparência no jogo. É um **requisito de fidelidade**, não alegação de que já é tecnicamente possível; precisa de testes visuais e contratos de renderização/VFX. |
| 10 | Plugin pré-existente | **DEFINIDO — Criar outro plugin.** Não tratar o plugin/exportador Blockbench antigo do repositório como o produto a ser evoluído. |
| 11 | Flechas/modelos relacionados | **DEFINIDO — Flechas padrão compartilhadas** pelas armas de longa distância que usam flechas. Mods poderão contornar/substituir isso. A possibilidade geral de projetos compostos (item 7 anterior) continua. Não exigir uma nova flecha exclusiva por cada arma. |
| 12 | Pergunta sobre armaduras avançadas | **RESOLVIDA.** Armaduras sobrepostas ao corpo acompanham o grupo corporal correspondente, sem clipes próprios. |
| 13 | Armas de longa distância | **DEFINIDO — Sim.** Mesmo editor contextual com **player de referência** e animações do jogador junto à arma, adaptado a arcos e demais armas ranged. |

## 2.1 Três decisões complementares — 2026-10-12

Estas três perguntas foram **respondidas** pelo autor e não devem ser repetidas.

| Nova questão | Resposta do autor | Requisito registrado |
| --- | --- | --- |
| Projeto separado só para animações do jogador? | **Não.** "É mais fácil ter isso junto com o projeto de criar o player." | **DEFINIDO:** animações gerais do player são criadas **no mesmo projeto de criação/modelagem do player**; não introduzir categoria separada exclusiva para animações de jogador. Continua existindo o editor contextual de armas para animar ações com elas equipadas. |
| Sons na timeline? | **Sim.** | **DEFINIDO:** inserir/referenciar **sons em keyframes de animação**, com sincronização de eventos de áudio assim como ocorre com partículas, dentro da mesma sessão. |
| Efeitos além de partículas? | **Sim.** | **DEFINIDO:** suportar **rastros de armas, flashes, brilhos e outros efeitos visuais** dentro das ferramentas de efeitos. Mesmo quando um efeito exigir outro mecanismo técnico (material emissivo, faixa geométrica, luz, etc.), não obrigar o autor a tratar essa diferença como um fluxo de criação desconectado. |

**Distinção técnica, não nova pergunta:** partículas são uma maneira de renderizar VFX, não a única. Trilhas contínuas, flashes de material, brilho emissivo e efeitos de luz podem utilizar implementações diferentes. A interface de efeitos deve abstrair isso. Todos os efeitos devem manter a meta de **preview visualmente idêntico ao jogo** e a exportação **GLB com arquivos auxiliares** definidos anteriormente.

**Regra anti-repetição:** não perguntar novamente se o player exige categoria separada de animação, se sons podem ter keyframes ou se VFX incluem efeitos não-partícula. Os três pontos estão fechados.

## 3. Desdobramentos de design que NÃO exigem perguntar novamente

### Armas corpo a corpo e de longa distância

No editor da arma, existe um player padrão sem skin para visualizar encaixes e animar a ação completa do player **com a arma equipada**. O autor pode animar **correr com espada, pular com espada, atacar com espada** etc., manipulando as pastas do player e da arma no contexto da ação. A regra se aplica também a armas de longa distância, com as ações pertinentes (por exemplo disparo).

O player de referência é recurso de edição: sua geometria não precisa ser duplicada nos arquivos exportados de cada arma. Separar os canais/recursos exportados da arma e do player é questão **técnica** para o formato e o runtime.

### Partículas e preview

O autor cria o efeito **do zero** dentro do Blockbench, posiciona emissões/eventos no frame desejado da mesma timeline e exporta os metadados/recursos auxiliares **junto do GLB**. O objetivo da visualização é **correspondência idêntica com o jogo**, exigindo um contrato comum de VFX e verificação lado a lado após a integração. Não confundir preview aspirado com suporte runtime já disponível.

### Cenário e flechas

- **Blocos:** recursos gráficos de construção de cenários; não incluir mecânica de construção/mineração do jogador neste requisito.
- **Flechas:** recurso visual padrão compartilhado entre armas que usam flecha; extensões/mods podem trocar ou personalizar através de recursos próprios. O plugin não precisa forçar autoria de uma flecha a cada arco.
- **Gameplay:** somente dados de autoria visual; estatísticas e regras em sistemas do jogo, não no plugin.
- **Novo plugin:** tratar como projeto novo de ShardBound Studio, mesmo que existam exportadores antigos no repositório.

## 4. Pendências técnicas e decisões internas — NÃO perguntar ao autor sem necessidade

As escolhas abaixo são parte do trabalho de especificação/implementação. **Não transformar cada uma em nova pergunta de produto.**

- Formato e extensão finais do projeto do plugin, considerando a **preferência provisória por formato próprio** e a necessidade de preservar edição no Blockbench.
- Schema versionado dos GLBs e **arquivos acompanhantes** de eventos, partículas e ligações de animação do player/arma.
- Persistência das poses e clips de **corrida, pulo, ataque e outras ações com a arma na mão**, inclusive animação no editor e separação para exportação.
- Estratégia técnica para reproduzir dentro do Blockbench os mesmos parâmetros visuais de VFX usados no ShardBound, mantendo o objetivo de fidelidade idêntica.
- Templates oficiais de skins em padrão Minecraft, com UV e camadas.
- Mecanismo de exportar para uma **pasta escolhida**, sem modificar automaticamente scripts/arquivos de mods; **proposta conservadora** até surgir necessidade real de outra opção.
- Hierarquia de grupos, escala, material, texturas embutidas, animação, validação e registro de recursos.
- Padrões de nomenclatura e reutilização de flechas comuns, respeitando overrides de mods.

## 5. Regra para as próximas conversas

1. **Não repetir** as perguntas 1–6, 9–13, as 7 decisões anteriores, **nem as três decisões complementares da seção 2.1**.
2. **Não perguntar** sobre colocação/quebra de blocos, deformação por pesos, compatibilidade de arquivos GeckoLib ou substituir o player por armadura — esses assuntos já foram resolvidos para este plugin.
3. As únicas incertezas expressamente indicadas pelo autor nesta rodada são a **preferência ainda não definitiva por um formato próprio de projeto** (7) e o **destino operacional da exportação** (8); a segunda pode ter padrão seguro sem nova pergunta.
4. Se uma futura ambiguidade técnica surgir, apresentar **uma proposta concreta com trade-offs e alternativa para o autor escrever sua opinião**, e só perguntar se a decisão realmente bloquear trabalho.
