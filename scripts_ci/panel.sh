#!/usr/bin/env bash
# Panel multimodelo de CHUNSA — revisión, investigación y sondeo de rutas.
#
# Uso:
#   panel.sh probe                  Qué rutas están vivas y cuáles navegan
#   panel.sh review <fichero|diff>  Revisión adversarial multimodelo
#   panel.sh research <fichero>     Investigación (el fichero contiene la pregunta)
#
# Lecciones incorporadas (sesión 2026-07-28):
#  - Las rutas caen y reviven: SIEMPRE sondear antes de confiar en una.
#  - Solo las rutas gemini-web/gweb navegan; qwen y deepseek NO.
#  - El proxy duplica fragmentos de streaming: hay que de-duplicar la salida.
#  - MiniMax necesita --allowedTools ANTES de -p, o el CLI se come el prompt.
#  - Un modelo sin web al que se le pide investigar inventa con tono seguro.

set -u
EP="http://127.0.0.1:20128/v1/chat/completions"
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$REPO/docs/research/panel/$(date +%Y%m%d-%H%M)"
DEDUP="$REPO/scripts_ci/panel_dedup.py"

# Rutas con web verificada (sondear igualmente: cambian)
WEB_ROUTES=("gemini-web/gemini-3.1-pro" "gemini-web/gemini-3.5-flash" "gweb/gemini-3.1-flash-lite")
# Rutas sin web, útiles como críticos de razonamiento
CRITIC_ROUTES=("zenmux-free/deepseek/deepseek-v4-pro" "qwen-web/qwen3.7-plus")

FORMATO='FORMATO OBLIGATORIO: maximo 120 lineas. Un hallazgo por linea: "[FUENTE] afirmacion densa con cifras -> [CHUNSA] implicacion concreta". Marca cada linea [V] verificado citando fuente, [I] inferencia, [?] no confirmado. Una linea [V] sin fuente citada es un fallo del encargo. PROHIBIDO: introducciones, resumenes, cortesias, repetir la pregunta. Tablas para numeros comparables. Si no puedes verificar algo marcalo [?]; NO lo inventes.'

ask() { # ask <ruta> <max_tokens> <prompt> -> stdout
  jq -nc --arg m "$1" --argjson mt "$2" --arg p "$3" \
    '{model:$m,messages:[{role:"user",content:$p}],max_tokens:$mt,stream:false}' \
  | curl -sS --max-time 900 -X POST "$EP" -H 'Content-Type: application/json' --data-binary @- \
  | jq -r '.choices[0].message.content // .error.message // "SIN RESPUESTA"'
}

cmd_probe() {
  echo "== Sondeo de rutas ($(date -Iseconds)) =="
  printf '%-42s %-6s %s\n' RUTA WEB MUESTRA
  for m in "${WEB_ROUTES[@]}" "${CRITIC_ROUTES[@]}"; do
    ( r="$(ask "$m" 80 'Busca en la web la version estable mas reciente de Godot Engine y su fecha. UNA linea. Si no puedes acceder a la web responde exactamente SIN_ACCESO_WEB.')"
      short="$(printf '%s' "$r" | tr '\n' ' ' | cut -c1-70)"
      case "$r" in *SIN_ACCESO_WEB*) w=no ;; *) w=SI ;; esac
      printf '%-42s %-6s %s\n' "$m" "$w" "$short" ) &
  done
  wait
  echo
  echo "-- MiniMax (WebFetch local) --"
  timeout 240 fish -c 'claude-minimax --allowedTools "WebFetch" -p "Invoca WebFetch sobre https://godotengine.org/ y di en UNA linea que version anuncia la portada. Si falla, di el error exacto."' 2>&1 | tail -3
}

cmd_review() {
  local target="$1"; mkdir -p "$OUT"
  local body
  if [ -f "$target" ]; then body="$(cat "$target")"; else body="$(git -C "$REPO" diff "$target")"; fi
  local prompt="Eres un revisor adversarial de un RTS determinista en C++20 (kernel sin float, checksum bit-exacto, save/replay). BUSCA FALLOS, no elogies. Prioriza en este orden: (1) roturas de determinismo -- float, reloj, orden de iteracion no determinista, entropia; (2) errores de correccion que un test verde no detectaria; (3) memoria: desbordamientos, indices sin cota; (4) contradicciones internas del propio texto; (5) deuda de diseno. Para cada hallazgo di como REPRODUCIRLO. Si algo esta bien, una linea y sigue.

MATERIAL:
$body

$FORMATO"
  echo "Revisando -> $OUT"
  for m in "${CRITIC_ROUTES[@]}" "${WEB_ROUTES[0]}"; do
    ( n="$(printf '%s' "$m" | tr '/.' '--')"; ask "$m" 8000 "$prompt" > "$OUT/review_$n.md"
      printf '  %-42s %s lineas\n' "$m" "$(wc -l < "$OUT/review_$n.md")" ) &
  done
  wait
  [ -x "$DEDUP" ] && python3 "$DEDUP" "$OUT"/review_*.md
}

cmd_research() {
  local qfile="$1"; mkdir -p "$OUT"
  local q; q="$(cat "$qfile")"
  echo "Investigando -> $OUT"
  for m in "${WEB_ROUTES[@]}"; do
    ( n="$(printf '%s' "$m" | tr '/.' '--')"; ask "$m" 8000 "$q

$FORMATO" > "$OUT/research_$n.md"
      printf '  %-42s %s lineas\n' "$m" "$(wc -l < "$OUT/research_$n.md")" ) &
  done
  wait
  [ -x "$DEDUP" ] && python3 "$DEDUP" "$OUT"/research_*.md
  echo "  MiniMax (WebFetch, sintesis larga):"
  timeout 2400 fish -c "claude-minimax --allowedTools 'WebFetch,Read,Write' -p 'Investiga lo que pide $qfile usando WebFetch (WebSearch NO funciona aqui). Guarda la sintesis en $OUT/research_minimax.md respetando el formato de marcas [V]/[I]/[?] con fuente citada. No inventes cifras que no hayas leido.'" >/dev/null 2>&1
  echo "  hecho"
}

case "${1:-}" in
  probe)    cmd_probe ;;
  review)   cmd_review "${2:?falta fichero o rango de diff}" ;;
  research) cmd_research "${2:?falta fichero con la pregunta}" ;;
  *) sed -n '2,14p' "${BASH_SOURCE[0]}"; exit 1 ;;
esac
