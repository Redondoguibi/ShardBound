# 05 — Decisões em aberto e perguntas ao autor

Este documento registra perguntas **ainda não respondidas**. A ausência de resposta **não** autoriza impor outra experiência de animação ou retirar tipos de conteúdo já pedidos.

## Decisões já fechadas — NÃO perguntar novamente

- Quer plugin **para Blockbench**, especializado em conteúdo do ShardBound.
- Quer modelar e animar com **pastas/grupos, cubos, pivôs e timeline**, no estilo Minecraft com GeckoLib.
- **Não quer** workflow separado de criação de ossos, skinning/pintura de pesos ou substituição por rigging típico do Blender.
- Quer criar **skins de player, itens, blocos, armaduras, monstros, armas melee, armas ranged, partículas e efeitos**.
- Quer **efeitos/partículas no mesmo projeto e sessão da animação do modelo**, em frames escolhidos.
- GLB/GLTF com texturas embutidas é a direção geral já estabelecida para modelos no ShardBound.
- A direção artística principal é cubos/cuboids e pixel-art.

## Prioridade alta — perguntas para fechar primeiro

**Q1 — Escopo de dados de gameplay:** ao escolher "monstro" ou "arma", o plugin deve apenas preparar **modelo + texturas + animações + efeitos + metadados visuais**, ou deve também oferecer formulários para definir dano, velocidade, vida, projéteis, comportamento etc., gerando parte dos arquivos `.sl`?

**Q2 — Criação de partículas:** você quer criar o efeito do zero **dentro do Blockbench** (textura, cor, velocidade, quantidade etc.) ou inicialmente selecionar efeitos prontos em uma biblioteca do ShardBound, com edição avançada em fase posterior? A autoria integrada e o keyframe na mesma timeline são requisitos de qualquer modo.

**Q3 — Forma da exportação:** um clique que gera `.glb` e arquivos auxiliares de efeitos/eventos (invisíveis para o fluxo cotidiano) atende ao requisito de "exportar partículas dentro do modelo", ou o arquivo final precisa ser literalmente **um único `.glb`** com dados customizados embutidos?

**Q4 — Skin:** a skin deve importar/exportar **PNG 64×64 compatível com skin clássica do Minecraft**, incluindo camada externa e variante de braços, ou será um formato visual próprio do ShardBound com um template padrão diferente?

## Prioridade média — podem ser fechadas após protótipos

**Q5 — Templates/variantes:** os tipos do menu inicial devem ser projeto separado por tipo (Skin, Monstro, Bloco, etc.) ou o mesmo projeto deve poder mudar de categoria sem recomeçar?

**Q6 — Exportação para mod:** o autor escolhe uma pasta de mod, e o plugin instala/atualiza arquivos nela automaticamente, ou somente gera uma pasta exportável para copiar manualmente? Como prevenir overwrite de `.sl` customizado?

**Q7 — Previews de efeitos:** é exigida fidelidade visual muito próxima do runtime ShardBound dentro do Blockbench desde a primeira versão, ou preview aproximado seguido de teste in-game é aceitável no início?

**Q8 — Integração com exportador existente:** vale evoluir o fluxo anterior do Blockbench registrado no repositório ou criar um plugin independente, desde que reaproveite partes que funcionem? Isso requer inspeção técnica do código atual.

## Decisões técnicas a validar sem transferir ao autor

- Mapeamento grupos -> nós glTF/transformações rígidas, escala e hierarquia.
- Suporte de interpolação, clips e keyframes de efeito na API específica do Blockbench.
- Modelo de identificação de eventos, looping, reentrância, pausa e cancelamento.
- Representação física dos VFX: sistema de partículas nativo e metadados separados ou embutidos.
- Versionamento de schemas, cache, erros e integração com registries `namespace:path`.
- Compatibilidade de rigs/grupos quando peças de armadura seguem player e monstros distintos.
- Implementação real de animação e emissão VFX no ShardBound, hoje não concluída segundo a documentação existente.

## Encerramento desta rodada

Após o autor responder Q1–Q4, atualizar estes arquivos com as respostas marcadas **Definido** antes de iniciar o plugin. Q5–Q8 podem ser priorizadas junto ao primeiro protótipo, sem perder as decisões.
