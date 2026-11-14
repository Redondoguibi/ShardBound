# Exemplo async/await

Copie esta pasta para o diretório de mods, junto de `foundation`. Ao abrir um mundo, o mod executa três etapas separadas por 0,25 segundo de tempo de jogo, registra cada etapa e disponibiliza o resultado em `job.result` após `job.done`.

A pausa suspende o relógio. Reabrir o mundo começa uma tarefa nova; este exemplo não persiste progresso. Para outros padrões, cancelamento, limites e a sintaxe completa, consulte [o guia](../../docs/modding/13-ASYNC-AWAIT.md).
