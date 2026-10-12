# 03 — Partículas, eventos de animação e exportação

## 1. Requisito principal: efeitos no mesmo projeto e animação

**Definido:** durante a animação de um modelo (por exemplo, uma espada), o autor deve poder inserir efeitos de partículas em **frames específicos da mesma sessão do Blockbench**, exportando o resultado relacionado ao modelo. Não exigir animar a espada em uma sessão e editar os efeitos manualmente em outra ferramenta.

Exemplo de experiência:

```text
Projeto: crystal_blade
  Grupo blade
  Grupo handle
  Marcador blade_tip  [proposta: ponto de emissão]

Animation: attack_1
  Rotação da pasta blade:  0.00s, 0.20s, 0.40s...
  Partículas:             0.20s -> crystal_sparks em blade_tip
  Som:                    0.20s -> swing (evento na timeline)
  Evento visual:          0.28s -> trail_on (início do rastro)
  Evento visual:          0.40s -> trail_off (fim do rastro)
  Flash:                  0.40s -> crystal_flash
```

Os tempos e IDs acima são **exemplos**, não convenções definitivas.

### Contexto de animação com player de referência

**Definido em 2026-10-11:** a autoria de animações de armas acontece em um ambiente no Blockbench onde o criador pode ver o **player padrão sem skin**, posicionar a arma na mão e animar o ataque alterando também poses do jogador. Os **eventos de partículas permanecem na mesma sessão/timeline** dessa animação, com a possibilidade de se prender à espada, à mão ou a outro ponto configurado.

**Definido:** o criador deve poder animar também **corrida, pulo e ataque com a espada na mão**, além de outras ações pertinentes. **A especificar tecnicamente:** o modo exato de exportar timelines que alteram partes do player e da arma (clips separados, canais vinculados, composição ou outra representação). Uma futura implementação deve manter o vínculo temporal dos efeitos sem copiar a geometria visual do player para todo GLB de arma.

**Definido para armaduras:** o preview serve principalmente para encaixar peças sobre o player e acompanhar os grupos corporais; não requer poses manuais do player nesse fluxo. Animações opcionais das próprias peças podem ter eventos de VFX, sujeitos ao contrato do runtime.

## 2. Como o vínculo funcionaria

**Proposta técnica:** cada clip possui uma lista ordenada de eventos. Um evento referencia:
- animação/clip de origem;
- tempo (segundos e/ou frame + FPS declarados);
- tipo de evento (`particle`, `sound`, `vfx`, `gameplay_marker`);
- recurso `namespace:path` referenciado;
- grupo/pasta ou marcador de emissão (quando houver);
- parâmetros opcionais, sem obrigar a escrever código.

**Proposta:** permitir marcadores/locators como pontos auxiliares no Outliner. Não são um sistema de ossos separado; servem somente para indicar posições de origem, empunhadura, som ou emissão e seguem as pastas pai.

**Proposta:** oferecer `burst` (explosão pontual), `start`/`stop` (emissor contínuo), `follow` (seguir o grupo) e `detach` (manter no mundo ao emitir). Regras de velocidade, orientação local/mundo e reinício de loop precisam ser especificadas.

**Importante:** a partícula é um efeito visual; o keyframe que causa dano ou cria projétil real deve ser distinguido do VFX. O conteúdo/script do ShardBound decide as consequências de gameplay, evitando que a simples reprodução gráfica de uma animação cause dano duplicado.

## 3. Editor de partículas

**Definido:** criar partículas e efeitos do zero com o plugin, inclusive vinculados a modelos animados. **Também são definidos os keyframes de sons e os efeitos visuais não limitados a partículas**, como rastros de armas, flashes e efeitos de brilho.

**Definido (2026-10-12):** **editor completo para criar partículas do zero dentro do Blockbench**, não apenas seleção de efeitos prontos. Deve permitir autoria visual dos parâmetros dos próprios efeitos, incluindo textura/sprite, escala, cor, opacidade, duração, quantidade, velocidade, emissão, gravidade opcional, rotação e comportamento ao longo do tempo. A lista exata de propriedades poderá evoluir na implementação.

**Definido:** a pré-visualização de partículas/efeitos no Blockbench deve ter **aparência idêntica à do jogo**. Isso é uma meta de fidelidade a ser tecnicamente implementada e validada depois contra o runtime ShardBound, não uma funcionalidade atualmente verificada. Materiais, partículas, transparência, iluminação e ordem de renderização devem ter regras visuais compartilhadas/equivalentes. Não reduzir silenciosamente a exigência a uma aproximação sem informar a diferença.

**Definido:** os efeitos continuam associados a grupos/locators e keyframes da **mesma sessão** de animação do modelo. A criação do editor ocorre na fase do plugin; a execução in-game é integrada depois.

## 4. Trilhas de áudio e outros efeitos visuais — definido

**Sons sincronizados:** o autor quer **inserir sons em frames determinados da timeline**, com o mesmo fluxo visual usado para efeitos de partículas. Cada keyframe aponta para um recurso de áudio e indica quando reproduzi-lo. O editor deverá permitir configurar o evento na **mesma sessão** do modelo/clip. **Proposta técnica:** os arquivos de som poderão ser recursos externos referenciados pelo ID, sem exigir embutir bytes de áudio no GLB.

**VFX avançados:** o autor também quer criar **rastros de armas, flashes e brilhos**, além das partículas tradicionais. O editor deve reuni-los no mesmo fluxo de efeitos. Nem todo efeito precisa ser tecnicamente um emissor de partículas: um **rastro contínuo** pode ser representado por geometria/faixa temporal; um **flash** por uma alteração temporária de cor/material/luz; um **brilho** por material emissivo ou composição luminosa. Essas são **opções de implementação**, não uma exigência de mecanismos específicos.

**Definido:** possibilitar marcação de início, fim ou disparo desses efeitos por keyframes em animações de modelo, arma, player e entidades quando aplicável, e manter o preview com aparência correspondente ao jogo.

**Limites técnicos:** a reprodução efetiva desses efeitos, sons e VFX pelo ShardBound será integrada após o plugin. O formato de cada parâmetro/evento precisa ser validado e versionado; não declarar suporte runtime já pronto.

## 5. Exportação: limites e opções

**Fato técnico:** GLB/glTF padrão é adequado a geometria, nós/transformações, materiais, imagens e animações compatíveis. **Não possui um contrato universal de execução para sistemas de partículas e eventos de gameplay.** É necessário definir metadados próprios e implementá-los no ShardBound.

### Exportação adotada — GLB + arquivos auxiliares (definido)

```text
crystal_blade/
├── crystal_blade.glb            # modelo, texturas embutidas, clips compatíveis
├── crystal_blade.effects.json   # definições ou referências de VFX
├── crystal_blade.events.json    # trilhas sincronizadas com as animações
└── crystal_blade.sl             # definição de conteúdo, se o exportador gerar
```

O exportador faz isso **em um único clique**. Os arquivos auxiliares não devem obrigar o autor a manter sessões ou ferramentas separadas. A estrutura é ilustrativa, não schema fechado.

Vantagens: eventos legíveis, validação independente, VFX reutilizáveis e evolução do contrato sem alterar geometria. Desvantagem: múltiplos arquivos físicos.

### Limites dessa escolha

**Definido:** a saída pode ter **múltiplos arquivos relacionados**, gerados automaticamente em um comando. Não há exigência de armazenar dados de partículas literalmente nos bytes de um único GLB. O nome, a estrutura das pastas e o **schema final** dos auxiliares continuam decisões técnicas a especificar.

O arquivo GLB permanece responsável pelos elementos suportados pelo seu formato (geometria, texturas embutidas, nós e animações compatíveis); os arquivos acompanhantes registram eventos e parâmetros de VFX específicos do ShardBound. A possibilidade técnica de `extras` no glTF não é a solução escolhida para o contrato principal.

## 6. Exemplo de contrato proposto, não implementado

```json
{
  "schema": "shardbound.animation_events.preview",
  "model": "demo:crystal_blade",
  "animations": {
    "attack_1": [
      {
        "time_seconds": 0.2,
        "type": "particle",
        "effect": "demo:crystal_sparks",
        "attach_to": "blade_tip",
        "action": "burst"
      }
    ]
  }
}
```

- `schema`, chaves e nomes são **apenas proposta**. Não afirmar que o compilador já reconhece esse JSON.
- O ID do VFX precisa ser resolvido pelo registry/mount de assets.
- Um evento em grupo removido/inexistente deve produzir erro objetivo.
- A exportação deve preservar determinismo de ordem entre eventos coincidentes.

## 7. Comportamento em runtime a especificar

1. Avaliar animações com clocks consistentes, velocidades/loops e transições.
2. Disparar eventos **uma vez quando o tempo atravessar o keyframe**, inclusive sob quedas de FPS, sem perdê-los nem duplicá-los.
3. Transformar a posição local do grupo/marcador em posição global.
4. Controlar vida, rate limits e descarregamento de emissores/partículas.
5. Lidar com pausa, cancelamento, troca de animação, morte e entidade descarregada.
6. Carregar texturas de partículas separadamente quando necessário, mesmo que o modelo utilize texturas embutidas.
7. Definir claramente o que é client-side visual e o que depende da lógica do jogo.

## 8. Critério de aceite proposto

**Fase plugin:** criar uma espada com duas pastas, textura, player de referência e clips de correr, pular e atacar; adicionar **partículas criadas do zero**, um **evento de som** e um **rastro ou flash** em keyframes específicos da mesma timeline. Conferir preview visual, salvar/reabrir e exportar GLB com arquivos acompanhantes **sem editar JSON manualmente**.

**Fase de integração posterior:** importar no ShardBound e comprovar que **partículas, rastros/flashes e sons sincronizados** funcionam nos frames definidos e que a aparência visual é **idêntica** ao preview (respeitando as condições de renderização previstas), inclusive com FPS variável.
