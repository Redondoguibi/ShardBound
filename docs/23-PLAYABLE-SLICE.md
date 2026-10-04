# Etapa 13 — contrato do primeiro ciclo jogável

Contrato técnico de protótipo, 2026-10-03. Não congela arte, balanceamento,
catálogo, rig de skinning ou progressão. As decisões do autor em [22](22-GAME-DIRECTION-AND-REFERENCES.md) prevalecem.

## Plataforma e conteúdo

O pacote `adventure` contém o mundo e as regras em módulos `.sl`, usando os
registries, hooks e saves existentes. A Application apenas conecta serviços.
O hub e a floresta são uma área contínua pequena, sem carregamento/streaming.
O limite do protótipo é físico; nenhuma missão bloqueia a passagem entre regiões.

Entradas provisórias: WASD, Espaço para salto, botão esquerdo para ataque na
direção do último movimento, M para mapa, 1/2/3 para destinos descobertos,
I para inventário, Q para trocar arma, F5/F9 para salvar/carregar e Escape para sair.
O mapa não pausa inimigos nem o relógio de combate. Não há movimento por clique.

O controlador preserva a física Jolt a 60 Hz e usa sondas descendentes que
ignoram o próprio corpo para detectar apoio. Salto só ocorre vivo, ativo e
apoiado, nunca durante subida. A API retorna false quando não pode saltar.
Velocidade vertical negativa, sem apoio, define queda. Diagonais são normalizadas.

## Arte e movimento rígido

Template original no Blockbench: cabeça 8×8×8, tronco 8×12×4, membros 4×12×4,
pés em Y=0, frente -Z. Autoria em 16 unidades por unidade de mundo; pivôs
Head=(0,24,0), Body=(0,24,0), braços=(±4,22,0), pernas=(±2,12,0).
Nomes de peças começam com esses seis nomes. A pose rígida é transitória e
aplicada em coordenadas de modelo antes do transform da entidade; não é skinning
nem suporte a clips glTF. A fonte `.bbmodel` permanece editável.

## Combate e itens

Espada de ferro inicial; definições de arma contêm nível, raridade, dano,
alcance, arco, preparação, recuperação e padrão. Valores são provisórios e
podem ser alterados por `modify`/hooks. Acerto consulta alcance, direção e
linha de visão no instante de impacto; cada alvo recebe no máximo um acerto
por ataque. Crítico de queda aplica 1,7× no instante de impacto.
Inimigos têm perseguição limitada à floresta, preparação visível e recuperação.
O hub é seguro. Loot e recompensas não impõem campanha ou curva de XP.

## Waystones, morte e persistência

Raio provisório de descoberta: 2,5 unidades. Descobertas são independentes
por destino. Viagem gratuita e instantânea exige seis segundos completos sem
dano positivo causado/recebido pelo jogador. Dano zero não reinicia o relógio.
Teleportes zeram velocidade. Dano ambiental ao jogador também bloqueia viagem.
Na morte: uma aplicação de `floor(ouro * 0.15)`, retorno ao hub com vida cheia,
equipamento, itens e descobertas preservados. Sem final/campanha.

O envelope de save continua v1. Estado do pacote usa schema 1 e IDs persistentes;
referências de runtime são reconstruídas em `on_loaded`. Cooldowns e descobertas
fazem parte do estado, para carregar não liberar viagem durante combate.
Tint RGB é campo opcional do modelo no save, padrão branco para saves antigos;
poses rígidas são reconstruídas. Saves futuros incompatíveis geram diagnóstico.
Ausência de mods mantém a preservação de órfãos da Etapa 12.

## Aceitação

Verificar salto/aterrissagem/parede/ausência de salto duplo, exploração livre,
ativação dos destinos, dano dado e recebido, limite exato de seis segundos,
crítico só na queda, morte/perda única, save/load, parâmetros modificados por
outro pacote, pixels reais de GPU e smoke da Application em Debug/Release.
Esse ciclo não equivale à conclusão do jogo inteiro: animações finais, UI final,
streaming, catálogo amplo e otimização representativa continuam trabalhos futuros.
