# Shaders extensíveis por mods — implementação e validação

Data: 2026-10-18. Contrato público: [guia de shaders](modding/09-SHADERS.md).
Exemplo opcional: [shader_lab](../examples/shaders/README.md).

## Implementado

- Registro `assets.json → shaders`, com namespace, ABI 1 e tipos `surface` e
  `postprocess`. Compilação HLSL, reflexão dos bindings, compartilhamento dos
  programas GPU e limites de recursos. O `modcheck` compila os mesmos programas
  sem criar um dispositivo gráfico e valida referências nos blocos.
- Callbacks de superfície modificam RGBA, normal e emissão. O passe de sombras
  usa o mesmo callback para preservar recortes de alfa. Os estados existentes
  de iluminação, culling, transparência e instancing continuam ativos.
- Parâmetros dinâmicos: quatro `float4`, textura auxiliar, tempo da sessão e
  resolução interna. APIs de entidade e textura aplicam os programas a materiais
  existentes; blocos podem guardar o vínculo em `world.json` e no snapshot.
- Até oito passes de pós-processamento, ordenados por prioridade/ID, usando
  alternância de alvos. A interface é desenhada depois. Resize/render scale são
  respeitados. F3 informa passes ativos e `RenderStats.postDraws` conta draws.
- Saves de entidades preservam ID, parâmetros e textura auxiliar. Saves antigos
  carregam sem migração obrigatória. Recursos ausentes mantêm os dados e usam
  renderização padrão com diagnóstico; dados inválidos não trocam o mundo vivo.
- Diagnósticos com arquivo real, linha/coluna e recurso, inclusive em includes.
  Includes são limitados e contidos no VFS. A regra de textura identifica seu
  proprietário; outro mod não pode substituí-la silenciosamente. Passes de
  pós-processamento só podem ser atualizados pelo namespace que os criou.

## Correções de integração

1. A chave de lote do renderer passou a distinguir programa, parâmetros, textura
   auxiliar, modo alfa e overlay. Materiais incompatíveis não compartilham draw;
   instâncias equivalentes continuam agrupadas.
2. O caminho iluminado devolvia alfa 1. Agora materiais Blend/Additive preservam
   o alfa produzido pelo shader, mantendo o comportamento opaco e Mask.
3. O compilador atribuía erros ao arquivo combinado do engine. Diretivas `#line`
   por arquivo e um nome separado para a biblioteca do engine corrigem o diagnóstico.
4. A leitura dos parâmetros do save valida a faixa de float usada pela API,
   evitando aplicar aos shaders o limite mais estreito das coordenadas salvas.

## Validação final

| Configuração | CTest | Tempo observado | Avisos de compilação |
|---|---|---|---|
| Debug | 42/42 aprovados | 146,73 s | 0 |
| Release | 42/42 aprovados | 71,41 s | 0 |

Comandos: `./tools/build.ps1 -Preset debug` e
`./tools/build.ps1 -Preset release`, com paralelismo de compilação limitado.
As configurações rodaram simultaneamente em diretórios separados; esses tempos
são registros de execução, não benchmarks comparáveis de desempenho.
[Resultados individuais](validation/shaders-2026-10-18.json).
Logs locais: `build/shaders-debug.log` e `build/shaders-release.log`.

Os três testes novos são `shaders`, `shader_modcheck` e `shader_smoke`.
O primeiro usa uma janela oculta e a GPU real para conferir pixels: cores e
parâmetros por instância, agrupamento, textura auxiliar/tempo, alfa iluminado,
recorte em sombras, composição e ordenação dos passes, proteção da interface,
resize e resolução interna. Também cobre limites, conflitos, falhas de compilação,
includes inválidos/cíclicos, diagnósticos de includes, bindings não suportados,
APIs de script, rollback de alterações inválidas, saves antigos, recursos ausentes
que retornam e shaders em snapshots/publicação de chunks.

Os outros dois validam a compilação do exemplo pelo CLI e sua inicialização real
no jogo. Os 39 testes existentes também passaram, incluindo mundos procedurais,
plataforma de mods, equipamentos, Studio, interfaces e persistência.

Uma execução adicional do CLI com um shader deliberadamente inválido retornou
exit code 1, `valid: false`, `SHADER101`, namespace/recurso corretos e arquivo HLSL
real na linha 1. A verificação de whitespace do diff e os links novos passaram.
Não foi realizado playtest manual prolongado nem benchmark entre GPUs.

## Limites restantes

A extensão é de pixel/superfície e pós-processamento. Vertex/geometry/compute,
render graph arbitrário, HDR/depth para pós-processamento, editor visual e hot
reload de HLSL não fazem parte desta ABI. São 16 floats e uma textura adicional
por vínculo. O cache de programas é em memória; a compilação na preparação é
síncrona. Configuração global de passes/regras pertence à sessão e deve ser
reaplicada pelo mod, enquanto vínculos de entidades/blocos são persistidos.
Shaders HLSL podem ter custos arbitrários na GPU; os testes não estabelecem um
limite universal de frame time. Não restaram falhas nos testes executados.

## Evolução posterior de armazenamento e compatibilidade

Este relatório preserva a validação daquela etapa. O runtime ganhou índice externo, autosave em worker, preparação transacional e recuperação de mundos; os contratos atuais estão em [10-SAVES-COMPATIBILITY-AND-RECOVERY.md](modding/10-SAVES-COMPATIBILITY-AND-RECOVERY.md).
