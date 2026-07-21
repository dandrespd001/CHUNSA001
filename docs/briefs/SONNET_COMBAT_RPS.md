# BRIEF — Combate RPS v1 (Sonnet 5) · primera pieza del Sprint 0.3

Eres ingeniero C++ senior de sistemas deterministas en CHUNSA001. **Rama `sonnet/combat-rps` desde main. Jamás toques main.** Contrato CERRADO del Arquitecto: **impleméntalo tal cual, no rediseñes.** Es el primer sistema de combate: unidades con HP/ataque/clase que se golpean según el triángulo Piedra-Papel-Tijera (doc `07_COMBATE.md`), buscando objetivos por el spatial hash existente. Determinismo bit-exacto INTACTO (G1/G3/G4/G5 verdes).

## ⚠️ RECURSOS (el equipo se apaga por térmica): builds `nice -n 19 ... -j2`, uno a la vez.

## Invariantes (o G1 falla y se rechaza)
- Cero float en estado. Cero UB de signed overflow. Cero heap en `step()`.
- Orden de iteración fijo: índice de entidad ascendente. Desempates deterministas por índice menor.
- Daño en ENTEROS (basis points). No toques `flow_field.hpp`, `vision.hpp`, `save_io.hpp` salvo lo que el contrato indica.

## API existente que usarás (no la cambies)
- `GameState` (game_state.hpp): `entities` (EntityTable con `capacity`, `alive[]`, `generation[]`), `pos_x/pos_y[]` (raw Q47.16), `owner[]`, `shash` (SpatialHash), `destroy_batch[]`+`destroy_count`. `ENTITY_HARD_CAP=65536`.
- SpatialHash (spatial_hash.hpp): `sh_cell_index(sh, x_raw, y_raw)→cell`, `sh_first(sh, cell)→idx|SH_EMPTY`, `sh_next(sh, i)→idx|SH_EMPTY`, `SH_EMPTY`, `sh.cells_x`, `sh.cells_y`, `SH_CELL_RAW=131072` (2 tiles). El `sh_rebuild` ya corre en `step()` antes del punto donde añadirás el combate.
- Vec2Fx/dist (vec2fx.hpp): `uint64_t dist_sq_raw(Vec2Fx a, Vec2Fx b, FatalReason& f)` (magnitud² en raw; `mag_u64`).
- commands.hpp: `CommandType` (append-only), `CmdPayload{EntityHandle handle; int64_t x_raw,y_raw; int32_t speed_mtpt;}`, `RejectReason`.
- Comando existente `SPAWN_DEBUG` (crea unidad sin stats de combate → hp=0). El nuevo `SPAWN_UNIT` los añade.

## Contrato exacto

### 1. `game_state.hpp` — componentes de combate (tras los de flujo)
```cpp
    // Combate (Sprint 0.3). ESTADO: serializado + checksummeado.
    int32_t  hp[ENTITY_HARD_CAP];         // <=0 ⇒ muere al final del tick
    int32_t  max_hp[ENTITY_HARD_CAP];
    int32_t  attack[ENTITY_HARD_CAP];     // daño base (entero)
    int32_t  range_mt[ENTITY_HARD_CAP];   // alcance en mili-tiles (1000 = 1 tile)
    uint8_t  unit_class[ENTITY_HARD_CAP]; // 0=infantry 1=cavalry 2=artillery
    uint16_t atk_cd[ENTITY_HARD_CAP];     // ticks restantes hasta poder atacar de nuevo
```
(memset de gs_init ya los deja en cero.)

### 2. `commands.hpp` — `SPAWN_UNIT = 5` (append-only)
Reutiliza `CmdPayload`: `handle` = {slot deseado, gen esperada 1}, `x_raw/y_raw` = posición. Los stats de combate NO caben en CmdPayload actual → **añade a CmdPayload** los campos: `int32_t hp; int32_t attack; int32_t range_mt; uint8_t unit_class;` (al final; actualiza el checksum/serialize de `pending` en consecuencia — ver §5). `SPAWN_DEBUG` deja esos campos en 0.
Constante de cooldown de ataque: en step.hpp `inline constexpr uint16_t ATK_COOLDOWN_TICKS = 10;` (0.5 s @20 Hz).

### 3. `step.hpp` — aplicar SPAWN_UNIT + sistema de combate
**3a.** `case CommandType::SPAWN_UNIT:` en apply_command: como SPAWN_DEBUG (world_contains, et_spawn, set pos/owner) y además `g.hp[i]=g.max_hp[i]=c.p.hp; g.attack[i]=c.p.attack; g.range_mt[i]=c.p.range_mt; g.unit_class[i]=c.p.unit_class; g.atk_cd[i]=0; g.speed_mtpt[i]=0;` Valida `c.p.hp>0 && c.p.attack>=0 && c.p.range_mt>=0 && c.p.unit_class<=2`, si no MALFORMED.

**3b.** Multiplicador RPS (función `inline int32_t rps_mult_bp(uint8_t atk_class, uint8_t tgt_class)`), tabla en basis points (10000=100%), de doc 07:
```
            tgt=inf  tgt=cav  tgt=art
atk=inf     10000    10000    10000
atk=cav      8000    10000    13000
atk=art     13000     8000    10000
```

**3c.** `inline void combat_system(GameState& g)` — llamada en `step()` **después de `sh_rebuild` y del bloque de visión, antes del DESTROY** (nueva fase; el combate es cada tick, período 1). Para cada unidad `i` viva en orden ascendente:
- Si `g.hp[i] <= 0` → continue (ya muerta este tick, aún no reciclada).
- Si `g.atk_cd[i] > 0` → `--g.atk_cd[i];` continue.
- Buscar **enemigo más cercano en rango**: recorre la celda de `i` y sus 8 vecinas (usa `sh_cell_index` sobre `pos` de `i`; vecinas = variar cx±1, cy±1 dentro de `[0,cells_x)×[0,cells_y)`); para cada candidato `j` de esas listas (`sh_first`/`sh_next`): saltar si `j==i`, `!alive[j]`, `hp[j]<=0`, o `owner[j]==owner[i]` (mismo bando). Distancia: `dist_sq_raw(pos_i, pos_j)`; en rango si `dist_sq <= (range_raw)²` con `range_raw = (int64_t)range_mt[i]*65536/1000`. Elegir el de **menor dist_sq; desempate: menor índice j**.
- Si hay objetivo `best`: `dmg = attack[i] * rps_mult_bp(class_i, class_best) / 10000` (entero, ≥0); `g.hp[best] -= dmg;` si `g.hp[best] <= 0` y `best` no está ya en el destroy_batch → `et_mark_dead(g.entities, best); g.destroy_batch[g.destroy_count++]=best;` (respeta `destroy_count < PENDING_CAP`). `g.atk_cd[i] = ATK_COOLDOWN_TICKS;`
- **Determinismo:** el daño se aplica inmediatamente; el orden ascendente de `i` es la regla (una unidad procesada antes puede matar a su objetivo antes de que éste ataque — es correcto y determinista). No leas float. `dist_sq_raw` con `FatalReason&` local (no debe activarse en cota de mundo).

**3d.** Al reciclar en el DESTROY (paso 6), limpia también los componentes de combate del slot en `zero_components` (añade hp/max_hp/attack/range_mt/unit_class/atk_cd = 0). Verifica que no rompa el conteo.

### 4. `checksum.hpp` — state_checksum_v1
Tras el bloque de flujo, añade `hp[0..capacity)`, `max_hp`, `attack`, `range_mt` (i32 cada uno), `unit_class` (u8), `atk_cd` (u16), en ese orden, por índice ascendente.

### 5. `serialize.hpp` — gs_serialize/deserialize
Serializa los 6 arrays de combate (mismo orden que el checksum) tras el bloque de flujo. En `pending` (ScheduledCommand), serializa los 4 campos nuevos de CmdPayload (hp, attack, range_mt u32/i32; unit_class u8) tras `speed_mtpt`. Deserialize en el mismo orden. Sube `SAVE_FORMAT_VERSION` a 4 en save_io.hpp.

### 6. Test — `tests/unit/test_combat.cpp` (nuevo)
Escenario de choque de dos bandos, determinista:
- `GameState` en heap, `gs_init` cfg `{512, 2, 1, 20, 20, 256, 256, 7}`.
- Tick 0: SPAWN_UNIT de **60 caballería** (owner 0, class=1, hp=100, attack=20, range_mt=1500) agrupadas en x∈[120,130] y∈[120,136] tiles; y **60 artillería** (owner 1, class=2, hp=100, attack=20, range_mt=1500) en x∈[126,136] y∈[120,136] (solapando para que entren en rango). Posiciones/handles deterministas, sequences por emisor crecientes.
- Corre 400 ticks. CHECK:
  1. `g.fatal == NONE`.
  2. **Ventaja RPS**: la caballería (owner 0, +30% vs artillería) debe terminar con **estrictamente más unidades vivas** que la artillería (owner 1). Cuenta vivos por owner al final.
  3. Alguna unidad murió (el combate ocurrió): total vivos < 120.
  4. **Determinismo**: segunda corrida fresca idéntica → mismo `state_checksum_v1`.
- Imprime "combat: owner0=X owner1=Y checksum=..." y "combat: OK".
Añade target `chunsa_test_combat` a CMakeLists (link `chunsa_sim_core`, `add_test`).

## Verificación OBLIGATORIA antes de commitear (en la rama)
1. `nice -n 19 cmake -B build-gcc -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ && nice -n 19 cmake --build build-gcc -j2` limpio (`-Werror`, 0 warnings).
2. `ctest --test-dir build-gcc --output-on-failure` — TODOS verdes, incluido `combat`.
3. Gates (checksums cambiarán por el estado nuevo — deben PASAR, no coincidir con los viejos): `golden --vectors tests/determinism/golden` (1074/1074) · `run --selftest-g1` (OK, alloc_delta=0) · `savetest --units 200 --save-at 150 --resume-to 400` (OK) · `savetest --ai --units 200 --save-at 9 --resume-to 60 --hold-dispatched` (OK) · `record --units 200 --ticks 300 --out /tmp/g5.curp && verify --replay /tmp/g5.curp` (OK, ai_executions=0). Si alguno falla, rompiste el determinismo: arréglalo.
4. Commit en la rama: "Sprint 0.3: combate RPS v1 (SPAWN_UNIT, triángulo, busca por spatial hash) — generado: sonnet-5, pendiente revisión Arquitecto".
5. Escribe `docs/briefs/SONNET_COMBAT_RPS_RESULT.md`: qué tocaste, salida de verificación 2/3, desviaciones. Inclúyelo en el commit.
