# Design AI-first

O objetivo não é obrigatoriamente colocar IA dentro do jogo. É tornar o **desenvolvimento do jogo e dos mods extremamente amigável a IA**.

## Meta

Um agente deve conseguir receber algo como:

> Crie uma área congelada com três inimigos, um boss e seis equipamentos.

e produzir um mod organizado que possa ser validado pelo toolchain.

## Princípios

### 1. Estruturas explícitas

Preferir estruturas previsíveis a convenções escondidas.

### 2. IDs estáveis

Cada recurso deve ter identidade fácil de referenciar.

### 3. Schemas documentados

JSON e arquivos declarativos precisam de schemas e exemplos.

### 4. APIs pequenas e ortogonais

Melhor ter primitivas claras como:

```text
spawn()
play_sound()
add_component()
register_item()
```

do que uma API gigante e ambígua.

### 5. Erros estruturados

Além do texto humano, diagnósticos devem possuir código, categoria, arquivo e localização.

Exemplo:

```text
MOD102 MissingAsset
file: scripts/boss...
resource: frozen:ice_golem
```

### 6. Validação separada de execução

Deve ser possível validar um mod sem iniciar o jogo inteiro.

### 7. Ferramentas CLI

O projeto deverá futuramente ter comandos equivalentes a:

```text
game-tool new-mod
game-tool validate
game-tool build
game-tool inspect
game-tool dependency-tree
game-tool test
```

Os nomes são provisórios.

### 8. Documentação versionada

A documentação da API deve acompanhar a versão do runtime.

### 9. Determinismo quando possível

Processos determinísticos tornam bugs mais fáceis de reproduzir e corrigir.

### 10. Componentização

Evitar arquivos monolíticos e APIs que exijam conhecer o core inteiro para adicionar conteúdo simples.
