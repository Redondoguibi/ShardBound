# Implementação e cobertura

Versão **1.3.0**, autoria para **Blockbench Desktop 5.2.2**, contratos **v1**. O editor permanece em `BLOCKBENCH/studio/`. A API v1 e os callbacks de validação assíncrona continuam compatíveis com a Bridge 0.5. MTools, assets do jogo e runtime permanecem independentes.

A versão 1.3 integra os fluxos aos lugares nativos: Arquivo para criação/salvamento/exportação, hierarquia para modelos, Texturas/Pintar para materiais e sprites, Animar/timeline para clips, efeitos e sons. Não existe painel Studio nem submenu Studio em Ferramentas. As propriedades de efeitos estendem as propriedades nativas do keyframe, com prévia durante os ajustes e curvas visuais. Áudio pode ser escolhido no frame ou arrastado sobre a timeline. A revisão inclui Desfazer atômico, validação contextual, limpeza no unload e correção conservadora de UV de templates antigos. Veja [revisão de UX](REVISAO-UX.md). Dados existentes mantêm o contrato v1.

Foram usados como especificação todos os documentos de `docs/shardbound-studio/`: README, visão/escopo, modelagem/texturas/animações, partículas/eventos/exportação, integração/implementação e decisões. As escolhas técnicas que estavam abertas — extensão, schemas, divisão dos GLBs e exportação dos canais corporais — estão registradas em `CONTRATOS.md`.

## Ferramentas entregues

| Requisito documentado | Implementação | Verificação |
| --- | --- | --- |
| Plugin novo | ID `shardbound_studio`, arquivo único e formato próprios | Carregamento real 5.2.2; alterações só na nova pasta |
| Pastas/cubos/pivôs, sem editor de skeleton | Grupos e ferramentas nativas; nós rígidos no GLB; painel de pivô identificado como pasta | Keyframes nativos e GLBs sem `skins` |
| Malhas especiais | Malhas nativas, UVs e materiais por face | Malha triangular com duas texturas exportada/validada |
| Textura e pintura | PNG, UV nativo, criação de tela/sprite e modo Pintar | Botões/diálogos reais; PNGs incorporados no GLB |
| Player com animações no mesmo projeto | Seis pastas corporais, clips nativos no projeto do player | Player animado, compilação e GLB validados |
| Skin Minecraft 64×64 | UV padrão, orientações de topo/base, camadas externas, braços de 4/3 pixels | Testes das duas variantes; importação, rejeição de tamanho inválido e Undo/Redo |
| Melee/ranged contextual | Player neutro autoral; modelo encaixado; corpo inteiro animável | Clips run/jump/attack/fire, transformações da arma e canais completos exportados |
| Flecha padrão compartilhada | `core:arrow` por padrão ranged, editável por ID | Preservação do ID no projeto/manifesto |
| Armadura sobreposta por partes | Seis pastas filhas do contexto corporal; offsets e animação própria | Matrizes reais acompanhando o corpo e GLB separado |
| Monstros sem rig obrigatório | Pastas/hierarquia arbitrárias | Monstro com mandíbula e outro monstro independente |
| Um ou vários modelos | Lista de modelos, raízes, IDs e GLB por objeto | Nove modelos no projeto de teste; nove GLBs validados |
| Blocos de cenário | Cubo inicial, grade/pivôs e autoria nativa | Bloco texturizado exportado; sem regras de mineração/gameplay |
| Partículas do zero | Formas, emissão, seed, velocidade, gravidade local, drag, sprite/atlas, tamanho/cor/alpha/rotação e curvas | Algoritmos determinísticos, geometria visual real e edição pela UI |
| Rastros | Ribbon de triângulos com histórico da trajetória, largura, alpha e curvas | Preview com geometria real, sincronizado à arma |
| Flash | Overlay aditivo da geometria-alvo, cor/alpha/curvas | Preview real sobre os cubos |
| Brilho/luz | Sprite aditivo mais `PointLight`, alcance/intensidade | Preview e objetos Three reais |
| Eventos na mesma timeline | Canal nativo combinado, lista, frame/tempo, alvo, offset, start/stop/burst, follow/detach | Eventos coincidentes, movimento individual, cópia, Undo/Redo, scrub e reprodução |
| Som em keyframes | WAV/OGG/MP3 incorporados, decodificação, volume/pitch/loop e cancelamento | WAV original realmente decodificado/reproduzido; arquivo inválido recusado |
| Projeto editável | `.sbstudio` com serialização nativa e metadados versionados | Salvar/reabrir preserva geometria, texturas, clips, sons e eventos |
| GLB + acompanhantes em um comando | Texturas incorporadas, clips, efeitos/áudio e quatro auxiliares | Khronos glTF Validator + JSON Schema + escrita real em diretório |
| Referência sem duplicar geometria corporal | Apenas ancestrais de transformação no GLB de equipamento; canais corporais auxiliares | Contagem dos meshes e seis canais + raiz |
| Exportação não destrutiva | Nova subpasta, sufixos, escrita exclusiva, destino escolhido | Duas exportações não sobrescrevem a primeira |
| Compatibilidade do editor | Canais Studio restritos ao seu formato; materiais/listeners restaurados | Generic Model e MTools carregados lado a lado; unload/reload sem perda de dados |

## Decisões técnicas concretizadas

- `.sbstudio` usa o formato de projeto nativo como base, com bytes de recursos incorporados.
- Pastas do contexto são referenciadas por UUIDs; renomear uma pasta não desfaz automaticamente o vínculo. Remover uma pasta necessária produz diagnóstico.
- Armas e armaduras ficam sob as pastas corporais correspondentes. A geometria do contexto é excluída da exportação do equipamento. Canais das seis partes e raiz estão explícitos nos auxiliares, com composição futura descrita no contrato.
- Clips usam valores numéricos e interpolação nativa amostrada. A autoria permanece na timeline familiar.
- Efeitos usam avaliação determinística por tempo; não são imagens ou contadores apresentados como simulação.
- Modelos têm OPAQUE/MASK; VFX usam alpha/aditivo em contrato separado, respeitando a limitação atual do importador estático.
- A exportação usa uma fotografia consistente do projeto e restaura seleção de clip, tempo e modo de visualização, inclusive em falha. Recursos Three temporários são descartados.
- Ao descarregar o plugin com projetos abertos, os descritores de dados necessários ao salvamento permanecem disponíveis. Recarregar reconecta o novo formato/controle aos projetos existentes.
- Não se gera `.sl`, registry ou `assets.json` por suposição; a fase de integração deverá decidir as declarações executáveis.

## Limitações reais

1. **Runtime/fidelidade:** o engine passou a carregar os auxiliares e executar animações/equipamentos/VFX/sons; veja [runtime e uso](../../../docs/shardbound-studio/05-RUNTIME-E-USO.md). A aparência **idêntica** ao ShardBound é uma exigência preservada para a fase B; não foi validada nem substituída por uma promessa de aproximação equivalente.
2. **Iluminação do preview:** os materiais nativos do Blockbench mantêm seu sistema de iluminação. Há luz pontual real, mas sua influência nesses shaders não equivale automaticamente ao renderer do jogo. Glow aditivo é visível; comparação luminosa final depende da fase B.
3. **Exportação de curvas:** GLB usa amostras lineares das curvas nativas, com FPS configurável. Step é amostrado próximo à descontinuidade com precisão Float32; curvas entre amostras não são declaradas analiticamente idênticas.
4. **Expressões e outros formatos:** não há importação/exportação GeckoLib. Molang dinâmico, lógica de IK e canais de outros runtimes não compõem o contrato Studio; conteúdo incompatível recebe diagnóstico.
5. **Áudio:** monitoração 2D no editor; UUID/attachment exportado para futura espacialização. WAV foi verificado em reprodução; disponibilidade de codecs OGG/MP3 depende da decodificação real do Chromium, verificada na importação. Combinações extremas de pitch e velocidade da timeline podem ultrapassar a faixa aceita pelo navegador.
6. **Plataformas:** testes executados em Windows com Blockbench Desktop 5.2.2. Não se declara teste em macOS/Linux, versões anteriores ou versão web. Exportação por diretório é Desktop.
7. **Performance:** limites explícitos de partículas, curvas, atlas, áudio e amostragem estão em `CONTRATOS.md`. A validação recusa excedentes em vez de truncar o asset.
8. **Efeito sem geometria:** um projeto/modelo do tipo efeitos pode ter um GLB contendo somente sua raiz; as funcionalidades de partículas, VFX e sons são representadas nos acompanhantes. Modelos de objetos exigem geometria antes de exportar.

Os exemplos são assets originais de demonstração e não substituem a criação livre. O plugin não produz gameplay automaticamente a partir de uma animação gráfica.
