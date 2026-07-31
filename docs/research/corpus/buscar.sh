#!/usr/bin/env bash
# Consulta los repositorios abiertos que SÍ responden (ver FUENTES.md).
#   ./buscar.sh archive "blast furnace"     -> textos escaneados
#   ./buscar.sh openlib "metallurgy"        -> catálogo de libros
#   ./buscar.sh doaj    "cement chemistry"  -> artículos de acceso abierto
set -euo pipefail
q="${2:?falta la consulta}"
enc=$(python3 -c "import urllib.parse,sys;print(urllib.parse.quote(sys.argv[1]))" "$q")
case "${1:?falta el repositorio}" in
  archive)
    curl -s -m 40 "https://archive.org/advancedsearch.php?q=${enc}+AND+mediatype%3Atexts&fl%5B%5D=identifier&fl%5B%5D=title&fl%5B%5D=year&rows=20&output=json" \
    | python3 -c "
import sys,json
d=json.load(sys.stdin)['response']
print('encontrados:', d['numFound'])
for x in d['docs']:
    print(f\"{x.get('year','????')}  https://archive.org/details/{x['identifier']}\")
    print(f\"        {str(x.get('title'))[:90]}\")" ;;
  openlib)
    curl -s -m 40 "https://openlibrary.org/search.json?q=${enc}&limit=15" \
    | python3 -c "
import sys,json
for x in json.load(sys.stdin).get('docs',[]):
    print(f\"{x.get('first_publish_year','????')}  {x.get('title','')[:80]}\")" ;;
  doaj)
    curl -s -m 40 "https://doaj.org/api/search/articles/${enc}?pageSize=10" \
    | python3 -c "
import sys,json
for x in json.load(sys.stdin).get('results',[]):
    b=x['bibjson']; print(b.get('year','????'), '|', b.get('title','')[:90])" ;;
  *) echo 'repositorios: archive | openlib | doaj'; exit 1 ;;
esac
