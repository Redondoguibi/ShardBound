# Modcheck v0

Ferramenta inicial da etapa 12. Usa o mesmo manifest loader, VFS, lexer e parser
do runtime, sem abrir janela, inicializar GPU ou executar scripts.

```powershell
./build/debug/modcheck.exe ./mods
./build/debug/modcheck.exe ./examples/gameplay --json
```

O caminho é relativo ao diretório corrente, como numa CLI convencional. Aponta
para a pasta que contém os pacotes, cada um com seu `mod.json`.

Valida manifests, duplicatas, versões, dependências, sintaxe do entry e imports
transitivos, extensão `.sl`, contenção no mod e ciclos de importação. Imports
dentro de funções/condicionais também são inspecionados estaticamente. Um arquivo
é verificado uma vez por pacote. Limite de profundidade de imports: 64.

Saída humana por padrão; `--json` retorna `valid`, `mods`, `scripts` e lista de
diagnósticos com código, severidade, arquivo, mod, recurso, linha e coluna. Códigos
de saída: 0 válido, 1 erros de validação, 2 uso incorreto. Dependência opcional
ausente é aviso; dependência obrigatória ausente é erro para o validador, embora
o runtime ainda possa carregar conteúdo independente conforme sua política.

Esta ferramenta não valida semântica de execução, existência de funções/símbolos,
tipos dinâmicos, assets.json, decodificação de assets, render ou gameplay. O texto
PASS significa aprovação apenas nos checks acima. O pipeline completo continua
sendo exercitado pelo runtime e pelos testes de integração.
