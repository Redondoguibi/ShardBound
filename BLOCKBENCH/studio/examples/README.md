# Exemplos originais

Carregue o plugin, depois abra os arquivos `.sbstudio` por **Arquivo → Abrir modelo**. São projetos editáveis, com geometria, texturas, clips e recursos incorporados.

- **`crystal-sword.sbstudio`** — exemplo principal: espada de cristal com pastas **Lâmina** e **Empunhadura**, player neutro, clips `idle`, `run`, `jump`, `attack`, partículas, rastro, flash, brilho e áudio em frames da mesma timeline. Selecione `attack` e reproduza. O som é um chime WAV original de demonstração, não um asset de combate do jogo.
- **`player.sbstudio`** — player padrão, skin 64×64, camadas externas e clip `run` editável. Selecione as pastas corporais para adicionar suas próprias poses; use o modo Pintar para criar sua skin.
- **`authoring-lab.sbstudio`** — laboratório técnico com nove modelos independentes: ranged, melee, armadura, dois monstros, bloco, item/malha, player e efeitos. Demonstra toda a estrutura, clips do corpo inteiro e eventos. Modelos sobrepostos ficam visíveis juntos para inspeção; use o Outliner para ocultar objetos durante a edição.

**`crystal-sword-package/`** contém a exportação real do exemplo principal, com GLB, áudio e acompanhantes v1. Não é um mod já registrado no jogo. Todos os assets visuais e o áudio foram criados para estes exemplos; não há texturas Minecraft redistribuídas.

Os arquivos são gerados pelos testes reais. Para atualizar a partir de uma execução verificada:

```powershell
node scripts/refresh-examples.mjs .local/e2e/PASTA_DA_EXECUCAO
```

O script copia somente os projetos/recursos de exemplo e exclui perfis e relatórios temporários.
