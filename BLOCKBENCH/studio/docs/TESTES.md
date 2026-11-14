# Testes

Verificação feita com **Blockbench Desktop 5.2.2.0 instalado no Windows**, carregando o JavaScript compilado pelo mecanismo nativo de plugins. O processo de teste usa perfil exclusivo e janela separada; encerra somente o processo que criou. Nenhum teste usa o modelo pessoal aberto como fixture.

**Resultado de 12/10/2026:** 9 testes de domínio/contrato aprovados; integração completa aprovada; **11 GLBs com zero erros e zero avisos**; **12 acompanhantes JSON validados**; zero erros do plugin/shaders. [Relatório verificável e SHA256 do plugin testado](TEST-RESULT.json). [Captura real do exemplo](screenshots/crystal-sword.png).

## Reproduzir

```powershell
npm ci
npm run build
npm test
npm run test:blockbench
```

`BLOCKBENCH_EXE` pode indicar o executável 5.2.2. `npm test` cobre os algoritmos puros e contratos. `test:blockbench` executa o plugin no renderer real, incluindo UI, edição nativa, áudio, exportador e filesystem. Não usa mocks para considerar uma funcionalidade entregue. O teste de recuperação injeta uma falha deliberada no exportador, exclusivamente para verificar limpeza e restauração após erro.

## Verificações automatizadas

**Interface 1.3.0 integrada aos espaços nativos:** criação de monstro preserva o espaço de trabalho e não cria referência de player nem oferece comandos de skin/encaixe. Não há painel Studio, submenu Studio em Ferramentas ou ação redundante de Novo. O teste clica na timeline para criar partículas, ajusta parâmetros com prévia antes do commit, usa Desfazer/Refazer e curvas nas propriedades nativas do keyframe e chega ao modo Pintar diretamente. Material usa a textura do menu de contexto. Propriedades do projeto incluem os campos Studio. [Espaço de trabalho](screenshots/monster-native-workspace.png) e [edição visual real](screenshots/studio-live-effect.png).

**Regressões de autoria nativa:** + da lista de animações abre suas propriedades nativas com duração editável e sem relógios Molang incompatíveis. Partícula e evento são desfeitos juntos. Origem aceita locator; prévia isolada respeita o deslocamento do evento. Alterar aplicação visual atualiza os eventos e recusa superfície sem geometria sem escrever parcialmente. Importar/inserir e substituir áudio realmente decodificado são operações únicas de Desfazer. Cancelar o seletor não cria evento. O callback registrado de arraste usa o cursor atual e decodifica bytes WAV reais. O campo de tempo move um evento. Salvar como seleciona extensão .sbstudio e escreve arquivo real, seguido do Salvar nativo. Reabrir template legado corrige UV64 com Desfazer/Refazer; UV personalizado permanece intacto.

Os testes de seletor de áudio e Salvar como interceptam **somente a fronteira com o diálogo do Windows**; não automatizam os diálogos do sistema nem um arraste físico do Explorer. Decodificação, eventos, desfazer, serialização, escrita em disco e callbacks registrados são reais. Essa distinção não é usada para afirmar que um diálogo do sistema foi testado visualmente.

**9 testes de domínio/contrato:** IDs e segurança de caminhos, versões, parâmetros de todos os tipos de efeito, partículas determinísticas/gravidade/emissão, curvas/gradientes/atlas, ordem e cruzamento de eventos/loops/start-stop, vida de rastros e impulsos, layout e variantes da skin, serialização determinística e validação JSON Schema.

**Integração no Blockbench real:**

- Carregar plugin; criar player; animar pasta na timeline; compilar projeto editável.
- Exportar player com animação e textura incorporada; restaurar tempo e clip selecionados.
- Criar ranged e melee com referência slim, armadura de seis regiões, dois monstros independentes, item com malha, bloco, player e modelo de efeitos no mesmo projeto.
- Animar as seis regiões corporais, raiz, mandíbula, escala e grupo adicional de armadura em run/jump/attack/fire.
- Avaliar partículas, rastro com geometria, flash e brilho com luz no preview real.
- Editar um evento coincidente sem mover os outros; Undo/Redo nativos; copiar evento e distinguir suas instâncias.
- Decodificar e reproduzir um WAV original; recusar bytes de áudio inválidos.
- Salvar/reabrir e comparar todos os metadados, eventos, nomes dos clips e quantidade de elementos.
- Exportar nove GLBs independentes; verificar ausência de geometria do player na espada; arquivos de sprite/áudio e seis canais corporais + raiz.
- Clicar em **Partícula neste frame**, editar parâmetros e curva visual no keyframe e criar sprite diretamente no modo Pintar.
- Usar o + da timeline para abrir a escolha de recurso; cancelar sem criar evento vazio.
- Undo/Redo da criação de sprite preserva seus pixels; criar/vincular contexto e recriar clips também restaura os canais corretos.
- Exportar duas vezes ao mesmo diretório, comprovando novos destinos e preservação do pacote anterior.
- Importar skin 64×64, Undo/Redo dos pixels e rejeição de tamanho incorreto.
- Rejeitar versão desconhecida de projeto antes de trocar a aba.
- Falhar durante exportação e verificar liberação de estado de exportação/suspensão.
- Descarregar/recarregar mantendo metadados e keyframes serializáveis em projetos abertos.
- Carregar o MTools fornecido no repositório e alternar para Generic Model, conferindo exatamente os três canais nativos originais.
- Gerar exemplo de espada com duas pastas, clips de idle/run/jump/attack, VFX e som; reproduzir pela timeline nativa com áudio sincronizado.
- Atravessar o evento de áudio com salto de tempo maior que sua duração; comprovar disparo único e stop de um burst de áudio em repetição, sem erros de cancelamento.
- Conferir os samples GLB de escala e os dois lados da interpolação step contra o comportamento nativo; exportar a partir do modo UV e restaurá-lo.
- Abrir o assistente Studio pela entrada nativa do formato.

**Validadores externos:** cada GLB é conferido pelo Khronos `gltf-validator`; cada acompanhante é conferido com Ajv contra `schemas/studio-v1.schema.json`. Os casos completos produzem **11 GLBs**: player, nove modelos do laboratório e espada do exemplo. Testes exigem zero erros de GLB, imagens incorporadas e ausência de materiais BLEND/skins. Avisos informativos de nós/tracks não usados podem ocorrer em modelos intencionalmente sem geometria; não são ocultados nos relatórios.

## Evidências

Cada execução salva em `.local/e2e/<timestamp>/`:

- projetos `.sbstudio` de player, laboratório e espada;
- pacotes exportados e relatórios `.glb.validation.json`;
- capturas do player, efeitos, pintura do sprite e exemplo da espada;
- formulário real de UI inspecionado;
- `errors.json` com fase, mensagem e stack.

O teste exige **zero erros do plugin e zero erros de compilação de shader**. A página inicial do próprio Blockbench 5.2.2 pode emitir dois erros anteriores ao carregamento do plugin ao atualizar o painel de notícias/resizers; são registrados separadamente na fase `startup`. Eles foram rastreados ao código instalado de `start_screen.ts`, não suprimidos como erros do Studio.

O teste de MTools comprova carregamento e coexistência. Não certifica cada operador de terceiros. A importação de skins, partículas e materiais também foi inspecionada nas capturas; equivalência visual com o jogo e testes de importador/runtime permanecem na fase B.

O MTools é lido da pasta irmã fornecida no repositório. Ele não é redistribuído no ZIP; em uma extração independente sem essa pasta, apenas essa verificação externa é marcada como não executada. Os testes do Studio e do Generic Model continuam.

## Pacote

`npm run package` recompila e cria `dist/ShardBound-Studio-1.3.0.zip` com JavaScript instalável, fonte, documentos, schema e exemplos. Usa lista explícita de arquivos; não inclui `node_modules`, `.local`, perfis, segredos ou o MCP anterior. O pacote contém SHA256 do JavaScript.

A distribuição foi aberta e conferida após gerar o ZIP: o JavaScript empacotado tem o mesmo SHA256 do plugin aprovado nos testes, e os diretórios/arquivos privados estão ausentes.

## Compatibilidade MCP 1.3

A Bridge 0.5 foi testada com Studio 1.3 no Blockbench 5.2.2 por meio do SDK MCP, daemon e WebSocket reais, com 34 ferramentas descobertas e 32 exercitadas, incluindo as 16 operações Studio. Veja [guia e relatório da integração](../../bridge/STUDIO-COMPATIBILITY.md). Os relatórios registram o hash exato do plugin.
