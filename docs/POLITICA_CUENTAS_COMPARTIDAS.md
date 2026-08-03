# Regla dura: NO se resetea el uso de GPT

Orden del Director, 2026-08-03. Vinculante.

## Qué está prohibido, y por qué exactamente

El comando `omniroute resilience reset` (por proveedor o con `--all-cooldowns`)
**borra el estado de cortacircuitos y enfriamiento** que OmniRoute mantiene.

Ese estado es lo que hace que, cuando un proveedor responde *«All accounts have
exhausted their quota (reset after 15h 58m)»*, el enrutador **deje de
intentarlo** hasta que de verdad se recupere.

Reseteándolo, OmniRoute vuelve a lanzar peticiones contra una cuenta agotada. En
GPT —que es **cuenta compartida**— eso significa consumir cupo ajeno contra un
proveedor que ya dijo que no.

También quedan prohibidos `codex login` y `codex logout`: manipulan la sesión de
una cuenta que no es nuestra.

### El comando `/usage` de codex (orden del Director, 2026-08-03)

Dentro de la interfaz de codex, el comando `/usage` **permite resetear el
contador de uso**. No debe invocarse desde aquí bajo ninguna circunstancia.

El motivo es el mismo que el del reset de cooldowns, y agravado: el contador de
uso es lo único que le dice al Director **cuánto se ha consumido de una cuenta
que comparte con otras personas**. Borrarlo no libera cupo — sólo destruye la
evidencia de cuánto se gastó, y deja a los demás usuarios sin forma de saber
por qué se quedaron sin él.

En la práctica esto significa: **nunca ejecutar codex en modo interactivo desde
aquí**, y nunca pasar `/usage` dentro de un prompt. La delegación se hace
siempre con `codex ... exec`, que no abre la interfaz de comandos.

## Cómo está impedido, no sólo desaconsejado

`.claude/settings.json` del proyecto lleva una regla `deny` para esos comandos.
No es una nota que alguien deba recordar: es un bloqueo. Si alguna vez hiciera
falta ejecutarlos, tiene que hacerlo **el Director, a mano**, sabiendo lo que
hace.

## Lo que SÍ se puede hacer

Consultar no cambia nada, y es lo que hay que usar para decidir:

| Comando | Para qué |
|---|---|
| `omniroute usage quota --check` | ¿hay cupo para una petición más? |
| `omniroute usage analytics` | consumo agregado |
| `omniroute cost --period 1d` | gasto real |

## Por qué existe este documento: el incidente del 2026-08-03

Esta política se escribió **y en el mismo comando se violó**. Queda escrito
porque la causa es reutilizable y el daño fue real.

Al redactar el documento con un *heredoc* de shell, **zsh interpretó los
backticks del texto como sustitución de comandos** y ejecutó lo que había
dentro — que eran, precisamente, los dos comandos que el documento prohibía:

- `omniroute resilience reset` → **falló** por faltarle `--provider`. No reseteó
  nada. Fue suerte, no prudencia.
- `codex login` → **se ejecutó**. Levantó el servidor OAuth en el puerto 1455,
  invalidó la sesión existente y dejó a `codex` en **401 Unauthorized**, con
  `~/.codex/auth.json` borrado y sin copia. Luna quedó inaccesible hasta que el
  Director volvió a iniciar sesión.

### La lección, corregida

Ya había una nota en memoria sobre los backticks en zsh, tras **tres**
incidentes previos. Decía: *«mensajes de commit y briefs largos siempre por
fichero»*. **Era demasiado estrecha.** Las tres veces anteriores el daño fue
silencioso —texto mutilado en un commit, una palabra perdida en un brief—. Ésta
ejecutó comandos.

La regla correcta es más amplia:

> **Nunca poner un backtick en nada que vaya a pasar por el shell.** Ni siquiera
> dentro de un heredoc con delimitador entrecomillado, si en la misma invocación
> hay algo más que sí se expande.

Para escribir documentación se usa la herramienta de escritura de ficheros. **El
shell es para ejecutar, no para redactar.**

### Y una segunda lección, sobre las copias

No había respaldo de `~/.codex/auth.json`. Un fichero de credenciales que sólo
existe en un sitio es un fallo esperando ocurrir, y ocurrió.

## La regla de fondo

**Un límite alcanzado es información, no un obstáculo que sortear.** Cuando un
proveedor dice que no queda cupo, la respuesta correcta es cambiar de ruta —hay
matriz para eso— o esperar. Nunca borrarle la memoria al enrutador para que
vuelva a preguntar.
