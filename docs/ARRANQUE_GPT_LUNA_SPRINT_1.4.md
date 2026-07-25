# ARRANQUE — ChatGPT (Luna Max) · Sprint 1.4: modo skirmish jugable + victoria/derrota

Actúas como **desarrollador gráfico/Godot** de CHUNSA. El Arquitecto (Claude) mantiene los
contratos y revisa; tú implementas la parte JUGABLE del gate de fase: una partida humano
(owner 0) vs IA (owner 1) que termina en victoria o derrota. El kernel de la IA y la condición
de victoria YA están en main (`afbe66f`): la IA de 3 capas real, `game_over`/`winner` en el
GameState, aldeanos vulnerables (una partida con economía termina por conquista).

## Esto NO es solo UI: añades el JUGADOR IA al bucle de simulación
A diferencia de los sprints 1.2/1.3, aquí tocas `sim_loop` para correr la IA de owner 1. Sigue
siendo el adaptador (NO el kernel `core/`), pero es el punto más delicado: el ciclo de la IA
debe copiar EXACTAMENTE el patrón del kernel o romperás la consistencia.

## Lee ANTES de tocar nada
1. `docs/specs/SPEC-005_IA_OPONENTE.md` §8.6 (experiencia jugable) y §2 (andamiaje de IA).
2. `docs/REVISION_SPRINT_1.4_KERNEL.md` — qué está en main.
3. `addons/chunsa_sim/core/include/chunsa/ai_stub.hpp` — la API del `AiJobBox`
   (`ai_box_init`/`ai_should_dispatch`/`ai_dispatch`/`ai_execute`/`ai_due`/`ai_commit`/
   `ai_stalled`) y `AiRuntimeV1`.
4. `addons/chunsa_sim/core/include/chunsa/skirmish.hpp` y `skirmish_eco.hpp` — cómo el CLI
   arma una partida con IA (catálogo, setup por comandos, el ciclo). Úsalos de referencia para
   tu escenario jugable.
5. `addons/chunsa_sim/gdextension/chunsa_sim_node.cpp/.h` — tu archivo.

## Rama y reglas duras
- Rama `gpt/skirmish-jugable-1.4` desde `main`; commits atómicos; nada directo a `main`.
- **JAMÁS toques `addons/chunsa_sim/core/`**. Copias sus funciones y patrones, no las modificas.
- ⚠️ Térmica: builds `nice -n 19 -j2`, Godot `nice -n 19`, uno a la vez.
- Verificación headless obligatoria (abajo), exit 0, sin `CHUNSA ERROR`. Captura si puedes.
- Entrega con `docs/briefs/GPT_SKIRMISH_JUGABLE_1.4_RESULT.md`.

## El ciclo de la IA en sim_loop — patrón LITERAL del kernel (cópialo, no lo inventes)
Añade al nodo miembros `chunsa::AiJobBox ai_box; chunsa::AiRuntimeV1 ai_rt{0,0};` e inicialízalos
en `_ready` tras `gs_init`: `chunsa::ai_box_init(ai_box, 1);` (owner 1 = IA). En `sim_loop`,
DENTRO del cálculo del batch de cada tick, ANTES de `chunsa::step(...)`, añade el pump de la IA
(exactamente el patrón de `driver.hpp::drive`, `o.with_ai==true`):
```cpp
// t = gs->tick, batch/n ya contienen showcase + comandos del jugador.
if (chunsa::ai_should_dispatch(ai_box, t)) chunsa::ai_dispatch(ai_box, t, ai_rt);
if (ai_box.state == chunsa::AiJobState::DISPATCHED) chunsa::ai_execute(ai_box, *gs);
if (chunsa::ai_stalled(ai_box, t)) chunsa::ai_execute(ai_box, *gs);
if (chunsa::ai_due(ai_box, t)) {
    for (uint32_t k = 0; k < ai_box.result_count && n < batch.size(); ++k)
        batch[n++] = ai_box.result[k];
    chunsa::ai_commit(ai_box, ai_rt);
}
```
(Es el mismo pump que corre el CLI. No cambies la fase ni el orden; la IA emite MOVE_TO/
TRAIN_UNIT/etc. con `emitter=1`, que el kernel valida.)

## Entregable
1. **Escenario jugable**: una partida humano (owner 0) vs IA (owner 1). Puedes reemplazar o
   añadir junto al showcase actual un setup por comandos donde ambos jugadores arrancan
   simétricos (centro + cuartel + un ejército + aldeanos), owner 0 controlable por el jugador
   y owner 1 movido por el `AiJobBox`. Con aldeanos vulnerables (ya en main) la partida puede
   terminar. Usa `skirmish_eco.hpp` como plantilla del catálogo/setup (mismos record_id del
   blob real). El jugador ya tiene todo el HUD del 1.2/1.3 (construir, entrenar, mover, grupos).
2. **Exposición de victoria**: añade `game_over`/`winner` al `DemoSnapshot` y cópialos en
   `sim_loop` (`s->game_over = gs->game_over; s->winner = gs->winner;`).
3. **Pantalla de victoria/derrota**: cuando `snap_curr.game_over == 1`, un overlay en `_draw`:
   "VICTORIA" si `winner == 0` (el humano), "DERROTA" si `winner == 1` (la IA), "EMPATE" si
   `winner == 0xFF`. Congela o marca visualmente el fin. Un print de diagnóstico
   (`CHUNSA game_over winner=N tick=T`) para la verificación headless.
4. Mantén funcionando todo lo del 1.2/1.3 (HUD, cámara, minimapa, grupos, producción).

## Verificación headless obligatoria
```
nice -n 19 ./third_party_build/Godot_v4.7.1-stable_linux.x86_64 --headless --path demo --quit-after 4000
```
La IA (owner 1) debe actuar (verás sus edificios/unidades evolucionar) y, si el escenario está
bien diseñado para converger, `CHUNSA game_over winner=...` debe aparecer antes de 4000 ticks
(si no converge tan rápido, sube `--quit-after` para confirmar que SÍ termina; documenta el
tick). Sin `CHUNSA ERROR`, exit 0. Nota: la IA de la demo NO necesita ser bit-exacta con el CLI
(la demo es presentación); solo debe FUNCIONAR — la IA juega y la partida puede terminar.

## Nota importante
El `player_epoch` ya se fija a 3 en `_ready` (del 1.2). El catálogo real tiene los cuarteles
(`chariotry_stable`/`castra_barracks`) y los aldeanos (`work_crew`) — úsalos para el setup
simétrico. Si algo del kernel te parece que falta y no es binding mecánico, PÁRATE y repórtalo.

Español para docs/commits. Determinismo sagrado (la IA copia el patrón literal), la UI nunca
decide el juego, parar > improvisar.
