# Extensões da engine — implementação

Data: 2026-10-19. Contrato e exemplos em [modding/12-ENGINE-EXTENSIONS.md](modding/12-ENGINE-EXTENSIONS.md).

## Implementação

`EnginePipeline` é um registro por sessão com fases fixas, movimento, física, atualização, streaming e render. Sua integração está na mesma `Application` usada pelo executável e pelos testes. Mods existentes mantêm o caminho padrão quando não registram extensões. O menu, apresentação, autosave e manutenção do host permanecem gerenciados pelo núcleo.

O registro oferece observadores antes/depois e provedores de substituição explícitos, propriedade por namespace, negociação de API, prioridades determinísticas, dependência para sobrepor outro mod, fotografias de despacho, ativação/remoção e limites de 32 extensões/mod e 256/sessão. Históricos de falha são limitados a 64 registros; custos CPU são acumulados por extensão. F3 e o relatório de desempenho expõem o estado.

Os provedores podem delegar ao comportamento padrão uma vez, com parâmetros validados. A validação é separada do início da execução para que um parâmetro inválido não impeça fallback. Reentrância e delegação repetida são recusadas. Falhas de callbacks desativam somente a extensão; falhas do núcleo não são convertidas em sucesso. Efeitos já realizados por scripts não têm rollback geral.

`EngineBridge` publica as mesmas operações por scripts e pela ABI C nativa existente, sem modificar o layout de `SbHost`/`SbPlugin`. A descoberta de dependências também é registrada para pacotes exclusivamente nativos. Configurações usam proveniência de campos e exigem dependência explícita para conflitos; alterações independentes em campos de objetos são permitidas.

`EngineServices` valida gravidade/inclinação, ambiente e luzes, câmera/parâmetros de fase e ordem fixa. A publicação visual recebe uma lista preparada, valida o orçamento também na fronteira do renderer e usa movimento de vetor para evitar alocação depois da troca do ambiente. A física mantém as mesmas regras durante a preparação transacional de um load; não foi necessário alterar o formato de saves. Configurações de mods são reconstruídas por sessão e podem ser persistidas pelo estado já existente do mod.

## Verificações e limites

O novo teste `engine_api` cobre ordenação, autoria, conflito, versões inválidas, limites, observadores defeituosos, delegação única, fallback, exceção do núcleo, reentrância, mudanças durante despacho, orçamento de script, configurações inválidas sem publicação parcial e proveniência de campos independentes. O teste gráfico compara pixels de uma superfície sem/com luz configurada pela API.

A integração abre um mundo real, carrega uma DLL ABI 1, registra provedores por scripts e nativo, exerce as seis fases e verifica a ordem efetiva movimento → física → callbacks fixos. Verifica submissão de cena controlada pelo mod, regras físicas após save/load, preservação dos mods no save e limpeza de extensões/luzes ao fechar. As suítes anteriores continuam verificando compatibilidade com mods, saves, shaders e movimento.

O ciclo de revisão corrigiu a publicação antes de validação, a identificação de exceções sem mensagem, o limite de luzes na fronteira C++, conflitos indevidos entre campos visuais independentes e a preservação da cena atrás da pausa. Um teste específico reproduziu corpos flutuando após sair de gravidade zero: a alteração de gravidade agora acorda corpos dinâmicos em repouso. O teste de orçamento exige o diagnóstico `SCRIPT304`, evitando aceitar uma falha de sintaxe como prova de limitação de execução.

O backend D3D11/Jolt/SDL, o ciclo da janela e os serviços essenciais de persistência não são substituídos binariamente por esta API. Renderização continua usando os contratos de assets e shaders existentes; o mod pode mudar etapas, câmera, luzes, filtros e comportamento, sem receber ponteiros da GPU. DLLs continuam confiáveis e em processo; crashes nativos e efeitos arbitrários não recebem isolamento de memória. Não houve teste em Windows limpo, outra GPU física ou execução remota da CI.

## Resultado final

| Verificação | Resultado |
| --- | --- |
| MSVC Release, suíte completa | 48/48; 35,05 s de testes |
| MSVC Debug, suíte completa | 48/48; 113,80 s de testes |
| WARP: engine_api, readiness_review e movement | 3/3; 5,09 s |
| Exemplo público low_gravity | modcheck válido, três mods/dez scripts; execução de 80 quadros sem erro |
| Callbacks do exemplo no relatório real | iluminação: 80; movimento: 54; ambos ativos e zero erros |
| Pacote com PATH restrito e diretório externo | modcheck, criação e retomada: saída 0; ZIP extraído com hashes corretos |
| Compilador e whitespace | Nenhum warning/erro de compilação nos builds finais; diff validado com CRLF permitido |

O pacote gerado é `dist/ShardBound-win64-2026-10-19-engine-api1.zip`, com guia, SDK e exemplos. Builds anteriores foram preservados. Evidências, logs e hashes estão em [validation/engine-api-2026-10-19.json](validation/engine-api-2026-10-19.json). Estes resultados cobrem os contratos implementados e os testes descritos, sem demonstrar ausência de falhas em toda a engine.
