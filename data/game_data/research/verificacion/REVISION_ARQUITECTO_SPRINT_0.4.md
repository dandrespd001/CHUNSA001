# Revisión arquitectónica ADR-014 — fichas del slice

Fecha: 2026-07-22  
Responsable del veredicto: Arquitecto Jefe de CHUNSA  
Resultado: **PROMOVER 36a, 36b, 36c y 36d**

## Alcance y entradas

La revisión cubrió línea a línea las fichas corregidas de Egipto dinástico,
Roma, Mali y Tawantinsuyu. Se contrastaron con `VERIF_36a.md`,
`VERIF_36b.md`, `VERIF_36c.md`, `VERIF_36d.md`, `VERIF_RESUMEN.md` y
`CORRECCION_SPRINT_0.4.md`, además de un segundo pase independiente con acceso
web sobre fuentes institucionales, académicas y primarias.

El veredicto aplica el gate de SPEC-002 §12.1 y ADR-014: una ficha solo puede
promoverse cuando no conserva errores críticos, citas inventadas o bibliografía
falsa. La promoción no convierte las abstracciones de balance marcadas `DISEÑO`
en hechos históricos ni elimina las cautelas expresas de cada ficha.

## Veredictos por ficha

| Ficha | Veredicto | Fundamento y límites |
|---|---|---|
| `36a_egipto_dinastico.md` | **PROMOVER** | Se retiraron transliteraciones espurias, cronologías y términos incorrectos, y referencias no trazables. Las fuentes UCL Digital Egypt y Met Museum respaldan el alcance histórico usado. Las variantes regionales o de periodo siguen siendo obligatorias al extraer datos. |
| `36b_roma.md` | **PROMOVER** | Se corrigieron cronología, terminología militar, revuelta panonia, vidrio y minería. El segundo pase corrigió el localizador de Plinio a *Naturalis Historia* 33.70–76. Para cubrir la clase `artillery` del fixture se añadió `TEC-tormenta`, respaldada por el material oficial de la exposición *Legion* del British Museum; alcance, cadencia y dotación permanecen como diseño. |
| `36c_mali.md` | **PROMOVER** | Se eliminaron citas fabricadas, actores no identificables y el anacronismo vegetal; se corrigieron Songhai/Tombuctú y la URL de UNESCO. Los paquetes históricamente documentados se etiquetan `H`; detalles de extracción minera, escala y causalidad no se infieren sin fuente específica. Queda fuera del blob D1 del Sprint 0.4. |
| `36d_tawantinsuyu.md` | **PROMOVER** | Se corrigieron cronología, mit'a, quipus, antecedentes panandinos, q'eswa y bibliografía. Los puentes de cuerda pasan a `H` con el límite explícito de que la renovación anual solo se documenta aquí para Q'eswachaka. La política lingüística ya no presupone un quechua imperial uniforme. Queda fuera del blob D1 del Sprint 0.4. |

## Errata del registro de verificación

`VERIF_36b.md` propuso por error 33.96–97 como localizador del pasaje minero de
Plinio. El texto primario sitúa las galerías, el derrumbe de montes y la
conducción de agua en 33.70–76. La ficha promovida usa 33.70–76; los informes
`VERIF_*` se conservan sin reescritura como evidencia histórica del proceso y
la enmienda queda registrada también en `CORRECCION_SPRINT_0.4.md`.

## Comprobaciones de cierre

- Los cuatro bloques YAML embebidos cargan correctamente con un parser seguro.
- No quedan las referencias fabricadas o mal atribuidas detectadas en la
  verificación original.
- No quedan URLs marcadas `A VERIFICAR` ni el enlace roto de UNESCO Tombuctú.
- `status` es `promoted`, con revisor y fecha explícitos en las cuatro fichas.
- Egipto y Roma quedan autorizadas para derivar el fixture D1; Mali y
  Tawantinsuyu quedan canónicas para integración posterior.

## Huellas de las fichas promovidas

| Ficha | SHA-256 del Markdown promovido |
|---|---|
| `36a_egipto_dinastico.md` | `ac5daedce22ef60bdd70774879e29406d2f4797a21a5824d7a0431427b0e8f31` |
| `36b_roma.md` | `37a03d609513ee37dbb92312ca57439c3dcf850be5c25ae7ecaa43f58b40e3e0` |
| `36c_mali.md` | `82e4079b4346d078144ebe6b0ee7451981ea215f23f3653b0c72988890b2a48f` |
| `36d_tawantinsuyu.md` | `5e287bd01f0f68ec0277511e2ae6cc8944a1933303ff18abeb0add06bee2c6d1` |

Estas huellas identifican el snapshot revisado de las fichas, no el
`content_hash` CHDB definido por SPEC-002. El `content_hash` solo se obtiene al
compilar el conjunto D1 y tiene un dominio criptográfico y un propósito
distintos.
