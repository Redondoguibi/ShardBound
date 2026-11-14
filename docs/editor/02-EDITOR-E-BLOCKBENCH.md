# Editor de mapas e Blockbench

**Estado de 13 de novembro de 2026.** Planejamento da primeira versão. A base técnica proposta é um executável próprio em C++20 para Windows, no repositório do ShardBound, reaproveitando o runtime, o renderer D3D11 e o pipeline de assets.

## Implementação disponível

Abra `shardbound_editor.exe` ao lado de `game.exe`. A aba **Início** permite criar ou abrir mapas e acessar **Blocos**, **Equipamentos**, **Blockbench**, **Assistente IA** e **Configurações**. Clique em **+** para abrir outra aba de início sem sair do projeto em uso. Cada mapa ganha sua própria aba, com câmera, camada, seleção de blocos, linha do tempo e revisão de mods independentes. Um ponto ao lado do nome sinaliza alterações pendentes. Ao fechar um mapa alterado, o editor oferece salvar, descartar ou cancelar.

Arraste uma aba pela faixa superior para mudar sua posição. Clique no **×** ou com o botão do meio para fechá-la. No editor do ShardBound, **Ctrl+T** cria uma aba, **Ctrl+Tab** alterna entre abas e **Ctrl+W** fecha a atual. No mapa, alterne o painel entre blocos, colocação e histórico. O editor carrega `content/` por padrão. Passe `--mods DIRETORIO` para carregar outro conjunto de mods.

Escolha **Planta** ou **3D** acima do mapa e ajuste a camada com **−** e **+**. Arraste com o botão esquerdo para girar a câmera em 3D. Arraste com o botão direito para mover a vista e use a roda para aproximar ou afastar, tanto em **Planta** quanto em **3D**. **Enquadrar** volta à vista geral. Um clique esquerdo usa a ferramenta selecionada. Para apagar, selecione **Apagar** e clique no bloco. Use **Shift + clique** para definir o ponto inicial. **Desfazer**, **Refazer** e a linha do tempo continuam disponíveis depois de salvar e reabrir o mapa.

Em **Projeto → Importar região de um save…**, preencha X, Z, largura e profundidade e escolha o `checkpoint.json` da partida. As edições salvas e os blocos do terreno entram no recorte. **Testar no jogo** abre uma partida de teste em diretório temporário. A lista de modelos permite colocar um modelo registrado com rotação e escala.

**Projeto → Exportar mapa e mods…** cria uma pasta com o mod que substitui o mapa oficial e uma cópia dos mods da revisão. O editor valida a pasta antes de publicar. Abra o pacote com `game.exe --mods "CAMINHO_DA_PASTA"` e inicie uma nova partida. O mapa exportado preserva as edições feitas sobre estruturas que já estavam no catálogo de colocações da revisão.

O editor usa a versão web oficial do Blockbench dentro do ShardBound. Ao trocar de aba, o projeto de modelagem permanece aberto. A integração usa o Microsoft Edge WebView2 e precisa de internet para carregar. A aba **Blockbench** em **Início** também pode ser usada para abrir ou editar um `.bbmodel` avulso.

### Criar um bloco

1. No mapa, clique em **+ Novo** na paleta. A partir de **Início**, entre em **Blocos → Criar bloco**. O editor usa o mapa atual e abre o mapa de exemplo se ainda não houver um mapa aberto.
2. O Blockbench abre em uma aba com um **Modelo genérico**. O cubo inicial tem 16 unidades de lado, base em Y = 0 e uma área de pintura de 16 × 16 por face. Edite os cubos ou as malhas no modo **Edit** e pinte no modo **Paint**.
3. Dê um nome ao bloco e use **Salvar na engine** ou **Ctrl+S**. O editor registra o bloco na revisão do mapa e guarda o projeto `.bbmodel` com as texturas. O controle **Sólido** usa colisão de célula inteira. **Sem colisão** permite atravessar o bloco.
4. Clique em **Ver no mapa** para voltar com o bloco selecionado. Clique no terreno para colocá-lo e use **3D** para ver a forma e a pintura. Para editar novamente, selecione o bloco na paleta e clique em **Editar**. Ao salvar de novo, os blocos já colocados passam a usar a nova versão. Salve o mapa para conservar a revisão ao reabrir.

O **Farol de Cobre** é um exemplo feito por esse fluxo. A pintura cobre uma face de cobre e o modelo tem um topo e uma base menores. Para um GLB que já existe, use **Blocos → Importar GLB…**.

Os plugins ShardBound Studio e ponte local continuam no Blockbench Desktop, acessível em **Configurações**. Na autoria de equipamento, **Criar clipes no Blockbench** prepara os clipes e **Exportar do Blockbench** importa o pacote exportado pela ponte do Studio. Para atualizar um pacote já publicado pelo editor, deixe o projeto correspondente ativo no Blockbench Desktop, escolha um de seus modelos e use **Atualizar no Blockbench**.

**Registrar seleção como estrutura…** usa dois cantos e as camadas selecionadas para cadastrar uma construção como mod. O cadastro não coloca a estrutura no jogo. Use a lista de estruturas para colocá-la onde quiser.

Cadastros de blocos, estruturas e equipamentos entram na revisão do projeto e podem ser desfeitos. **Projeto → Exportar mods da revisão…** copia o conjunto selecionado para uma pasta de distribuição. O arquivo do mapa guarda o caminho do conjunto de mods usado. Revisões locais ficam em `%LOCALAPPDATA%\ShardBound\Editor\Revisions`. O editor ainda lê arquivos `.editor.json` de saves antigos. Use **Exportar mapa…** para levar o mapa e os mods para outra instalação.

Para criar um interior, salve um mapa de sala com o ponto inicial marcado com **Shift + clique** e a saída com **Ctrl + clique**. Abra o mapa exterior, clique em **Ligar interior…**, selecione o arquivo da sala e marque a entrada e o ponto de retorno. O vínculo incorpora uma cópia do mapa da sala e pode ser desfeito pela linha do tempo.

## Fluxo de autoria

1. Criar um projeto de mapa finito ou importar uma seleção de um save do ShardBound.
2. Editar o terreno na vista 3D. Usar a paleta de blocos registrados, criar definições de bloco, pintar texturas pixel art e posicionar modelos.
3. Registrar seleções como estruturas reutilizáveis. Posicionar as estruturas escolhidas, montar interiores e configurar portas e pontos de chegada e retorno.
4. Criar [itens e armaduras](04-ITENS-ARMADURAS-E-ANIMACOES.md) no editor visual. Vincular às armas os clipes produzidos no Blockbench e encaixar as peças de armadura nos grupos do corpo.
5. Validar, abrir o mapa no jogo para testar e exportar o conteúdo como mod.

O importador recorta uma área explícita de um save do ShardBound. Ele lê o checkpoint, combina terreno e edições salvas dentro da seleção e cria um mapa com os voxels resultantes. O save de origem é aberto em leitura. Se faltar um bloco, o editor informa o ID para que o autor possa carregar o mod correspondente.

## Modelagem com o aplicativo instalado

O botão **Editar no Blockbench** abre o projeto na janela do Blockbench instalado pelo autor. O ShardBound Studio fornece os fluxos de modelagem, pintura e animação. O bridge local permite abrir, salvar, inspecionar e exportar o projeto correto. A janela do editor de mapas continua disponível para posicionar e testar os assets.

Use **Atualizar modelo** para exportar o projeto ativo, validar os arquivos e atualizar a prévia. A última revisão válida fica disponível se a atualização falhar. O painel de IA usa a ponte local e, em instalações portáteis, permite selecionar o arquivo `server.mjs` da ponte.

## Aceite

O teste `shardbound_editor.exe --smoke-blockbench` abre o Blockbench integrado, pinta e modela o **Farol de Cobre**, salva pela ação do Blockbench, coloca o bloco no mapa, reabre o projeto e atualiza o mesmo bloco. O teste precisa de WebView2 e internet.
