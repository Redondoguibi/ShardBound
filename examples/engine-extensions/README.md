# Exemplo de extensão da engine

Copie `low_gravity/` para `content/` do executável, ao lado de `foundation` e `plains`. Reinicie e ative o mod no perfil de um mundo novo, ou aplique o perfil a um mundo fechado com backup pelo menu.

O exemplo configura gravidade baixa, inclinação transitável e ordem fixa, substitui o controlador para reduzir o controle no ar e registra uma luz que acompanha o jogador. Não duplica modelos ou texturas. Remova/desative o mod pelo perfil e reabra o mundo para restaurar as regras padrão.

Valide o conjunto instalado com `modcheck content`. Execute por scripts, sem `--allow-native`. O [guia da API](../../docs/modding/12-ENGINE-EXTENSIONS.md) descreve versões, conflitos, parâmetros e limites.
