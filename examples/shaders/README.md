# Shader laboratory

Exemplo executável de um shader de superfície com emissão animada e um passe de
cor/vinheta. Não muda o conteúdo oficial instalado.

```powershell
./build/debug/modcheck.exe examples/shaders --json
./build/debug/game.exe --mods examples/shaders
```

O runtime resolve os caminhos relativamente à pasta do executável. Os builds
copiam este exemplo para `build/<preset>/examples/shaders`.

Para instalar junto ao jogo, copie a pasta `shader_lab` para `content` na pasta
do executável. O cubo demonstrativo aparece na origem; o passe afeta a cena inteira.
O exemplo é opcional e não entra automaticamente nos mundos oficiais.

Veja o [contrato e limites da ABI 1](../../docs/modding/09-SHADERS.md).
