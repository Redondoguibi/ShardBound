# Jogo-MCD2.5

Projeto experimental de um jogo 3D com estética pixel-art, construído sem uma engine de jogos tradicional e pensado desde o início para ser extremamente modificável.

A ideia central não é começar produzindo um jogo cheio de conteúdo. Primeiro será criado o **núcleo/runtime** do jogo, junto de um **sistema de mods** e uma **linguagem própria** capaz de descrever e estender conteúdo e comportamento.

## Conceito

O jogo base será pequeno e funcionará como a fundação do ecossistema — conceitualmente próximo de um hub.

O restante poderá ser adicionado por mods:

- áreas e mapas;
- inimigos;
- bosses;
- equipamentos;
- habilidades;
- NPCs;
- sistemas;
- alterações em sistemas existentes;
- novos tipos de conteúdo;
- extensões profundas do jogo.

Os mods serão híbridos. A linguagem própria será o principal meio de criação, mas um mod também poderá conter C++, JSON, modelos 3D, imagens, áudio e outros recursos.

## Workflow desejado

```text
Criar mod
   ↓
Adicionar scripts + dados + assets
   ↓
Colocar o mod no jogo
   ↓
Inicialização do jogo
   ↓
Resolver mods e dependências
   ↓
Compilar/carregar conteúdo
   ↓
Integrar ao runtime
   ↓
Jogar
```

Remover um mod e reiniciar o jogo remove o conteúdo fornecido por ele, respeitando as regras futuras de save e compatibilidade.

## Princípios

1. **Modding é arquitetura central**, não recurso posterior.
2. **C++ no núcleo**, com bibliotecas externas permitidas.
3. **Sem engine pronta**, mas bibliotecas especializadas são permitidas.
4. **Linguagem própria** equilibrada entre legibilidade humana e geração por IA.
5. **Mods híbridos**, não limitados à linguagem própria.
6. **Extensibilidade profunda**: sistemas existentes poderão ser estendidos e, em casos avançados, substituídos.
7. **Carregamento no startup** inicialmente; hot reload não é prioridade.
8. **IA-first**: a estrutura deve favorecer geração, revisão e manutenção de mods por agentes de IA.

## Documentação

Veja [docs/README.md](docs/README.md).

> Estado atual: planejamento inicial. Muitas decisões técnicas continuam deliberadamente abertas.
