# Etapa 13 — equipamentos, interface e mundo extensível

Contrato incremental de 2026-10-04. Refina o protótipo 23; decisões do autor
em 22 prevalecem. Os pontos 2–5 do próximo marco são acabamento de combate,
instâncias de equipamento, inventário e regiões/waystones definidas em conteúdo.

## Equipamento e saves

O pacote adventure é dono das regras. O inventário genérico do motor mantém
contagens por definição. `state.items` preserva uma entrada por cópia de arma:
`uid` numérico crescente, `definition`, `level`, `rarity`, `attributes` (record
de overrides) e `owned`. Remoções desativam entradas, depois compactadas; IDs nunca são reutilizados.
`state.equipped_uid` identifica a cópia equipada. A definição atual é mesclada
com atributos da instância; nível/raridade permanecem próprios. Não há fórmula
aleatória, XP ou progressão obrigatória. Hooks podem fornecer atributos.

Envelope v1 e schema adventure 1 permanecem; `equipment_schema=1` identifica
os campos aditivos. Saves anteriores são migrados pelas contagens e ID equipado.
Conteúdo ausente conserva instâncias/atributos e descobertas; itens indisponíveis
não podem ser equipados. Validação ocorre antes da troca de mundo.

## Conteúdo de mundo

`Regions`: nome, prioridade, safe, min/max X/Z, color, trees, shelters,
enemies. Cada região gera chão no intervalo definido; as áreas devem se conectar.
Maior prioridade vence sobreposição; empate usa ordem lexicográfica de IDs.
`Waystones`: label (nome de entidade único), name, region (ID), position,
destination, radius. Descobertas são indexadas pelo ID namespaced do registry.
`Worlds/adventure:Main` define start e respawn. `built_regions` registra IDs já
gerados; ao carregar, regiões e waystones novas são acrescentadas sem duplicar
conteúdo existente. Saves anteriores assumem as duas regiões originais.
Alterações de geometria já salva exigem um mundo novo; só acréscimos são reconciliados.
Todas as definições são enumeradas no início; mods dependentes podem adicionar
regiões, destinos e armas usando os mesmos contratos. Sem streaming neste marco.
Mapas calculam enquadramento a partir dos marcadores; não usam limites fixos.

## Interface e combate

I abre inventário, Q avança seleção, E equipa. M abre mapa, Q avança destino,
E viaja; 1/2/3 permanecem atalhos para os três primeiros destinos. Listas são
paginadas e suportam conteúdo de mods. Menus não pausam combate.
Armas usam preparação/impacto/recuperação, braço e espada compartilham pivô
de ombro, alcance aparece no chão durante preparação e impactos têm flash.
Poses continuam rígidas, sem skinning ou clips glTF.

APIs genéricas aditivas: `get_definitions(category)` retorna cópias ordenadas
de `{id,type,data}`; `definition_exists(category,id)`; `inventory_entries(entity)`
inclui itens de mods ausentes; `list_count`, `list_at` (índice zero), `list_contains`, `number_range` ([0,n)),
`list_append` (copia valor, máximo 4096), `record_has`, `text_length` (bytes UTF-8),
`data_merge` (cópia profunda).
`show_ui` aceita um quarto argumento opcional para painel expandido.
Limites deste marco: 256 instâncias, 63 destinos no catálogo, 256 regiões,
256 descobertas/IDs de região salvos (incluindo mods ausentes). Exceder limites
gera diagnóstico; não há descarte silencioso.
O hook `adventure:create_item` recebe a instância e permite nível, raridade e
atributos próprios; UID/definition/owned são protegidos. `adventure:melee` também
recebe weapon (snapshot mesclado) e item_uid. Eventos adicionais:
`adventure:equipped(uid,definition)` e `adventure:discovered(id)`.
Não mudam gramática `.sl`, registries, hierarquia ou formato do motor.
