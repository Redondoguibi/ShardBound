# Comparação das versões do bridge

Revisão de `a19ecc8` (local), `d7fe2c6` (GitHub) e do merge `18d1069`.
Critérios: comportamento útil para Generic Model, compatibilidade, preservação
do trabalho do usuário e testes executáveis. A origem da alteração não define
qual implementação deve prevalecer.

| Ponto | Versão local | Versão do GitHub | Decisão e justificativa |
| --- | --- | --- | --- |
| Instalação | Preservava credenciais; flags CLI e registro Codex | Escolhia porta livre; variáveis de ambiente e raízes de assets | Combinar: descobrir porta na primeira instalação, preservar pareamento nas seguintes e aceitar ambos os meios de configuração. |
| Diagnóstico | Consultava `status` pelo RPC | Verificava dependências e arquivos, mas apenas `/health` em execução | Combinar verificações de instalação com uma resposta real do controlador; `--offline` atende ao CI. O merge havia perdido a consulta RPC, recuperada nesta revisão. |
| Arquivos e formatos | Exports únicos em `outputs/`, bitmaps embutidos e GLTF estático | Listar, abrir, salvar em `assets/`, UVs de cubos e GLB | Manter os recursos novos e os exports reproduzíveis. Salvar sobre arquivo existente requer `overwrite: true`, alinhado à exportação. |
| Bridge Edge Loops | Até 32 cortes e twist inteiro entre -32 e 32 | Até 128 cortes e twist entre -256 e 256, inclusive fracionário | Combinar os limites maiores com twist inteiro: o MTools desloca índices de uma lista usando esse valor. Teste real com 64 cortes e twist 40. |
| MTools | 14 adaptadores com testes no editor e rollback | 19 adaptadores, com cobertura de schemas | Manter os 19 e ampliar os testes reais. Scatter deixava grupos vazios após Undo; o adaptador agora registra grupos nas duas fases da operação. |
| Fila e autenticação | Prazo inclui espera na fila; validação de segredos; desligamento explícito | Acrescentava comando interno para abrir fontes | Aplicar o mesmo prazo ao comando interno e manter autenticação e desligamento. |
| Animações em GLTF/GLB | Exportação estática explícita | Incluía animações automaticamente | Manter exportação estática documentada para o fluxo atual de Generic Model. Exportar animações exigiria uma opção explícita e testes com um projeto animado; não é uma garantia desta revisão. |

## Verificação

- 18 testes automatizados: schemas, OAuth, transporte, RPC do diagnóstico e
  operações de arquivo via cliente MCP. A gravação sem autorização de
  sobrescrita preservou o conteúdo anterior.
- Blockbench 5.2.1 com MTools 2.1.0 em perfil descartável: 19 adaptadores;
  as cinco ações acrescentadas também verificam Undo/Redo.
- Exportação BBModel, GLTF, GLB e OBJ; salvar/reabrir fonte; texturas e captura.
- Dois erros da tela inicial já existentes antes de carregar os plugins;
  nenhum erro novo durante a execução.

Os testes cobrem os cenários descritos; não equivalem a testar todos os
parâmetros possíveis ou a disponibilização das ferramentas no ChatGPT Desktop.

## Compatibilidade

`save_project` passa a usar `overwrite: false` por padrão. Clientes que
atualizam um arquivo existente precisam enviar `overwrite: true`.
O MTools original permanece intacto.
