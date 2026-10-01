# Mod de exemplo executável

Este pacote contém um cubo e textura originais, registrados como `example:cube`.
A definição `example:Cube` é importada explicitamente. `on_start` chama `spawn`;
`on_update` move a entidade com delta time através da API própria.

Copie a pasta para `mods/` ao lado de `game.exe` e reinicie. O CMake copia este
exemplo para o build. Retirá-lo do diretório do executável e reiniciar remove
o conteúdo; o cache pode continuar no disco sem remontar o mod.

Requer API 0.1; sem dependências. Modelo gerado pelo script
`tools/generate-demo-model.py`, licença Apache-2.0 do repositório.
