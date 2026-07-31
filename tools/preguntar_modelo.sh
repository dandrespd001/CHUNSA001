#!/usr/bin/env bash
# Delegacion a modelos externos para CHUNSA (Sprint 1.27).
#
# ── LA REGLA DE CUENTAS, QUE MANDA SOBRE TODO LO DEMAS ──────────────────────
# El Director tiene cupo en MiniMax, DeepSeek y Claude, que son cuentas
# PERSONALES. La de GPT es COMPARTIDA y solo da acceso a Luna. Por tanto:
#
#   Luna NO es una ruta de trabajo. Es un recurso prestado.
#
# Se usa cuando aporta algo que las personales no dan, y se justifica al
# usarla. Todo lo demas sale de las cuentas propias, aunque cueste un poco mas
# de tiempo. Gastar cupo ajeno por comodidad no es una optimizacion.
#
# ── POR QUE EXISTE ESTE SCRIPT ──────────────────────────────────────────────
# `omniroute` usa un timeout de cliente de 30 000 ms y NO tiene variable de
# entorno que lo cambie. Una auditoria real tarda de 60 a 300 s, asi que el
# cliente cortaba antes de recibir nada: salida VACIA, exit code 0, y los
# tokens SI se consumian en el proveedor. Se pagaba el trabajo y se tiraba. Lo
# detecto el Director al ver consumo en el panel de DeepSeek mientras aqui no
# llegaba nada. Los prompts largos pasados como ARGUMENTO tambien fallan en
# silencio; aqui siempre van por `--file`.
#
# ── POR QUE FLASH Y NO PRO ──────────────────────────────────────────────────
#   · SWE-bench Verified: Flash 79,0 % contra Pro 80,6 %. 1,6 puntos.
#   · El reentrenado V4-Flash-0731 gana a V4-Pro-PREVIEW en los nueve
#     benchmarks de agente (Terminal-Bench 2.1: 82,7 contra 72,1).
#   · Pro cuesta 3,1x mas: $0,435/$0,87 contra $0,14/$0,28 por 1M.
# Comparable en calidad y un tercio de precio. Pro queda retirado.
#
# ── EL DIAL DE RAZONAMIENTO ─────────────────────────────────────────────────
# Flash no es un modelo peor, es un modelo con mando. Medido con la MISMA
# auditoria: sin thinking ~60 s; con `--reasoning-effort high`, 271 s y 27k
# tokens, y encuentra cosas que Pro no vio. Ojo: los tokens de razonamiento
# NO son gratis — esa auditoria costo 1,46x lo que la misma en Pro. Se usa
# `auditar` cuando importa la correccion sutil, no por costumbre.
#
# ── AVISO DE PRECIO POR FRANJA ──────────────────────────────────────────────
# DeepSeek cobra el DOBLE en horas punta: 9:00-12:00 y 14:00-18:00 de Pekin
# (UTC+8), o sea 01:00-04:00 y 06:00-10:00 UTC.
#
# Uso:
#   ./tools/preguntar_modelo.sh <prompt>            # DeepSeek Flash (personal)
#   ./tools/preguntar_modelo.sh <prompt> auditar    # Flash + razonamiento alto
#   ./tools/preguntar_modelo.sh <prompt> luna       # CUENTA COMPARTIDA: justificar
set -euo pipefail

PROMPT="${1:?falta el fichero con el prompt}"
MODO="${2:-rapido}"

[ -r "$PROMPT" ] || { echo "no se puede leer el prompt: $PROMPT" >&2; exit 2; }

TIEMPO_MS="${OMNIROUTE_TIMEOUT_MS:-900000}"

if [ "$MODO" != "luna" ]; then
    h=$(date -u +%H)
    if { [ "$h" -ge 1 ] && [ "$h" -lt 4 ]; } || { [ "$h" -ge 6 ] && [ "$h" -lt 10 ]; }; then
        echo "AVISO: franja PUNTA de DeepSeek (x2 de precio). Si no corre prisa, espera." >&2
    fi
fi

case "$MODO" in
  luna)
    # CUENTA COMPARTIDA. Ademas Luna se DESPLOMA en contexto largo: 41,3 % en
    # MRCR 512K-1M frente al 72,5 % de Terra. Aunque hubiera cupo de sobra,
    # seria mala eleccion para barrer el repo. Solo tareas CORTAS y acotadas.
    echo "AVISO: Luna es la cuenta COMPARTIDA de GPT. Usala solo si las" >&2
    echo "       personales (DeepSeek, MiniMax, Claude) no sirven, y di por que." >&2
    exec codex -m gpt-5.6-luna exec "$(cat "$PROMPT")"
    ;;
  auditar)
    exec omniroute --timeout "$TIEMPO_MS" chat \
        --model ds/deepseek-v4-flash --file "$PROMPT" --reasoning-effort high
    ;;
  rapido)
    exec omniroute --timeout "$TIEMPO_MS" chat \
        --model ds/deepseek-v4-flash --file "$PROMPT"
    ;;
  *)
    echo "modo desconocido: $MODO (usa rapido | auditar | luna)" >&2; exit 2
    ;;
esac
