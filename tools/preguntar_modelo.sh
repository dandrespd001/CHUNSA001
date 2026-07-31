#!/usr/bin/env bash
# Envoltorio de delegacion a OmniRoute (Sprint 1.27).
#
# POR QUE EXISTE. El `--timeout` de omniroute vale 30000 ms por defecto y NO
# tiene variable de entorno que lo cambie. Una peticion de auditoria real tarda
# entre 60 y 300 segundos, asi que el cliente cortaba la conexion ANTES de
# recibir la respuesta. El sintoma es traicionero: salida vacia, exit code 0, y
# los tokens SI se consumen en el proveedor. Se paga el trabajo y se tira el
# resultado, sin que nada avise.
#
# Lo detecto el Director al ver consumo de tokens en el panel de DeepSeek
# mientras aqui no llegaba nada.
#
# Uso:  ./tools/preguntar_modelo.sh <modelo> <fichero-de-prompt> [extra...]
# Ej.:  ./tools/preguntar_modelo.sh ds/deepseek-v4-pro brief.txt
#       ./tools/preguntar_modelo.sh ds/deepseek-v4-flash brief.txt --reasoning-effort high
set -euo pipefail

MODELO="${1:?falta el modelo, p.ej. ds/deepseek-v4-pro}"
PROMPT="${2:?falta el fichero con el prompt}"
shift 2

[ -r "$PROMPT" ] || { echo "no se puede leer el prompt: $PROMPT" >&2; exit 2; }

# 15 minutos. Generoso a proposito: un corte del cliente cuesta tokens reales.
TIEMPO_MS="${OMNIROUTE_TIMEOUT_MS:-900000}"

exec omniroute --timeout "$TIEMPO_MS" chat --model "$MODELO" --file "$PROMPT" "$@"
