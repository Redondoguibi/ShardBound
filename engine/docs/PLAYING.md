# Jogar e configurar o ShardBound

## Abrir o jogo

Extraia o ZIP inteiro e abra `game.exe`. Mantenha as DLLs, `assets` e `content` junto do executável. O pacote inclui as dependências de execução e não precisa do Visual Studio ou do projeto-fonte.

Instalações novas guardam dados em `%LOCALAPPDATA%\ShardBound`. Se já existir uma pasta `user` junto do executável, ela continua sendo usada para preservar os mundos anteriores. `--portable` força o uso dessa pasta; `--user-dir PASTA` escolhe outro local e tem prioridade. Nenhuma dessas opções apaga ou move os saves existentes.

Na tela inicial, **Jogar** permite criar um mundo com nome e seed, ou continuar um mundo salvo. **Configurações** também está disponível na pausa, por Escape.

## Mods e perfis

Abra **Mods e perfis** na tela inicial para consultar os pacotes instalados, suas versões e dependências. É possível duplicar, renomear e alternar perfis. Ativar um mod inclui suas dependências obrigatórias; desligar uma dependência também desliga os mods que precisam dela. Dependências opcionais não são ativadas automaticamente.

O perfil escolhido determina os mods dos mundos novos. Um mundo existente continua usando sua seleção salva. Para alterá-la, entre em **Jogar → Mods**, na linha desse mundo, escolha o perfil e use **Aplicar ao mundo com backup**. O jogo cria o backup antes de publicar a seleção. Pacotes ainda exigidos pelas texturas, shaders e pelo gerador do terreno salvo não podem ser retirados dessa seleção.

Desativar um mod de entidades/gameplay conserva seus dados no save; eles podem voltar quando o mod for reativado. Isso não substitui a compatibilidade de versões declarada pelo autor. Problemas de dependências, preparação e abertura aparecem em **Ver diagnósticos de mods e dependências**. Conflitos que dependem da execução dos scripts são avaliados na abertura do mundo.

Os pacotes são descobertos na pasta `content` ao iniciar o jogo. Reinicie depois de instalar, remover ou atualizar arquivos de mods. Os recursos são preparados na inicialização; a seleção por mundo controla os scripts e extensões que serão executados. O gerenciador não baixa pacotes da internet. Extensões nativas mantêm a opção existente `--allow-native`.

## Controles, câmera e interface

Em **Configurações → Controles e interface**, clique numa ação e pressione sua nova tecla ou botão do mouse. Escape cancela a captura; Delete restaura o padrão daquela ação. **Restaurar controles** remove todas as personalizações. Ações registradas por mods também aparecem; as escolhas do jogador continuam tendo prioridade quando o mod registra novamente a mesma ação.

Remapear teclado ou mouse conserva os botões de gamepad já associados à ação. Uma configuração antiga que perdeu esse botão pode ser restaurada com Delete ou **Restaurar controles** antes de remapear novamente.

Escape, `/`, F3 e a combinação Page Up/Page Down mantêm suas funções de navegação e desenvolvimento. As ferramentas de câmera do modo de desenvolvedor têm seus próprios atalhos. A sensibilidade afeta a rotação das câmeras orbital e livre; a câmera normal continua fixa conforme o modo de jogo.

A escala da interface alterna entre 75%, 87,5% e 100%. O desenho e as áreas clicáveis acompanham a mesma escala. A configuração não modifica a resolução do mundo, que possui sua própria escala de renderização.

## Áudio

O volume geral fica na tela principal de configurações. **Canais de áudio** oferece ganhos separados para efeitos, música e interface, multiplicados pelo volume geral. Sons antigos de mods pertencem ao canal de efeitos. O mod precisa indicar outro canal quando quiser usar música ou interface; a separação não classifica arquivos de áudio automaticamente.

## Diagnóstico de desempenho

Ative **Configurações → Diagnóstico de desempenho** e volte ao jogo. O painel apresenta FPS, tempos de quadro, CPU, GPU, memória e um gráfico recente. O diagnóstico começa desligado e sua escolha fica salva.

**Exportar relatório de desempenho**, na mesma tela, grava um JSON na pasta `performance` dos dados do usuário. Sair do jogo com a coleta ligada também exporta um relatório. Desativar interrompe a coleta; os dados recentes continuam disponíveis para exportar. Ativar novamente inicia outra amostragem.

O relatório contém até 600 quadros recentes, percentis P50/P95/P99, médias e máximos, CPU, GPU, memória física e privada, custos de simulação fixa/streaming/apresentação e contadores de desenho de geometria. Cada quadro indica se estava em gameplay ou menu. Mods, mundo e configurações identificados por `*_at_export` representam o instante da exportação; a janela pode conter quadros anteriores a uma troca de tela ou configuração.

Para comparar máquinas, use o mesmo mundo, mods e configurações, execute um percurso semelhante e aguarde a janela se renovar antes de exportar. O tempo de quadro inclui a espera de apresentação/limite de FPS. O tempo de CPU mede trabalho na thread principal por relógio de parede, não porcentagem de utilização do processador. A medição de GPU é assíncrona, chega com atraso e não deve ser correlacionada diretamente à linha de CPU do mesmo quadro. Ausência de amostra aparece como `null`, nunca como um tempo inventado.

Para automação, `--performance` força a coleta nessa execução. `SHARDBOUND_RENDER_DRIVER=warp` seleciona o renderizador de software usado nos testes sem GPU dedicada; `hardware` é a seleção normal. Relatórios de WARP são identificados pelo nome do dispositivo e não representam o desempenho de uma GPU física.

## Distribuição e verificações

`tools/package.ps1` compila e testa Release, instala o runtime numa pasta nova, cria um inventário SHA-256 e verifica o pacote antes de produzir o ZIP. `-SkipBuild` serve para um build já validado. O verificador usa uma pasta de trabalho externa e um PATH restrito, valida os mods, cria um mundo, reabre seu checkpoint e exige relatórios de CPU/GPU. O ZIP também é extraído e conferido por hash.

O pacote inclui `modcheck.exe`, o SDK nativo e os avisos de licença das dependências. Os testes automatizados do jogo estão em `.github/workflows/game.yml`, para Debug e Release. A execução dessa automação no GitHub exige publicar as alterações; criá-la localmente não significa que ela já executou remotamente.
