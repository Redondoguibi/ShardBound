# Espaço de trabalho do ShardBound

Atualizado em 14 de novembro de 2026. Este documento descreve recursos implementados no editor.

## Organização

A tela inicial separa projetos, conteúdo e ferramentas. **Criar mapa** e **Abrir projeto** ficam na área de projetos. Os cartões **Blocos**, **Equipamentos**, **Modelagem livre** e **Assistente IA** abrem suas respectivas áreas em abas. O Blockbench continua integrado para criar, pintar e salvar blocos, armas e armaduras. Armaduras não recebem animações próprias.

**Recentes** conserva os cinco últimos mapas abertos ou salvos, sem repetir o mesmo caminho. Um clique seleciona e dois cliques ou **Abrir selecionado** abrem o mapa. Um projeto já aberto é ativado em sua aba. Se o arquivo tiver sido movido, o editor informa o problema e permite localizá-lo por **Abrir projeto**. Em uma instalação nova, use **Criar mapa** para começar com o catálogo vazio.

**Configurações** e **Ajuda** ficam sempre acessíveis no canto superior direito. As abas mantêm reordenação por arraste, fechamento, indicadores de alterações e estados independentes dos mapas.

## Mapa e bibliotecas

Arraste a divisória à esquerda do painel de blocos, colocação e histórico para ajustar sua largura. O editor conserva essa preferência e limita a largura para evitar sobreposição das ferramentas em janelas menores. **Foco**, ou **F10**, oculta o painel lateral e amplia o mapa. Acione novamente para restaurá-lo.

A aba **Blocos** contém busca e biblioteca dos blocos registrados na revisão atual. **Editar** reabre modelos que possuem um projeto de autoria salvo na engine. A paleta do mapa usa o mesmo catálogo. Equipamentos criados pela engine aparecem na biblioteca da aba **Equipamentos**; quando vazia, ela explica como iniciar a criação.

Botões, campos e listas compartilham um tema escuro com destaque verde, estados de seleção e foco. Passe o mouse sobre as ferramentas do mapa para consultar sua função e atalho.

Na base da paleta, **Tamanho do pincel** oferece **1×1**, **3×3** e **5×5** para colocar ou apagar blocos na camada atual. O tamanho é independente por aba. Cada clique é uma única ação de desfazer/refazer, e o pincel respeita as bordas do mapa. Na vista **Planta**, o contorno indica as células afetadas; no preenchimento, mostra o retângulo entre o primeiro canto e o cursor. O rodapé mostra as coordenadas X, Y e Z sob o cursor. O tamanho também funciona em 3D, mas o contorno é mostrado somente na planta.

## Controles

| Ação | Controle |
| --- | --- |
| Girar a câmera em 3D | Arrastar com o botão esquerdo |
| Mover a câmera | Arrastar com o botão direito |
| Aproximar ou afastar | Roda do mouse |
| Usar a ferramenta | Clique esquerdo |
| Pincel / apagar / amostra / preencher área | B / R / I / G |
| Enquadrar o mapa | F |
| Cancelar seleção de área ou estrutura | Esc |
| Ocultar ou mostrar o painel lateral | F10 |
| Testar o mapa no jogo | F6 |
| Salvar / salvar como / abrir | Ctrl+S / Ctrl+Shift+S / Ctrl+O |
| Desfazer / refazer | Ctrl+Z / Ctrl+Y |
| Nova aba / fechar aba | Ctrl+T / Ctrl+W |
| Próxima aba / aba anterior | Ctrl+Tab / Ctrl+Shift+Tab |
| Configurações / ajuda | Ctrl+, / F1 |

Os atalhos de uma letra do mapa não interceptam digitação em campos de texto. O Blockbench preserva seus próprios controles de modelagem, pintura e animação.

## Preferências e assistente

Escolha diretamente nos menus a vista inicial do mapa, visibilidade da grade, velocidade de movimento e velocidade de zoom. Movimento e zoom são independentes. As opções são salvas automaticamente em `editor-settings.json`, no diretório de dados do usuário. **Restaurar preferências** restaura a visualização e largura do painel, preservando caminhos das integrações e projetos recentes.

A janela do assistente pode ser redimensionada ou maximizada. A atividade e a área de escrita aproveitam a largura adicional, enquanto a conexão permanece na coluna direita. Os controles de execução e aplicação ficam na parte inferior. O botão de fechamento passa a **Cancelar tarefa** durante a execução. Esta atualização não altera os fornecedores, as chaves nem as permissões de ações da IA.

Antes da primeira tarefa, a área de atividade apresenta os passos para conectar, executar e aplicar o rascunho. Ao usar Codex local, a coluna de conexão se ajusta para retirar o espaço reservado aos campos de chave de API.

## Verificação

O teste `map_editor_preview` cobre edição, câmera, histórico e abas, além de modo foco, arraste da divisória, ferramentas sem sobreposição na janela mínima, busca na biblioteca e reabertura de preferências e recentes. A persistência é testada em um diretório isolado, sem alterar preferências do usuário.

Também verifica pintura e apagamento com múltiplas células, desfazer/refazer, ausência de ações vazias, limites do mapa e tamanho de pincel separado por aba.

Os testes `--smoke-blockbench` e `--smoke-equipment` verificam criação, pintura, publicação e reabertura no editor integrado. Eles dependem do WebView2 e de acesso à internet.
