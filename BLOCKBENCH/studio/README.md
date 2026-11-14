# ShardBound Studio 1.3.0

Plugin independente para **Blockbench Desktop 5.2.2**: modelos, skins, animações por pastas, partículas, rastros, efeitos de superfície, brilho/luz e sons na timeline nativa.

## Instalar ou atualizar

1. Salve seus projetos abertos.
2. Abra **Arquivo → Plugins** (ou F3 → Plugins).
3. Use **Load Plugin from File / Carregar plugin de arquivo** e selecione **dist/shardbound_studio.js**.
4. Se já estava instalado, substitua esse mesmo arquivo e **recarregue** o plugin no gerenciador. Confira a versão **1.3.0**.

Não precisa de Node.js, servidor ou MCP para usar o Studio. Mantenha o JavaScript numa pasta permanente. A exportação pode pedir permissão nativa de acesso à pasta escolhida.

## Onde cada coisa fica

| Trabalho | Lugar no Blockbench |
| --- | --- |
| Criar projeto | Arquivo → Novo → ShardBound Studio, ou cartão do formato na tela inicial |
| Salvar / salvar como | Arquivo → Salvar / Salvar como; Ctrl+S |
| Exportar GLB e recursos | Arquivo → Exportar → Pacote ShardBound (.glb + recursos) |
| Escala, ID, amostragem e projétil visual | Propriedades nativas do projeto |
| Cubos, malhas, pastas e marcadores | Ferramentas nativas e hierarquia |
| Declarar outro modelo independente | Menu de adição da hierarquia → Modelo independente |
| Nome/ID de um modelo | Menu de contexto de sua pasta → Propriedades do modelo |
| Vínculo de equipamento | Menu de contexto da pasta → Encaixe do equipamento |
| Pintura / UV / criar ou importar textura | Modo Pintar e painel Texturas nativos |
| Material | Menu de contexto da textura → Material ShardBound |
| Substituir skin | Arquivo → Importar ou menu da textura, quando existe player |
| Camada externa do player | Menu de contexto da pasta, quando existe player |
| Criar animação | Botão + da lista nativa de animações |
| Duração, loop, FPS | Propriedades nativas da animação |
| Partícula ou som no frame atual | Barra da timeline no modo Animar |
| Ajustar efeito / evento / som | Propriedades do keyframe selecionado |
| Reutilizar recursos | Menu Animação → Recursos de efeitos e sons; + do canal Studio |
| Diagnósticos | Validador nativo; exportação inválida abre seus resultados |

Não há painel Studio nem submenu Studio em Ferramentas. Abrir um monstro mantém a viewport, hierarquia, texturas e transformações normais. O monstro não recebe player de referência nem anatomia obrigatória.

## Modelar e animar

Escolha o conteúdo ao criar o projeto: player, item, arma corpo a corpo, arma ranged, armadura, monstro, bloco ou efeitos. IDs usam `namespace:path`, como `meumod:armas/espada`.

Player tem seis pastas corporais, skin 64×64, braços padrão/finos e camada externa. Equipamentos têm referência corporal editável para poses; modele dentro das pastas do equipamento. A referência não é duplicada no GLB da arma. Armadura começa vinculada às seis partes. Monstros permitem qualquer hierarquia. Use os controles nativos de posição, rotação, escala, pivô e keyframes.

Pastas internas pertencem ao mesmo objeto. **Modelo independente** declara outro objeto com ID e GLB próprios no mesmo projeto; não é necessário para adicionar uma pasta comum.

Crie clips pelo **+ nativo**. As propriedades oferecem nome, duração, loop e FPS. As seis partes do player e sua raiz podem ser animadas no mesmo projeto do equipamento.

## Criar efeitos vendo o resultado

1. No modo **Animar**, selecione um clip e posicione o cursor.
2. Selecione uma pasta e clique **Partícula neste frame** na timeline. O efeito e seu keyframe são criados imediatamente, com prévia na viewport. Para usar um marcador, escolha-o em **Origem** nas propriedades do evento; crie/posicione o marcador em Editar com as ferramentas nativas.
3. Nas **propriedades desse keyframe**, ajuste aparência, emissão, vida, velocidade, direção, gravidade, arrasto e rotação. Os parâmetros mostram a prévia durante a edição; confirmar o campo registra a alteração no Desfazer nativo.
4. Abra **Curvas ao longo da vida** para arrastar pontos de tamanho, opacidade, cor e rotação.
5. Em **Sprite e atlas**, escolha uma textura existente ou use **Pintar sprite**: abre uma tela transparente no modo Pintar nativo. Importe PNG normalmente pelo painel Texturas quando preferir.
6. Use **Tempo da timeline** para encerrar a prévia isolada ou reproduza a animação normalmente.

Você pode construir um clarão com um sprite grande, vida curta, emissão única e mistura aditiva. Não precisa escolher um preset de “flash”. Em **Avançado → Aplicação visual** ficam os mecanismos que realmente mudam a renderização: sprites emitidos, faixa ao longo do movimento, cor sobre a superfície e sprite com luz. A sobreposição de superfície exige alvo com geometria; rastros exigem origem. A mudança conserva parâmetros e valida os eventos que usam o recurso.

Origem, deslocamento e seguir/soltar são propriedades do evento. **Disparar uma vez**, **Iniciar** e **Parar instância** controlam a emissão/reprodução. Use a mesma instância no par iniciar/parar. Eventos coincidentes aparecem no seletor das propriedades; **Tempo do evento** move só o escolhido, enquanto arrastar o keyframe move os eventos nele contidos. Definições reutilizadas compartilham suas propriedades visuais.

## Áudio no frame

Posicione o cursor e use **Som neste frame…** na timeline para escolher WAV, OGG ou MP3. Também pode **arrastar arquivos sobre a timeline**: entram no tempo atual do cursor, sem diálogo separado de cadastro de recursos. O arquivo precisa ser decodificado de verdade; áudio inválido é recusado.

Selecione o keyframe para ajustar volume, velocidade/pitch, repetição ou substituir o arquivo. Importar e inserir, ou substituir o áudio de um evento, constitui uma única operação de Desfazer. A reprodução toca o áudio sincronizado; arrastar o cursor recalcula VFX sem tocar repetidamente os sons.

## Salvar e exportar

**Salvar / Salvar como** preservam geometria, pixels, UV, animações, eventos e bytes de áudio em **.sbstudio**. Reabra por Arquivo → Abrir modelo. O formato usa a serialização nativa com metadados versionados. Não há conversão automática de Generic Model: importe/copie a geometria para um projeto Studio quando necessário.

**Arquivo → Exportar → Pacote ShardBound (.glb + recursos)** cria uma pasta nova contendo GLB por modelo, sprites, áudio e quatro acompanhantes: `studio.manifest.json`, `studio.effects.json`, `studio.events.json` e `studio.player-animations.json`. Pacotes anteriores são preservados. O validador aponta recursos removidos, parâmetros incompatíveis, UVs e geometria sem modelo declarado.

Armas ranged declaram `core:arrow` por padrão nas propriedades do projeto; o jogo já registra esse visual compartilhado.

Projetos anteriores mantêm o contrato v1. Templates antigos com UV 32×32 incorreto recebem correção para 64×64 somente quando toda a geometria e UV ainda correspondem exatamente ao template gerado; a correção aceita Desfazer. UVs personalizados não são reescritos.

## ChatGPT / Codex

O Studio funciona sozinho. Para controlá-lo pelo **ShardBound Blockbench**, carregue também a **Bridge 0.5+**, faça o pareamento e siga o [guia de conexão](../bridge/STUDIO-COMPATIBILITY.md). A API Studio continua v1. Esta revisão da interface não exige cadastrar outro plugin de conta. Disponibilidade das ferramentas em uma conversa depende também da conexão/exposição pelo host ChatGPT; os testes locais não certificam uma sessão pessoal do Chat.

## Evidências e limites reais

- [Revisão dos problemas de interface](docs/REVISAO-UX.md)
- [Testes e relatório](docs/TESTES.md)
- [Cobertura e limitações](docs/IMPLEMENTACAO.md)
- [Contratos](docs/CONTRATOS.md)
- [Exemplos editáveis](examples/README.md)

O runtime do jogo consome os contratos v1; veja [instalação e uso no ShardBound](../../docs/shardbound-studio/05-RUNTIME-E-USO.md). O preview usa áudio 2D e materiais do Blockbench; PointLight não garante equivalência luminosa com o jogo. Curvas GLB são amostradas. Molang dinâmico, IK e formatos de outros runtimes não fazem parte do contrato de exportação. Testado em Windows/Blockbench Desktop 5.2.2; não certificado em outras plataformas.

## Desenvolvimento

Com Node.js 22+, nesta pasta: `npm ci`, `npm run build`, `npm test`, `npm run test:blockbench`, `npm run package`.

A integração usa uma instância separada com perfil temporário; não altera o modelo pessoal aberto. `BLOCKBENCH_EXE` permite informar o executável. O pacote contém JavaScript, fontes, exemplos, schema e documentos, sem perfis ou segredos.
