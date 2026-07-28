# openage — GitHub README + nyan
**URLs:** https://github.com/SFTtech/openage · https://github.com/SFTtech/nyan
**Fetched:** 2026-07-28 (WebFetch)
**Status:** OK

## openage — propósito
- "a volunteer project to create a free engine clone of the *Genie Engine* used by *Age of Empires*, *Age of Empires II (HD)* and *Star Wars: Galactic Battlegrounds*".
- No embarca los assets originales; necesita tener AoE1/AoE2 (incl. DE).
- **Caveats**: "gameplay is basically non-functional", "no network compatibility with the original", "no binary compatibility" (conversión one-way prevista).

## Stack
- C++20 (motor) + Python3 (scripting, conversión, consola, code-gen) + Cython (pegamento) + Qt6 (GUI) + CMake + OpenGL + Opus (audio) + **nyan** (configuración).
- Doc: Doxygen sobre C++ y docstrings Python. `make doc` después de `./configure`. Salida en `bin/doc/`.
- README advierte: "doc/ tends to get outdated".

## nyan — formato de datos
- Acrónimo recursivo: "nyan - yet another notation".
- **Lenguaje de descripción de datos + DB jerárquica clave-valor**, escrita en C++20.
- Diseñado para que mods modifiquen mods (modding meta-meta).
- **Type safety**: declaras tipos (`Unit`, `Building`) con campos tipados; JSON/YAML no tienen eso.
- **Herencia**: `(ParentType)`. Operadores `=` (override), `+=` (extend), `-=` (remove).
- **Patch operations**: `Change<Type>()` y `Add<Type>()` que modifican objetos *existentes* sin reescribirlos.
- **Mod-as-data**: mods son objetos de primera con `name` y `patches`.

### Ejemplos
```
Unit():
    hp : int
    animation : file

Building():
    hp : int
    creates : set(Unit)
    model : file

OverwatchSoldier(Unit):
    hp = 50
    animation = "./assets/soldier.ani"

CombineCitadel(Building):
    hp = 9001
    creates = {OverwatchSoldier, Strider}
    model = "./assets/lambda_hq.mdl"

Citizen(Unit):
    hp = 60
Gordon(Citizen):
    hp += 40
```

## Doc interna
- `doc/project_structure.md` recomendado como punto de entrada.
- `media_convert.md` describe el convertidor de assets.

## Estado
- 305 commits en master de nyan, 257 stars, 32 forks.
- "API still evolving; not yet considered stable".
- Licencia: GNU LGPLv3 (o posterior).
