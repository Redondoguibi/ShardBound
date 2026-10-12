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
  Som:                    0.20s -> swing
  Evento visual:          0.28s -> trail_on
  Evento visual:          0.40s -> trail_off
```

Os tempos e IDs acima são **exemplos**, não convenções definitivas.

### Contexto de animação com player de referência

**Definido em 2026-10-11:** a autoria de animações de armas acontece em um ambiente no Blockbench onde o criador pode ver o **player padrão sem skin**, posicionar a arma na mão e animar o ataque alterando também poses do jogador. Os **eventos de partículas permanecem na mesma sessão/timeline** dessa animação, com a possibilidade de se prender à espada, à mão ou a outro ponto configurado.

**Em aberto:** o modo exato de exportar uma timeline que altera partes do player e partes da arma (clips separados, canais vinculados, composição ou outra representação). Uma futura implementação deve manter o vínculo temporal dos efeitos sem copiar a geometria visual do player para todo GLB de arma.

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

**Definido como capacidade desejada:** criar partículas e efeitos com o plugin, inclusive vinculados a modelos animados.

**Proposta:** painel "Particle Editor" dentro do Blockbench, com edição de textura/sprite, escala, cor, opacidade, duração, quantidade, velocidade, emissão, gravidade opcional, rotação e curva temporal. Usar preview da cena do Blockbench se a API permitir implementar pré-visualização fiel.

A fidelidade da pré-visualização deve ser validada contra o runtime real. Efeitos de materiais emissivos, transparência e iluminação dinâmica dependem do renderer.

**Em aberto:** se o editor de partículas cria os próprios efeitos completamente ou inicialmente apenas referencia efeitos definidos em assets separados. A experiência final deve permitir autoria integrada; o nível de entrega por fase é uma escolha de roadmap.

## 4. Exportação: limites e opções

**Fato técnico:** GLB/glTF padrão é adequado a geometria, nós/transformações, materiais, imagens e animações compatíveis. **Não possui um contrato universal de execução para sistemas de partículas e eventos de gameplay.** É necessário definir metadados próprios e implementá-los no ShardBound.

### Alternativa A — GLB + arquivos auxiliares (preferência proposta; ainda não aprovada)

```text
crystal_blade/
├── crystal_blade.glb            # modelo, texturas embutidas, clips compatíveis
├── crystal_blade.effects.json   # definições ou referências de VFX
├── crystal_blade.events.json    # trilhas sincronizadas com as animações
└── crystal_blade.sl             # definição de conteúdo, se o exportador gerar
```

O exportador faz isso **em um único clique**. Os arquivos auxiliares não devem obrigar o autor a manter sessões ou ferramentas separadas. A estrutura é ilustrativa, não schema fechado.

Vantagens: eventos legíveis, validação independente, VFX reutilizáveis e evolução do contrato sem alterar geometria. Desvantagem: múltiplos arquivos físicos.

### Alternativa B — Metadados dentro do GLB (em aberto)

O glTF permite campos `extras` ou extensões customizadas. Poderíamos armazenar trilhas e declarações de VFX no próprio GLB. **Isso não faz os efeitos funcionarem em viewers genéricos**: somente leitores que conheçam a extensão do ShardBound conseguirão executá-los. O pipeline deve preservar ou extrair esses metadados.

Vantagens: asset autocontido; desvantagens: compatibilidade, versionamento e depuração.

**Não decidido:** formato final da exportação de partículas. A conversa só confirmou que efeitos devem viajar **associados ao modelo**, não necessariamente dentro dos bytes do `.glb`.

## 5. Exemplo de contrato proposto, não implementado

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

## 6. Comportamento em runtime a especificar

1. Avaliar animações com clocks consistentes, velocidades/loops e transições.
2. Disparar eventos **uma vez quando o tempo atravessar o keyframe**, inclusive sob quedas de FPS, sem perdê-los nem duplicá-los.
3. Transformar a posição local do grupo/marcador em posição global.
4. Controlar vida, rate limits e descarregamento de emissores/partículas.
5. Lidar com pausa, cancelamento, troca de animação, morte e entidade descarregada.
6. Carregar texturas de partículas separadamente quando necessário, mesmo que o modelo utilize texturas embutidas.
7. Definir claramente o que é client-side visual e o que depende da lógica do jogo.

## 7. Critério de aceite proposto

Criar uma espada com duas pastas, textura e clip `attack_1`; adicionar no próprio Blockbench um keyframe de partícula na ponta; exportar; importar no ShardBound; reproduzir o golpe e verificar que a partícula aparece na posição e no instante corretos, sem precisar editar JSON manualmente.
