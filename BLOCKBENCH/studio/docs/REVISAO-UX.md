# Revisão de interface — 1.3.0

O pedido foi rever a coerência do plugin inteiro, não apenas retirar o painel. Esta revisão usa os fluxos nativos do Blockbench 5.2.2 e mantém os contratos e recursos existentes.

| Problema encontrado | Correção |
| --- | --- |
| Painel permanente misturava trabalhos e ocupava a viewport | Removido; nenhum painel Studio é registrado |
| Submenu Ferramentas apenas transferia a mesma lista de comandos | Removido; ações distribuídas por contexto nativo |
| Comando Novo duplicava a entrada automática do formato | Apenas entrada nativa de formato em Arquivo/Novo e tela inicial |
| Salvar Studio competia com Salvar/Salvar como | Comandos nativos passam a salvar .sbstudio |
| Exportação também aparecia em Ferramentas | Pacote exclusivamente em Arquivo/Exportar |
| Criar clips tinha botão/formulário separado | + e propriedades nativas de animação; duração/FPS compatíveis com o contrato |
| Propriedades ofereciam relógios Molang incompatíveis | Ocultos somente nas propriedades de clips Studio; formatos externos preservados |
| Efeito exigia escolher tipo/ID antes de ver qualquer resultado | Partícula no frame cria recurso e keyframe imediatamente com prévia |
| Ajustes visuais dependiam de abrir/confirmar vários diálogos | Controles e curvas nas propriedades nativas do keyframe, prévia durante ajuste |
| “Flash” sugeria um preset obrigatório | Clarão pode ser criado com sprites livres; mecanismos de superfície/faixa/luz ficam em aplicação visual avançada |
| Trocar mecanismo visual deixava eventos com tipo antigo | Valida todos os usos antes da troca e atualiza eventos; erro não altera o recurso |
| Sprites exigiam configuração intermediária | Pintar sprite abre a textura editável diretamente no modo Pintar |
| Importar som era cadastro separado de inserir keyframe | Escolha no frame ou arraste sobre timeline; decodificação e inserção juntas |
| Importar/inserir ou substituir áudio exigia vários Desfazer | Uma operação nativa para cada importação/inserção ou substituição |
| Coincidência de eventos dificultava seleção e alteração individual | Seletor de eventos e campo de tempo individual nas propriedades |
| Material podia operar na textura selecionada em vez da clicada | Menu de contexto usa a textura que o usuário acionou |
| Monstros recebiam escolhas de braços/skin/equipamento sem contexto | Opções condicionadas ao conteúdo; anatomia livre preservada |
| Propriedades do projeto duplicadas em diálogo Studio | ID, escala, FPS de exportação e projétil nas propriedades nativas do projeto |
| Validação isolada em botão próprio | Verificações integradas ao validador nativo |
| Grande quantidade de campos podia expandir a janela e ocultar viewport | Seções recolhíveis e rolagem limitada à área de propriedades |
| Templates em projetos não-player tinham PNG64 e escala UV32 | Criação usa UV64; reparo com Desfazer só para layout original exato |
| Salvamento assíncrono podia marcar outra aba como salva | Callback mantém referência ao projeto que originou o salvamento |
| Desativar/recarregar podia deixar menus ou controles duplicados | Remoção de ações, extensões, handlers, listeners e restauração dos comandos nativos |

As verificações automatizadas e capturas reais estão em [TESTES.md](TESTES.md). Os testes não substituem avaliação de usabilidade por quem modela; a lista descreve problemas identificados e corrigidos, sem alegar ausência de qualquer problema futuro.

O arraste de áudio usa o **tempo atual do cursor**, não a posição horizontal do arquivo solto. Marcadores são criados/posicionados em Editar e escolhidos no campo **Origem** do evento; o Blockbench nativo não permite selecionar locators diretamente para animá-los. A prévia visual não certifica equivalência com o renderer do jogo; integração do runtime permanece na fase B.
