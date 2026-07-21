# BRIEF — Moral y pánico v1 (Sonnet 5) · Sprint 0.3

Eres ingeniero C++ senior de sistemas deterministas en CHUNSA001. **Rama `sonnet/morale` desde main. Jamás toques main.** Contrato CERRADO del Arquitecto: **impleméntalo tal cual, no rediseñes.** Añade moral a las unidades (doc `07_COMBATE.md` §7.6): una unidad en fuerte desventaja numérica local pierde moral, entra en pánico y **huye** del enemigo más cercano (deja de atacar); se recupera cuando está a salvo. Determinismo bit-exacto INTACTO.

## ⚠️ RECURSOS (el equipo se apaga por térmica): builds `nice -n 19 ... -j2`, uno a la vez.

## Invariantes (o G1 falla)
- Cero float en estado. Cero UB de overflow. Cero heap en `step()`. Orden de iteración ascendente. Desempates por índice menor. Enteros en todo el estado.
- No toques flow_field/vision/sha256 salvo lo indicado.

## API existente (el combate v1 ya está en main)
- `GameState`: `entities`, `pos_x/pos_y[]` (raw Q47.16), `owner[]`, `hp[]`, `unit_class[]`, `atk_cd[]`, `speed_mtpt[]`, `flow_mode[]`, `vel_x/vel_y[]`, `shash`, `fatal`.
- SpatialHash: `sh_cell_index(sh,x,y)`, `sh_first`/`sh_next`, `SH_EMPTY`, `sh.cells_x/cells_y`.
- `dist_sq_raw(Vec2Fx,Vec2Fx,FatalReason&)`, `normalize_v1(Vec2Fx,FatalReason&)`, `Fx`, `fx_mul`, `fx_add`, `FX_ONE_RAW=65536`, `mag_u64`.
- `detail::combat_system(g)` y `detail::movement_v1(g)` en step.hpp — los EXTENDERÁS.
- `zero_components(g,i)` — añade ahí los nuevos componentes a 0.

## Contrato exacto

### 1. `game_state.hpp` — componentes de moral (tras los de combate)
```cpp
    // Moral (Sprint 0.3, doc 07 §7.6). ESTADO: serializado + checksummeado.
    int32_t morale[ENTITY_HARD_CAP];   // 0..MORALE_MAX; SPAWN_UNIT lo pone a MORALE_MAX
    uint8_t fleeing[ENTITY_HARD_CAP];  // 1 = en pánico (huye, no ataca)
```
En `SPAWN_UNIT` (apply_command, step.hpp): `g.morale[i] = MORALE_MAX; g.fleeing[i] = 0;` (SPAWN_DEBUG los deja en 0 → esas unidades cuentan como sin moral; el combate real usa SPAWN_UNIT).

### 2. `step.hpp` — constantes + sistema de moral
```cpp
inline constexpr int32_t MORALE_MAX = 100;
inline constexpr int32_t MORALE_PANIC = 20;    // <= ⇒ entra en pánico
inline constexpr int32_t MORALE_RALLY = 50;    // >= ⇒ deja de huir
inline constexpr int32_t MORALE_DROP = 8;      // baja/tick si en desventaja
inline constexpr int32_t MORALE_REGEN = 2;     // sube/tick si a salvo
inline constexpr uint32_t MORALE_RADIUS_CELLS = 1;  // celda + vecinas
```

**`inline void morale_system(GameState& g) noexcept`** — se llama en `step()` **entre `combat_system` y el DESTROY** (tras el combate del tick, para reaccionar a lo que pasó). Para cada unidad `i` viva con `hp[i] > 0`, en orden ascendente:
- Cuenta `allies` y `enemies` en la celda de `i` y sus 8 vecinas (mismo patrón que combat_system): entidad `j` viva con `hp[j]>0`, `j!=i`; `owner[j]==owner[i]` → ++allies (incluye contarse a sí mismo NO, j!=i); `owner[j]!=owner[i]` → ++enemies.
- **Regla de moral**: si `enemies > allies + 1` (en desventaja local clara) → `morale[i] -= MORALE_DROP;` si `enemies == 0` → `morale[i] += MORALE_REGEN;` (en otro caso, sin cambio). Clamp a `[0, MORALE_MAX]`.
- **Histéresis de pánico**: si `morale[i] <= MORALE_PANIC` → `fleeing[i] = 1;` si `morale[i] >= MORALE_RALLY` → `fleeing[i] = 0;` (entre medias mantiene el estado — evita parpadeo).

### 3. `combat_system` — las unidades en pánico no atacan
Al inicio del bucle, tras el check de `hp[i]<=0`: `if (g.fleeing[i]) { if (g.atk_cd[i] > 0) --g.atk_cd[i]; continue; }` (siguen enfriando el cooldown pero no atacan).

### 4. `movement_v1` — huida
Añade una rama AL PRINCIPIO del bucle de unidades (antes de la rama de flujo y de la de seek), con prioridad máxima:
```cpp
        if (g.fleeing[i]) {
            // Huir: moverse en dirección OPUESTA al enemigo vivo más cercano
            // (celda + 8 vecinas). Si no hay enemigo cerca, quedarse quieto.
            // (busca el enemigo más cercano igual que combat, desempate índice)
            ... encuentra best enemigo (mismo patrón de celdas/filtros que combat_system,
                owner distinto, hp>0, vivo) por menor dist_sq, desempate menor índice ...
            if (best encontrado) {
                const int64_t step_fx = (int64_t)g.speed_mtpt[i] * FX_ONE_RAW / 1000;
                Vec2Fx away = normalize_v1(Vec2Fx{Fx{g.pos_x[i]-g.pos_x[best]},
                                                  Fx{g.pos_y[i]-g.pos_y[best]}}, g.fatal);
                Fx vx = fx_mul(away.x, Fx{step_fx}, g.fatal);
                Fx vy = fx_mul(away.y, Fx{step_fx}, g.fatal);
                g.vel_x[i]=vx.raw; g.vel_y[i]=vy.raw;
                g.pos_x[i]=fx_add(Fx{g.pos_x[i]},vx,g.fatal).raw;
                g.pos_y[i]=fx_add(Fx{g.pos_y[i]},vy,g.fatal).raw;
                // clamp a [0, WORLD_RAW_MAX) igual que la rama de flujo
            } else { g.vel_x[i]=0; g.vel_y[i]=0; }
            continue;
        }
```
Cuidado: si `g.pos_x[i]-g.pos_x[best]` y el y son ambos 0 (superpuestas), `normalize_v1` devuelve (0,0) → la unidad no se mueve ese tick (aceptable). Usa resta con cuidado del overflow: como ambas están en cota de mundo (<2^29), la resta cabe en int64 sin problema.

### 5. `checksum.hpp` / `serialize.hpp`
Tras el bloque de combate: `morale[0..capacity)` (i32) y `fleeing[0..capacity)` (u8), mismo orden en checksum y serialize/deserialize. Sube `SAVE_FORMAT_VERSION` a 5.

### 6. Test — `tests/unit/test_morale.cpp` (nuevo)
Escenario determinista de desbandada:
- cfg `{512, 2, 1, 20, 20, 256, 256, 11}`.
- Tick 0: SPAWN_UNIT de **10 infantería** owner 0 (hp=100, attack=15, range_mt=1500, class=0) agrupadas en x≈128 y≈128; y **80 infantería** owner 1 (mismos stats) rodeándolas MUY cerca (x∈[120,140] y∈[118,140]). El bando 0 (10) está en fuerte desventaja → debe entrar en pánico y huir.
- Corre 300 ticks. CHECK:
  1. `fatal == NONE`.
  2. **Pánico**: en algún momento ≥1 unidad del owner 0 tuvo `fleeing==1` (registra un flag durante la corrida, o comprueba que alguna del owner 0 viva terminó con morale < MORALE_MAX o fleeing==1).
  3. **Huida efectiva**: la distancia media de las unidades vivas del owner 0 al centro del enjambre enemigo (≈(130,129) tiles) al final es MAYOR que al inicio (se alejaron), O casi todas murieron (si el enjambre las alcanzó) — acepta cualquiera de las dos con un CHECK claro y comentado.
  4. **Determinismo**: segunda corrida fresca → mismo `state_checksum_v1`.
- Imprime "morale: o0_vivos=X panicked=Y checksum=..." y "morale: OK".
Target `chunsa_test_morale` en CMakeLists.

## Verificación OBLIGATORIA antes de commitear (en la rama)
1. Build `-Werror` 0 warnings (`nice -19 -j2`).
2. `ctest --test-dir build-gcc --output-on-failure` — TODOS verdes, incluido `morale` y `combat` (no rompas el combate).
3. Gates (checksums cambiarán, deben PASAR): golden 1074/1074 · `run --selftest-g1` (alloc_delta=0) · `savetest --units 200 --save-at 150 --resume-to 400` · `savetest --ai --units 200 --save-at 9 --resume-to 60 --hold-dispatched` · `record ... && verify` (ai_executions=0). Si algo falla, rompiste el determinismo: arréglalo.
4. Commit en la rama: "Sprint 0.3: moral y pánico v1 (desventaja→huida, doc 07 §7.6) — generado: sonnet-5, pendiente revisión Arquitecto".
5. `docs/briefs/SONNET_MORALE_RESULT.md`: qué tocaste, salida de verificación 2/3, desviaciones. En el commit.
