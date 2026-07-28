# Revisión del Arquitecto — K3: cierre de los bloqueantes de la auditoría (Sprint 1.6B)

Fecha: 2026-07-28
Rama: `arch/sprint-1.6b-k3` (base `arch/sprint-1.6b-k2` @ `3a2ccd2`)
Implementación: GPT-5.6 SOL (`codex -m gpt-5.6-sol`)
Revisión e integración: Arquitecto Jefe
Contrato: `docs/briefs/SOL_K3_P0_AUDITORIA_SPRINT_1.6B.md`
Informe del implementador: `docs/RESULT_SOL_K3_P0.md`

## Veredicto

**Aprobado con una corrección del revisor.** F-00 y F-01 quedan cerrados con la
semántica del contrato. F-02 llegó con un defecto que corregí yo (§3).

## 1. F-00 — capacidad de los consumidores de replay

Corregido en los **cuatro** drivers (`driver.hpp`, `skirmish.hpp`,
`skirmish_eco.hpp`, `skirmish_apertura.hpp`) con la línea acordada, en la rama
de replay y solo ahí. La cota `AI_MAX_COMMANDS` de la ruta de IA queda intacta.
Revisado el diff línea a línea: exactamente el contrato, sin desviaciones.

**Verificación fuerte del revisor — la prueba no es vacía.** No me bastó con
que el test pasara: revertí temporalmente el `resize` en `driver.hpp`,
recompilé bajo ASan y ejecuté. El binario **aborta** (exit 134) en el
`operator[]` endurecido de libstdc++ (`_GLIBCXX_ASSERTIONS`, activo en Debug).
Restaurado el fichero, vuelve a verde. Queda probado que
`test_replay_batch_limits_and_driver_capacity` reproduce de verdad el defecto y
no es un test que pasa por construcción.

Nota para futuras builds: en Release sin `_GLIBCXX_ASSERTIONS` el mismo caso es
un heap-buffer-overflow real (que es como SOL lo encontró en la auditoría), no
un abort limpio. La corrección es igual de necesaria en ambos perfiles.

## 2. F-01 — conservación de carga al redirigir GATHER

`step.hpp` implementa los tres casos del contrato mediante
`changes_loaded_resource`, después de todas las validaciones y sin tocar el
orden contractual de rechazos. Verificado que **no se añadió ningún campo**:
`RETURN` conserva `eco_assigned_deposit`, y tras el dropoff `need_reassign` es
falso, así que el aldeano marcha al depósito que pidió el jugador.
`SAVE_FORMAT_VERSION` 12 y `CHECKSUM_ALGO_VERSION` 7 intactos.

Enmienda normativa escrita por el Arquitecto en **SPEC-004 §18** (tabla por
casos + justificación de por qué no se sube versión). La redacción anterior
—`eco_state = SEEK` incondicional— queda explícitamente derogada.

## 3. F-02 — defecto encontrado por el revisor y corregido

SOL implementó la condición de frontera como una **disyunción**:

```cpp
g->eco_carry[i] > 0 || g->eco_state[i] == EcoState::HARVEST
                    || g->eco_state[i] == EcoState::RETURN;
```

`eco_carry` solo crece **una vez ya dentro** de `HARVEST`, así que el primer
tick que satisface la disyunción es **siempre** un aldeano *entrando* en
`HARVEST` con `carry == 0`. Lo confirma su propia salida:

```text
apertura C save-boundary: tick=142 citizen=7 state=1 deposit=5 carry=0 resource=0
```

Con eso, los `CHECK` post-load sobre `eco_carry` y `eco_carry_resource`
comparaban **0 con 0**: tautologías justo sobre los dos campos transitorios que
F-02 existe para blindar. Es el mismo vicio que el informe denunciaba, movido
de sitio.

Corrección aplicada (`transient = g->eco_carry[i] > 0`, que ya implica
`HARVEST`/`RETURN`). Frontera resultante, con carga parcial real cruzando el
save:

```text
apertura C save-boundary: tick=143 citizen=7 state=1 deposit=5 carry=5 resource=0 cont=b0fccab58d1cde4a
```

## 4. F-04 y F-06

- **F-04**: `CHECK(out.winner == 1u)` en el escenario contractual y en la prueba
  de doble corrida. Correcto.
- **F-06**: `make_apertura_state` devuelve `nullptr` sin los 12 depósitos
  reales, con pre-flight dedicado que aborta antes de los subtests. Resuelve la
  objeción del "CHECK blando" y queda confirmado en git.

## 5. Verificación independiente (ejecutada por el revisor, no reportada)

| Gate | Baseline del contrato | Medido por mí | Resultado |
|---|---|---|---|
| G1 selftest | `fefa48125dd35736` | `fefa48125dd35736` | bit-idéntico |
| G4 savetest state | `774316057e5667fb` | `774316057e5667fb` | bit-idéntico |
| G4 savetest cont | `d52ac0019700684f` | `d52ac0019700684f` | bit-idéntico |
| skirmish state | `3f64d3223b74d477` | `3f64d3223b74d477` | bit-idéntico |
| skirmish cont | `92ec9aa95374a429` | `92ec9aa95374a429` | bit-idéntico |
| Vectores dorados | 1074 casos | 1074 casos, 0 fallos | OK |
| `ctest` completo | 25/25 | **25/25**, 291,07 s | OK |
| ASan `replay_v3` | — | verde (rerun propio) | OK |
| Apertura A | `cf57ea3ca2266627` / `5b69fbcea73bb432` | `71774aaa9c166103` / `b2294197e9964ba5` | **cambio esperado** |

El cambio de la apertura (`end_tick` 12480 → 12292, `ai_executions` 624 → 615)
es consecuencia directa y buscada de F-01: cuando `ai_scan_economy` redirige un
donante que ya transporta un recurso hacia un depósito de otro, el contrato
nuevo intercala un `RETURN`. Eso corrige los stocks, y con stocks correctos las
decisiones posteriores caen antes. Resultado contractual intacto: `winner == 1`,
final muy por debajo de 36000 ticks, las cuatro fases observadas.

## 6. Hallazgo de diseño de gates (deuda, no bloqueante)

**Los checksums de G1, G4 y skirmish se imprimen pero no se aseveran en ningún
sitio** — lo comprobé por grep: ninguno de esos literales aparece en `tests/`,
`addons/` ni `CMakeLists.txt`. Consecuencia: **`ctest` en verde NO prueba
bit-identidad**. Toda la garantía de no-regresión de determinismo depende hoy de
que un humano lea la salida y la compare a mano con un baseline que vive en
documentos.

Propuesta para un sprint próximo: fijar los baselines como constantes
aserveradas en los propios gates, con un procedimiento explícito para
actualizarlas cuando un cambio de trayectoria esté justificado y documentado.
No lo hago ahora para no mezclarlo con el cierre de los bloqueantes.

## 7. Deuda que sigue abierta del informe de auditoría

Aceptada conscientemente, ninguna bloquea el cierre de K2/K3:

- **F-03** (media, operativa): `ai_skirmish_apertura` consume ~240 s de los
  ~291 s de la suite. Etiquetar como `integration/slow` y crear un smoke corto
  para PR.
- **F-05** (baja): `p1_built_military` marca cualquier edificio de Rome distinto
  del centro; con el catálogo actual coincide con el cuartel, pero un futuro
  edificio económico daría un falso positivo del DoD.
- **F-07** (baja): toda la evidencia funcional usa la semilla `20260724`.
- **F-08** (baja): bordes sin cubrir (`INVALID_CIV_ID`, `threshold-1`,
  `n_deposits == 0`, empates de distancia).
- **F-09/F-10** (baja, higiene): `RawCommand cmd{}` transitorio, comentario
  `/*preferred=*/`, semántica de `save_at == 0`, fixture legacy de 6 depósitos
  en `test_gather.cpp`.

## 8. Nota sobre el panel de auditoría

La lección operativa del panel del 2026-07-27 se confirma aquí: Claude Opus 4.7
y Qwen 3.7 Plus dieron el incremento por correcto; DeepSeek planteó el caso de
carga parcial y **GPT-5.6 SOL lo reprodujo y además encontró el overflow con
ASan**. La votación mayoritaria habría producido un falso "aprobado". Y en este
K3, la revisión humana del entregable de SOL encontró a su vez el aserto
tautológico de F-02. **Ninguna capa sola basta**: panel diverso + verificación
dirigida + revisión de arquitectura.
