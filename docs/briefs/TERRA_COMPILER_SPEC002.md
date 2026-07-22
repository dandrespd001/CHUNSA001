# Brief cerrado — `chunsa_data_compiler` CHDB v1

## Rol, entrada y exclusiones

Implementa el compilador offline aprobado por SPEC-002 v1.0. Trabaja solo en:

```text
tools/data_compile/chunsa_data_compiler.py
tools/data_compile/test_data_compiler.py
tests/data_compile/**
```

Los schemas de `data/schemas/*.schema.json` son entrada read-only. No modifiques schemas, CMake, kernel, demo, datos reales, docs ni dependencias. No uses red, subprocess, timestamps, cwd/hostname/git ni rutas absolutas en outputs. Python 3.14.6, PyYAML 6.0.3 y jsonschema 4.26.0 ya existen.

## CLI exacta

```text
chunsa_data_compiler.py validate <source_root> [--profile dev|release]
chunsa_data_compiler.py compile <source_root> --out <file.chdb>
  [--hash-out <file.content.json>] [--profile dev|release] [--print-hash]
chunsa_data_compiler.py inspect <file.chdb> [--json]
```

Default profile release; default hash-out `<out>.content.json`. Exit `0 OK`, `1 validación/integridad`, `2 CLI/E/S/config`, `3 error interno`. Compile siempre valida, no hay force. Print exacto:

```text
content_hash=sha256-v1:<64 lowercase hex>
records unit=N building=N tech=N civ=N map=N ai-profile=N
```

## Layout fuente

Root requerido: `manifest.yaml`; directorios no recursivos `units`, `buildings`, `tech`, `civilizations`, `maps`, `ai_profiles`; solo `*.yaml`, ordenados por filename UTF-8 para diagnóstico. Un archivo contiene un record. Categoría→schema/kind: units→unit/2, buildings→building/1, tech→tech/1, civilizations→civ/1, maps→map/1, ai_profiles→ai-profile/1. Directorios vacíos son legales. Manifest exactamente uno.

Caps antes de parsear: archivo 1 MiB, mapa 16 MiB; UTF-8 estricto; string 65535 bytes; profundidad máxima 16.

## YAML restringido fail-closed

No uses resolvers default. Scanner/composer rechaza tags explícitos, anchors, aliases, merge `<<`, claves duplicadas/no-string, multidoc, NUL y UTF-8 no NFC. Resolver vacío y solo:

- plain bool exactamente `true|false`;
- plain int `-?(0|[1-9][0-9]*)`, dentro de int64;
- string quoted, o plain que no parezca número/fecha/bool/null reservado;
- rechazar plain null/~, yes/no/on/off en cualquier case, fecha YYYY-MM-DD, float/exponente, prefijo numérico inválido, `+`, `_`, hex/octal/sexagesimal/cero inicial.

Fechas válidas de schemas deben venir quoted y siguen como string. No reparar/coaccionar.

## Validación de schemas

Carga los ocho JSON locales y usa Draft202012Validator con registry local; ningún `$ref` puede hacer fetch. Ordena errores como `(source_kind,record_id-or-empty,json_pointer,error_code)`. `additionalProperties`, required, type/range/condition son `E_SCHEMA`; schema_version incorrecto `E_SCHEMA_VERSION`; YAML `E_YAML_PROFILE`; caps `E_LIMIT`.

## Semantic pass D1

Acumula cuando sea seguro y emite una línea estable por error: `ERROR <code> <kind> <id-or-> <json-pointer>: <mensaje>`.

1. IDs globalmente únicos entre kinds, namespace en `manifest.owned_namespaces`.
2. Todas las refs existen y son del kind correcto. Capabilities/behaviors/materials/variant groups deben estar declarados. `base:*` también requiere namespace owned.
3. `civ_id`/`available_to` y listas civ unit/building/tech coinciden bidireccionalmente.
4. Epoch windows ordenadas, dentro de civ y con solape. Todos los playable_period_ids existen en la civ. Periodos/años ordenados sin solape ni año 0. Counterfactual ya condicionado por schema.
5. Unit: suppression tags incompatibles; reglas citizen/combat; coste de recurso con al menos un valor `>0`; D1 compila siege/naval pero no activa. Building constructible y tech no-institution aplican el mismo requisito salvo que satisfagan literalmente la alternativa material permitida por sus schemas.
6. Tech prerequisites DAG; mutually_exclusive simétrico; no self refs; regional group declarado. Material_costs cuenta solo declared material strategic=true y máximo 2.
7. Materiales: IDs únicos. Spawn material solo kind deposit. Recipe output solo intermediate, no autoconsumo; grafo input-material→output acíclico. Refs/costes declarados.
8. Map: width*height checked; terrain/cost RLE suma exacta y runs adyacentes iguales prohibidos. Row-major. Bounds half-open x<width*1000/y<height*1000 con checked; starts/deposits sobre cost!=255 y terrain!=water; cells únicas y no compartidas start/deposit. starting_positions se canoniza por slot. resource_spawns por `(y,x,kind_code,id UTF-8,amount)` con resource=0/material=1. Resource id debe ser A/B/P/W/Me/F/I/El; material record_id deposit.
9. AI: utility consideration única; points input_bp estrictamente creciente; tactical group/behavior pares únicos; behavior declarado; difficulty/LOD periods >0.
10. Provenance: release exige manifest y todos records status promoted. Dev admite otros y marca flag. Fechas calendario reales. Source URL HTTPS y reglas de evidence ya estructurales. verification_reports son paths relativos sin segmentos `.`/`..`, no absolutos.

Códigos mínimos: `E_YAML_PROFILE E_SCHEMA E_SCHEMA_VERSION E_DUPLICATE_ID E_NAMESPACE E_REFERENCE E_CYCLE E_EPOCH_WINDOW E_UNIT_CLASS E_MAP_RLE E_AI_FAIRPLAY E_PROVENANCE E_LIMIT E_BLOB_PARSE E_IO E_INTERNAL`.

## Normalización canónica

No mutar input para ocultar error. Tras validar: mapas/objects por key UTF-8; sets declarados por schemas se copian ordenados por encoding CVE1 del valor (strings por UTF-8); listas de prioridad/curve points/RLE conservan orden salvo los dos spawn sorts normativos. Records por record_id UTF-8; manifest por package_id. Comments, filenames y map-key order no afectan bytes.

## CVE1

Serializa recursivamente valores ya normalizados:

```text
0x01 false
0x02 true
0x10 int64: 8 bytes two-complement LE
0x20 string: u32 byte_length + UTF-8 NFC
0x30 array: u32 count + values
0x40 object: u32 pair_count + por par [u32 key_len + key bytes + value]
```

Keys estrictamente crecientes UTF-8. Root record object. Sin null/float/bytes. Implementa parser CVE acotado para self-parse/inspect: depth16, nodes/record262144, collection65535, string65535; consume exactamente payload, sección y archivo; aritmética de offsets checked. No reservar por count sin cap y bytes restantes.

## CHDB v1 exacto

Archivo max64MiB, sin compresión. Header 40 bytes:

```text
char[8] "CHNSDB1\0"; u16 major=1; u16 minor=0; u32 schema_set=1;
u32 flags (bit0 UNVERIFIED, bit1 HAS_PATCHES; D1 solo bit0);
u32 section_count=7; u32 directory_entry_size=24; u32 reserved=0; u64 file_size.
```

Directorio 7 entries de 24 bytes: `u16 kind,u16 schema_version,u32 record_count,u64 offset,u64 byte_size`. Kinds/versiones: 1 MANIFEST/1, 2 UNIT/2, 3 BUILDING/1, 4 TECH/1, 5 CIV/1, 6 MAP/1, 7 AI_PROFILE/1. Todas presentes aunque vacías. Manifest count1. Sections contiguas: first offset `40+7*24=208`, next exact previous end, last end file_size.

Cada record `u32 payload_size + CVE1_object`, payload <=1MiB (MAP16MiB). Counts caps: unit/building/tech65535, civ/map/ai1024. Self-parse antes de escribir y compara kind/count/IDs con semantic index. Cualquier fallo interno E_BLOB_PARSE y no publica.

## Hash y escritura

`artifact_hash=SHA256(b"CHUNSA_CONTENT_V1\0" + complete_blob)`. No se incrusta. Sidecar bytes exactos sin espacios/BOM, keys en este orden, lowercase hash, un LF:

```json
{"algorithm":"sha256","algorithm_version":1,"blob_format":"1.0","content_hash":"<hex>","schema_set_version":1}
```

Escribe temp regular en el mismo directorio, flush+fsync, `os.replace`; limpia temps en fallo. Blob y sidecar solo se publican después de self-parse. Si falla el segundo replace, reporta E_IO sin afirmar transacción multiarchivo.

## Inspect

Valida CHDB hostil con todos los caps. Sin `--json`, imprime versiones/flags/hash y counts estables. Con `--json`, una sola línea JSON `sort_keys=True,separators=(",",":")` con metadata, hash y records decodificados; no parsea sidecar.

## Tests requeridos

Unittest, temp dirs, sin red/subprocess salvo invocar `main(argv)` directamente. Incluye fixtures sintéticos mínimos de los 7 schemas.

- YAML adversarial: duplicado, alias/anchor, merge, tag, multidoc, yes/on/null/date/float/hex/octal/cero inicial, non-NFC, profundidad17, oversized.
- Schema: un negativo representativo por kind, costes solo-cero para unit/building/tech no-institution y refs locales sin red.
- Semantic: duplicate cross-kind, namespace, missing/wrong-kind refs, cycles, civ bidireccional, periods, materials/recipes, map RLE/bounds/cells, AI points, provenance release/dev.
- Determinismo: filenames/map keys/sets/comments reordenados producen bytes/hash iguales; entero semántico cambia hash.
- Assert header/directorio/sections/flags y sidecar bytes exactos.
- Parser rechaza magic/version/flags/counts/offset overflow/gaps/overlap/trailing, payload truncado, CVE depth/nodes/string/non-NFC/key order/trailing.
- CLI exit/defaults/print output/inspect JSON.

No generes ni cambies el fixture histórico real. Devuelve cambios sin commit y un resumen de tests.
