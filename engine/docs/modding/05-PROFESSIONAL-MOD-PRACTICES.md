# 5. Práticas profissionais para mods

## 1. Trate seu mod como software versionado

Use versionamento consistente.

Recomendação:

```text
1.0.0
1.1.0
2.0.0
```

Mudanças que quebram IDs, saves ou APIs públicas devem ser consideradas breaking changes.

## 2. IDs são API pública

Não renomeie:

```text
my_mod:scarab
```

para:

```text
my_mod:desert_scarab
```

depois de publicar sem fornecer uma estratégia de migração, porque outros mods e saves podem depender do ID antigo.

## 3. Declare dependências corretamente

Não assuma que outro mod está instalado.

Separe:

- dependência obrigatória;
- integração opcional.

## 4. Prefira extensão a replace

Quando possível:

```text
hook
event
registry
component
merge
```

é melhor para compatibilidade do que substituir um sistema inteiro.

Use replace quando a intenção for realmente tomar posse daquele comportamento/recurso.

## 5. Evite conflitos silenciosos

Um mod profissional deve falhar de forma diagnosticável.

Erros úteis incluem:

- ID;
- arquivo;
- linha quando possível;
- dependência;
- recurso ausente;
- ação sugerida.

## 6. Performance

Lembre que o usuário pode ter muitos mods instalados.

### Modelos

- reuse meshes/materials;
- favoreça instancing;
- mantenha cuboids simples quando a direção visual permitir;
- evite material único por objeto sem motivo.

### Texturas

- use nearest em pixel-art quando adequado;
- não use resolução maior apenas "por segurança";
- use atlas quando fizer sentido;
- evite transparência real se alpha-test resolve.

### Scripts

- evite loops globais por frame sem necessidade;
- prefira eventos;
- limite buscas amplas por entidades;
- cache referências/IDs quando a API permitir.

### VFX

- não transforme toda partícula em luz;
- use budgets;
- use pooling;
- limite partículas distantes.

## 7. Assets autocontidos

Para modelos, GLB com texturas embutidas é um workflow recomendado.

Isso facilita:

- distribuição;
- validação;
- cache;
- autoria por IA;
- evitar paths quebrados.

## 8. Compatibilidade

Se outro mod precisa alterar somente uma parte do seu modelo, o sistema poderá expor sub-recursos como:

```text
my_mod:boss/texture/Body
my_mod:boss/animation/attack
```

Mantenha nomes de sub-recursos estáveis quando fizerem parte da API pública.

## 9. Teste com ausência de dependências opcionais

Seu mod deve ser testado em pelo menos:

```text
somente seu mod
+
seu mod + dependências obrigatórias
+
seu mod + integrações opcionais
```

## 10. Teste instalação limpa

Não confie no seu cache local.

Antes de distribuir, teste:

```text
cache vazio
→ instalar mod
→ Preparing Mods
→ compilar
→ carregar
```

e depois:

```text
segundo startup
→ cache hit
→ startup rápido
```

## 11. Documente seu próprio mod

Inclua pelo menos:

- README;
- versão suportada da API;
- dependências;
- lista de IDs públicos importantes;
- instruções de instalação;
- incompatibilidades conhecidas;
- changelog.

## 12. Preserve o namespace

Nunca registre conteúdo novo usando `core:`.

O namespace `core:` pertence ao jogo base.

Use seu próprio namespace e APIs explícitas de `modify`/override quando quiser alterar o core.

## 13. Não dependa do cache compilado

Cache é descartável.

Seu mod deve ser reconstruível apenas a partir dos source assets e arquivos de configuração presentes no pacote.

## 14. Automação e CI

O projeto pretende fornecer ferramentas CLI apropriadas para validação e build.

Quando disponíveis, um mod profissional deve rodar validação automaticamente antes de release.

Pipeline ideal:

```text
commit
↓
validate manifest
↓
validate scripts
↓
validate assets
↓
build/cache test
↓
run mod tests
↓
package
```

## 15. C++ nativo

Extensões nativas estão disponíveis pela ABI C v1 e pelo SDK documentado em
[../21-EXTENSIBILITY-V1.md](../21-EXTENSIBILITY-V1.md).

Não use C++ se a linguagem/API pública já resolve a necessidade. Código nativo aumenta:

- risco de crash;
- problemas de ABI;
- compatibilidade entre versões;
- superfície de segurança;
- custo de manutenção.
