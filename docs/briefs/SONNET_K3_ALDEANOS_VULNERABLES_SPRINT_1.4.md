# BRIEF K3 — Aldeanos vulnerables + partida con economía (Sonnet · Sprint 1.4, cierre de kernel)

Implementa la **enmienda SPEC-004 §7.1** (`docs/specs/SPEC-004_SISTEMAS_PARTIDA.md`), decisión
del Director: los aldeanos (`unit_class==3`) pasan a ser objetivo válido de combate, lo que
habilita que una partida CON economía termine por conquista (la condición de victoria v1 de
SPEC-005 §6 queda alcanzable con aldeanos presentes). Cambio acotado pero delicado: toca el
combate y el balance. Lee §7.1 entera antes de tocar nada.

## Rama y alcance
- Rama `sonnet/k3-aldeanos-vulnerables` desde `main` (HEAD, con K1+K2 de la IA). Jamás toques
  `main`.
- Archivos esperados: `step.hpp` (los 2 guards de targeting + la rama RPS), tests
  (test_combat/test_economy afectados + uno nuevo del comportamiento), `skirmish.hpp` o un
  escenario nuevo (§ más abajo). Regen de los checksums golden de los escenarios afectados.

## Los cambios exactos (§7.1)
1. **`combat_system` targeting** (~línea 857): el guard `if (g.unit_class[j] > 2 &&
   g.entity_kind[j] != 1u) continue;` excluye hoy a los aldeanos. Cámbialo para que un aldeano
   enemigo (unit_class==3, entity_kind==0) SÍ sea objetivo. Ojo: los edificios (entity_kind==1)
   siguen siendo objetivo; ninguna entidad viva enemiga debería quedar excluida ahora — pero
   razona el guard con cuidado (no dejes fuera edificios ni metas dentro a los propios).
2. **`aggro_system` targeting** (~línea 945): mismo guard, mismo cambio.
3. **Guards de ATACANTE intactos** (~líneas 676, 823, 909): `if (g.unit_class[i] > 2) continue;`
   se MANTIENE — los aldeanos son vulnerables pero NO atacan ni persiguen. Verifícalo.
4. **RPS contra aldeano** (~línea 874): hoy `rps_mult_bp(g.unit_class[i], g.unit_class[best])`
   con la tabla 3×3 haría OOB si `g.unit_class[best]==3`. Añade una rama (patrón exacto de
   `rps_mult_vs_building_bp`): si el objetivo es aldeano → ×1.0 (10000 bp) neutro. Documenta
   como balance v1 ajustable. NO toques la tabla 3×3 existente.

## Escenario de partida con economía (el valor de este cierre)
Añade un escenario (extiende `skirmish.hpp` con una variante, o un `skirmish_eco`) que incluya
aldeanos reales (economía) y que TERMINE por conquista en <36000 ticks, determinista. Es la
demostración de que ahora una partida completa con economía concluye. Verifica: dos corridas
idénticas (mismo winner/tick/checksums), save/continuar, replay bit-exacto. Documenta el tick de
fin y el ganador. (Puedes mantener el skirmish militar de K2 intacto como test de regresión.)

## Determinismo y regen de golden
- El cambio es determinista por construcción (solo amplía el conjunto de objetivos; sin RNG
  nuevo, sin float, sin heap — mantén el régimen). NO es bump de dominio de checksum (los campos
  hasheados no cambian). Es cambio de TRAYECTORIA: los tests que fijan checksums de escenarios
  con aldeanos al alcance de enemigos (showcase, economía+combate) se regeneran. Los escenarios
  SIN aldeanos-en-peligro deben quedar bit-idénticos — verifícalo con dump pre/post.
- Espera que el showcase cambie (los 120 aldeanos amarillos ahora pueden morir si el combate los
  alcanza): eso es CORRECTO y esperado, documéntalo.

## Gates (§8.4 de SPEC-005 + los del combate)
Golden 1074/1074 (aritméticos, no deben cambiar) · G1 alloc_delta=0 · G3/G4/G5 · el skirmish
militar de K2 sigue OK (regresión) · el skirmish con economía nuevo termina por conquista ·
ctest completo verde · build -Werror · cero float/heap en las rutas nuevas de step.

## Reglas duras
Append-only; iteración ascendente; cero float/heap; térmica nice -n 19 -j2 un build a la vez;
conservador ante huecos + desviación numerada. GameState SIEMPRE en heap en tests. NO merges a
main.

## Entrega
Commits atómicos + `docs/briefs/SONNET_K3_ALDEANOS_VULNERABLES_RESULT.md` (desviaciones, gates
con checksums, el tick de fin y ganador del skirmish con economía, y confirmación de que los
escenarios sin aldeanos-en-peligro quedan bit-idénticos). El Arquitecto revisa (+ Opus audita
si el cambio de combate lo amerita) e integra.
