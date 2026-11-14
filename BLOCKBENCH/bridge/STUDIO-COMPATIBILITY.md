# ShardBound Blockbench 0.5 + Studio 1.3

Integração para **Blockbench Desktop 5.2.2 no Windows**. São dois plugins nativos:
**ShardBound Studio** fornece o editor; **ShardBound Bridge** recebe as ferramentas
do ChatGPT/Codex. O plugin de conta **ShardBound Blockbench** mantém o mesmo ID e
a mesma conexão Secure MCP Tunnel, com a skill atualizada. O Generic Model e os
19 adaptadores MTools continuam disponíveis. Não há conversão automática de formato.

## Começar

1. Salve seus projetos no Blockbench.
2. Para preparar tudo/reiniciar a conexão, abra PowerShell em `BLOCKBENCH/bridge`:

   ```powershell
   powershell -NoProfile -ExecutionPolicy Bypass -File .\Update-Studio-Integration.ps1 -RestartConnection
   ```

   O script compila os dois plugins e preserva o pareamento e a chave do túnel
   existentes. Se este PC já tiver sido preparado, os arquivos abaixo já estarão
   prontos. Não é necessário criar outro servidor MCP nem outro plugin na conta.
3. No Blockbench, **Arquivo → Plugins**, carregue/recarregue os arquivos locais:
   - `BLOCKBENCH/studio/dist/shardbound_studio.js` — Studio **1.3.0**.
   - `BLOCKBENCH/bridge/.local/shardbound_bridge.js` — Bridge **0.5.0**, pareada.
   - MTools, opcional, para suas operações de malha.
4. **Arquivo → Novo → ShardBound Studio**, ou abra seu `.sbstudio`.
   Os projetos Studio 1.0 continuam compatíveis: o contrato de dados permanece v1.
5. No ChatGPT Desktop, abra uma **nova conversa normal**, selecione
   [ShardBound Blockbench](https://chatgpt.com/plugins/plugins_6abfe848600c8191876653a097dd8d53)
   e mantenha sua conexão ShardBound MCP ativa. Se a interface mostrar Atualizar
   no plugin instalado, aplique a atualização. No Codex, reinicie a conexão MCP
   ou abra nova conversa para descobrir as ferramentas novas.
6. Peça: “Inspecione meu projeto ShardBound Studio, mostre a captura real e leia
   os modelos, clips e efeitos. Depois crie uma animação de ataque com partículas
   e exporte o pacote completo.”

Após reiniciar o Windows, execute `Connect-ChatGPT.ps1` nesta pasta e abra o
Blockbench com os dois plugins. Não envie senhas/chaves no chat.

## Verificar a conexão

Peça `status`, `read_project`, `capture_view` e `studio_read` sem editar. O status
deve retornar `bridge_version: 0.5.0`, Studio `version: 1.3.0`, `api_version: 1`,
e formato `shardbound_studio`. O catálogo passa de 18 para **34 ferramentas**.
Se só aparecerem as 18 antigas, reinicie o runtime **e o daemon** com o script
de atualização, recarregue a Bridge no Blockbench e abra nova conversa.

Uma skill anexada não comprova acesso. Erros da conta/túnel (como HTTP 429) são
relatados separadamente dos testes locais. `Status-ChatGPT-Desktop-Plugin.ps1`
testa as camadas locais; só uma chamada real naquela conversa comprova o host.

## Cobertura

| Ferramenta nova | Autoria real |
| --- | --- |
| `studio_read`, `studio_validate` | Metadados, modelos/vínculos, clips/keyframes paginados, eventos, locators, recursos e erros |
| `studio_create_project`, `studio_project_settings` | Templates player/item/melee/ranged/bloco/armadura/monstro/efeito, escala, amostragem e flecha compartilhada |
| `studio_model`, `studio_locator` | Vários modelos independentes, encaixes nas partes do player e origens de efeitos |
| `studio_clip`, `studio_keyframes` | Timeline nativa, posição/rotação/escala, linear/step/Catmull-Rom/Bezier, edição em lote e Undo |
| `studio_effect` | Todos os parâmetros de partículas, atlas, curvas de vida, rastros, flash e brilho/luz |
| `studio_event` | Mesmo frame/timeline, alvo, offset, start/stop/burst, follow/detach e som |
| `studio_sound`, `studio_skin` | Arquivos reais em assets/, decodificação de áudio, PNG64×64, braços wide/slim e camadas |
| `studio_material`, `paint_texture` | MASK/OPAQUE, corte alpha, dois lados; pintura/apagamento de pixels e Undo nativo |
| `studio_timeline` | Seek/play/pause nativos, captura de VFX e reprodução sincronizada de áudio |
| `studio_export` | GLBs animados separados + quatro acompanhantes + sprites/áudio reais, sem corpo de referência duplicado |

As 18 ferramentas anteriores também reconhecem Studio quando aplicável. `update_elements`
ganha reparenting de grupos/cubos/malhas, com rejeição de ciclos. Raízes declaradas e
hierarquia do player são protegidas; use os encaixes Studio. As operações geométricas
do MTools continuam usando seleção explícita e Undo nativo.

## Arquivos, exportação e limites

`save_project` exige `.sbstudio` no Studio e `.bbmodel` no Generic, em `assets/`.
`open_project_file` aceita ambos, sem fechar outras abas. A fonte inclui bitmaps,
animações e recursos. `export_project` aceita snapshots editáveis; exportação GLB
de Studio deve usar `studio_export` para preservar o contrato completo.

O pacote é escrito numa pasta nova sob `outputs/`, ou sob o diretório `assets/`
solicitado. O caminho retornado é local ao PC. A transferência usa chunks de 1 MiB,
reserva exclusiva e arquivos `wx`; nunca sobrescreve mods/scripts. Erros conservam
o destino parcial identificado para diagnóstico. Não existe execução arbitrária
de JavaScript. Caminhos de importação/escrita rejeitam travessia e junctions fora
das raízes permitidas. Operações remotas são serializadas; cada alteração Studio
exige UUID do projeto e revisão atual. Releia após cada alteração/timeout.

Limites do MCP: projeto de 32 MiB; áudio de 24 MiB; PNG de skin de 4 MiB; pacote de 256 MiB e
10000 arquivos; transferência expira após 5 minutos ociosa. Use a exportação local
do Studio para pacotes maiores. Exportações podem levar até 3 minutos para compilar;
o snippet Codex novo usa 300 segundos. Limites do editor/preview são os documentados
no Studio. Integração do runtime do jogo e equivalência visual permanecem fase B.

## Testes

Em `BLOCKBENCH/studio`: `npm run build`, `npm test`, `npm run test:blockbench`.
Em `BLOCKBENCH/bridge`: `npm test`, `npm run test:blockbench`, `npm run test:studio`.

`test:studio` carrega Studio+Bridge+MTools reais no Blockbench 5.2.2, em perfil e
janela separados. As chamadas passam pelo SDK MCP, servidor de arquivos, daemon,
WebSocket e plugin nativo. Testa pixels/Undo, pastas/animadores, quatro VFX,
locators, áudio realmente decodificado/tocado, skin slim, persistência e exportação
com Khronos glTF Validator, além de rejeitar revisões antigas e alvos referenciados.
Não altera projetos pessoais. [Relatório da integração](STUDIO-TEST-RESULT.json).
Os dois erros nativos de notícias/start screen anteriores ao carregamento dos
plugins são registrados separadamente; erros após o carregamento fazem o teste falhar.
