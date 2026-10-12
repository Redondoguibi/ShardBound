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

## 4. Contratos necessários antes de produção ampla

1. **Formato de projeto/metadata:** versão, tipo de conteúdo, IDs, template, configuração de exportação.
2. **Hierarquia de grupos:** regras de pivôs, nomes, transformações, parentagem e escala; sem workflow separado de ossos.
3. **Animações:** clips, curvas/interpolações aceitas, unidades de tempo, loops, blending/transições e nomenclatura.
4. **Efeitos:** eventos de timeline, locators, resource IDs, emissão e ciclo de vida.
5. **Texturas/materiais:** embutidos no GLB, formato da imagem, transparência, filtro e UV.
6. **Equipamento/skin:** template de player, grupos compatíveis, pontos de empunhadura e mapeamento de peças.
7. **Mod package:** como gerar/atualizar `assets.json`, `.sl` e demais declarações **sem sobrescrever trabalho manual**.
8. **Versão de compatibilidade:** plugin, export schema, importer e runtime precisam informar versões e migrações.

## 5. Roadmap proposto, ainda não aprovado

### V1 — Base confiável de criação/exportação

- Formato ou preset ShardBound no Blockbench, sem romper edição `.bbmodel`.
- Templates de itens, blocos, armas e player.
- Exportação GLB com texturas embutidas e escala/UV/pivôs validados.
- Geração assistida de identificadores e declaração de assets, quando couber.
- Testes de roundtrip: abrir, editar, salvar, exportar e carregar asset estático no jogo.

### V2 — Animação de grupos + eventos

- Clips do Blockbench exportados com hierarquia correta.
- Runtime que reproduza os clips sem exigir skinning por pesos na autoria.
- Animar monstros, armas e peças anexadas.
- Trilhas de efeitos/sons por frame; marcadores de emissão e validação.
- Integração de VFX com o renderer do jogo e preview quando viável.
- Testes de keyframes, loops, pausa, troca de clip, alinhamento e FPS variável.

### V3 — Fluxo profissional de conteúdo/mods

- Armaduras completas seguindo grupos do player; skins e variantes.
- Ferramentas ricas para armas de longa distância e respectivos pontos de origem de efeitos/projéteis.
- Editor de partículas mais completo, reuso de efeitos e pré-visualização confiável.
- Exportar/atualizar diretamente pacote de conteúdo do mod com validação e documentação.
- Compatibilidade/versionamento, mensagens de erro e exemplos oficiais.

As fases acima são **propostas de ordem**, não escopo cancelado: todos os tipos pedidos continuam como requisitos do produto.

## 6. Testes mínimos de aceitação

- Uma espada com animação por grupo e partícula disparada na ponta durante `attack_1`.
- Um monstro com pastas de membros, `idle`/`walk`/`attack`, reproduzindo sem mudança inesperada de pivô.
- Uma arma de longa distância com efeito no ponto visual de saída no frame marcado (sem implicar que o keyframe sozinho crie projétil de gameplay).
- Uma peça de armadura que acompanha a pasta correspondente do player.
- Uma skin que mantém UV/alinhamento no template adotado.
- Um bloco animado e texturizado sem regressão de colisão.
- Reimportação determinística, erros claros e nenhum dado aceito silenciosamente quando não suportado.

## 7. Observação sobre ferramentas existentes

O repositório já documenta fontes `.bbmodel` e um fluxo anterior de exportação Blockbench (`assets/core/models/README.md` menciona "plugin ShardBound Blockbench"). A implementação desse fluxo **não foi auditada aqui** para reutilização, migração ou compatibilidade. Investigar o existente antes de criar um plugin inteiramente novo, sem assumir que atende aos requisitos de timeline/VFX.
