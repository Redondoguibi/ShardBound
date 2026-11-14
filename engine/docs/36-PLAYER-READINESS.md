# Movimento, mods, configurações, diagnóstico e distribuição

Data: 2026-10-19. Instruções para o jogador: [PLAYING.md](PLAYING.md).

## Movimento e colisões

O motor horizontal continua usando o corpo dinâmico existente, preservando a integração com ECS, Jolt e os mods. A preparação de movimento usa varreduras do volume completo do corpo para tentar subir degraus: verifica teto, travessia horizontal e suporte de chegada antes de reposicionar. A descida acompanha suporte estático próximo somente quando o personagem já estava apoiado e não iniciou um salto. Paredes, quinas e passagens estreitas continuam sendo obstáculos físicos.

O mod `foundation` habilita degraus de 1,05 unidade nos mundos novos. `set_player_step_height(altura)` permite aos mods escolher de 0 a 2; zero desativa a subida/descida automática e permanece o padrão do motor para preservar mods e saves anteriores. O valor é opcional no save, validado antes da troca do mundo vivo. A inclinação transitável mantém o limiar de normal vertical 0,6, aproximadamente 53°. Em rampas mais íngremes, a velocidade horizontal dirigida não pode produzir um motor de escalada, inclusive durante o salto.

Corpos sob movimento controlado usam colisão contínua para evitar atravessar obstáculos finos em alta velocidade. A correção anterior de atrito lateral continua ativa; propriedades e comandos comuns de velocidade dos outros corpos mantêm seu comportamento.

`add_box(entidade, meia_extensao, dinamico, rotacao?)` aceita uma rotação física opcional em radianos. Três argumentos conservam as caixas alinhadas aos eixos anteriores, independentemente da rotação visual. A composição física segue a mesma ordem da renderização: roll, pitch e yaw. A rotação física é preservada no save; não é necessário alterar a ABI nativa.

## Gerenciamento de mods

`ModProfiles` mantém até 32 conjuntos nomeados, com IDs, versão do arquivo e perfil ativo. Mudanças usam publicação atômica antes da troca do estado em memória. Perfis anteriores com mods ausentes continuam editáveis; dados inválidos são preservados e o menu oferece o conjunto padrão em memória. Uma gravação posterior conserva a revisão anterior pelo mecanismo de backup.

O menu apresenta versões, dependências, disponibilidade e diagnósticos paginados. Ativação resolve dependências obrigatórias; desativação remove também dependentes. Perfis podem ser duplicados, renomeados e alternados. Dependências opcionais incompatíveis também são diagnosticadas quando ambos os pacotes estão selecionados.

Mundos novos capturam a seleção do perfil. Mundos antigos usam seus próprios IDs, sem incorporar silenciosamente mods recém-instalados. A seleção de um mundo fechado pode ser alterada com backup prévio, sem regenerar ou substituir sua definição congelada de terreno. A interface impede retirar recursos necessários a essa definição e recusa versões de saves declaradamente incompatíveis.

A compatibilidade da aplicação distingue mods ainda selecionados de dados órfãos retidos no checkpoint. Um pacote selecionado ausente continua sendo erro; um pacote explicitamente desativado pode ter suas entidades e estado preservados sem executar scripts. A checagem de versão usa o checkpoint como autoridade e a cadeia real de migrações é validada ao abrir. O contrato estrito de `SaveGame::checkCompatibility` não foi enfraquecido.

## Configurações e acessibilidade

Settings versão 2 lê os arquivos da versão 1 com defaults compatíveis. Valores, IDs de teclas, botões e limites das ações são validados antes da publicação. Preferências de teclado/mouse ficam separadas dos registros de input dos mods, de forma que um novo `bind` do mod não apague a personalização do jogador. A captura do clique de remapeamento é consumida para não reabrir o próprio botão.

O menu inclui restauração individual/geral, sensibilidade de câmera e escala uniforme de interface. Renderer, widgets, menus, ponte de ponteiro e projeções usam a mesma escala. O volume geral foi mantido; grupos reais do miniaudio acrescentam canais de efeitos, música e interface. `play_sound(id)` conserva o canal anterior e `play_sound(id, "music"|"interface"|"effects")` escolhe o grupo. Ganhos de canal afetam as vozes já associadas ao grupo.

## Diagnóstico ativável

O diagnóstico é configurável tanto no título quanto na pausa e permanece desligado por padrão. `FrameProfiler` limita a retenção a 600 quadros; o gráfico usa até 120. O relatório registra distribuições, contadores, memória, fases de execução e contexto da exportação. CPU/streaming/simulação fixa/apresentação são medidos separadamente; `draws` conta os desenhos de geometria, sem somar passes de sombra, pós-processamento e UI. Memória privada é consultada no máximo uma vez por segundo durante a coleta.

O renderer usa oito conjuntos de consultas de timestamp/disjoint do D3D11. Os resultados são consultados com `DONOTFLUSH`, sem esperar a GPU; se todos estiverem em uso, a coleta daquele render é omitida. Resultados inválidos/disjoint não se tornam números fictícios. A fila de resultados concluídos também é limitada, e desativar a opção libera as consultas. O painel é atualizado a cada 250 ms e convive com HUDs legados, widgets e console.

O JSON inclui o dispositivo D3D11 efetivo, CPU, quantidade de processadores lógicos, RAM, build e contexto `*_at_export`. WARP é identificado como software. As medições assíncronas de GPU não são pareadas com o quadro de CPU correspondente, e a janela pode atravessar mudanças de menu/configuração. O diagnóstico fornece medições para ensaios em outras máquinas; não comprova desempenho universal.

## Runtime distribuível e automação

O alvo de instalação `Runtime` inclui jogo, validador, DLLs transitivas, redistribuíveis MSVC, conteúdo, documentação, SDK e avisos de licença. O empacotador preserva saídas existentes, verifica execução com PATH restrito e exige criação/retomada reais do mundo antes de gerar o ZIP. Os arquivos extraídos são conferidos por SHA-256. Os testes de distribuição usam diretório próprio e não acessam os mundos pessoais.

Instalações novas usam LocalAppData; uma pasta `user` existente continua tendo prioridade. Há `--portable` e `--user-dir`, sem migração destrutiva ou perda da localização antiga. A automação do runtime compila e testa Debug/Release, preserva logs e publica artefatos de build no job. As dependências vcpkg seguem o baseline do manifest e o renderizador de software permite executar os testes em runners sem GPU física.

## Cobertura e limites

O teste `readiness` exercita os serviços e a própria `Application`: configurações antigas/novas, valores inválidos sem substituir o arquivo válido, nomes de teclas, prioridade de bindings, perfis persistentes, dependências, versões, dados de mod desativado/reativado, backup e terreno congelado, canais de áudio, estatística e limite de memória do profiler, clique real na opção de desempenho, remapeamento de mouse sem recaptura, sensibilidade efetiva, escala, pixels de HUD legado, timestamp real do D3D11, exportação e desligamento da coleta. Também cobre degraus, teto, descida, rampas transitáveis/íngremes, salto, corredor estreito, parede fina em alta velocidade, rotação combinada e persistência de colisores inclinados.

O teste `movement` continua verificando o bug de travamento nas paredes em quatro direções e três passos de simulação. A queda contra um bloco mantém as exigências de queda e apoio reais.

Permanecem: preparação de recursos na inicialização, ausência de hot reload/download de mods, avaliação de conflitos dinâmicos e migrações na abertura, limites de entidades/terreno existentes, ausência de rollback geral para efeitos arbitrários de scripts/DLLs e ausência de acessibilidade completa por leitor de tela/IME. Os testes automatizados e o PATH restrito não equivalem a uma instalação de Windows recém-formatada nem a horas de jogo em várias GPUs. A execução remota da CI depende de publicar as alterações.

## Resultado da validação — 2026-10-19

| Verificação | Resultado |
| --- | --- |
| Build MSVC Debug e suíte completa | 46/46; 104,78 s de testes |
| Build MSVC Release e suíte completa | 46/46; 43,89 s de testes |
| Regressões direcionadas Release | readiness, robustness, movement e gameplay: 4/4 |
| Integração das funcionalidades com WARP | readiness: 1/1; timestamps reais do renderizador de software |
| Pacote Release com PATH restrito e diretório externo | modcheck, criação e retomada: código de saída 0 |
| Coleta do executável distribuído | 119 quadros de gameplay e 116 amostras de GPU em cada execução |
| ZIP extraído | Inventário SHA-256 aprovado |
| Revisão visual | Configurações, controles, mods e diagnóstico inspecionados nas capturas reais |

Os dois builds finais não emitiram warnings ou erros do compilador. A GPU física usada na verificação do pacote foi Intel UHD Graphics 630; o teste WARP identificou `Microsoft Basic Render Driver` como software. Estes são ensaios de funcionamento, não benchmarks comparáveis entre máquinas.

As primeiras suítes completas revelaram duas falhas: a subida automática como novo padrão alterava o exemplo antigo de gameplay, e uma restauração sofreu acesso negado na renomeação de diretório. O padrão do motor foi preservado em zero, com habilitação explícita por `foundation`. A manutenção agora repete brevemente renomeações negadas por bloqueios de compartilhamento, com limite de 400 ms, sem sobrescrever destinos ou esconder erros permanentes. O teste de robustez segura um handle sem compartilhamento de exclusão e verifica a conclusão após liberá-lo; também verifica que um destino existente permanece intacto.

A revisão ainda corrigiu a varredura de um colisor dinâmico girado, que usava apenas translação. Uma passagem estreita com degrau agora verifica o volume orientado. Outros testes de integração cobrem a prioridade dos bindings do jogador, a captura de mouse sem recapturar o botão da UI, a preservação de HUD legado e dos dados de mods desativados/reativados.

Evidências, hashes e caminhos dos logs estão em [validation/player-readiness-2026-10-19.json](validation/player-readiness-2026-10-19.json). O pacote local gerado está em `dist/ShardBound-win64-2026-10-19.zip`, acompanhado de checksum e verificação. Não houve publicação ou execução remota da CI, sessão manual prolongada, teste em outras GPUs físicas ou VM limpa de Windows.
