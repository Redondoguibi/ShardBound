# Trailcraft — extensão executável

Copie esta pasta para `build/debug/content/trailcraft` (ou `release/content`)
depois do build. O pacote depende de adventure; reinicie o jogo para carregá-lo.

- Modifica a espada de ferro e acrescenta dano pelo hook público.
- Adiciona a Lâmina do caminho; uma cópia entra no inventário ao iniciar.
- Personaliza cada cópia dessa arma com nível 4, raridade Rara e dano 31.
- Adiciona a Trilha do norte, conectada ao fim da floresta, e sua waystone.

Use I, Q e E para selecionar/equipar a arma. Explore ao norte da clareira até
o acampamento para descobrir o destino. M, Q e E permitem viajar para ele.
Carregar um save anterior acrescenta a região e a waystone; a arma inicial do
mod só é dada em mundos novos. Mods podem implementar seus próprios loots.

Os atributos das instâncias e descobertas persistem. Sem o mod, os dados dos
itens ficam preservados e indisponíveis para equipamento. Contrato em
[24](../../../docs/24-EQUIPMENT-AND-WORLD-CONTENT.md).
