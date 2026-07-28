# OpenRA — GitHub README + wiki
**URLs:** https://github.com/OpenRA/OpenRA · https://github.com/OpenRA/OpenRA/wiki
**Fetched:** 2026-07-28 (WebFetch)
**Status:** parcial — wiki/Architecture no encontrada, wiki raíz no expone lockstep.

## Motor
- "A Libre/Free Real Time Strategy game engine supporting early Westwood classics."
- Reescritura en C# de Red Alert, Tiberian Dawn, Dune 2000.
- .NET (solución `OpenRA.slnx`), SDL + OpenGL para presentación.
- Proyectos: `OpenRA.Game`, `OpenRA.Server`, `OpenRA.Mods.Common`, `OpenRA.Mods.Cnc`, `OpenRA.Mods.D2k`.
- API Lua para misiones + YAML para traits (modding data-driven).

## Wiki raíz
- Secciones: Community, Guides (gameplay, modding, dedicated servers), Development, Architecture.
- "Architecture and Notes" incluye: source organization, mod manifests, sprite sequences, palettes, coordinate systems, traits, Lua scripting, veterancy.
- El README marca el "Hacking" wiki como "now very outdated".
- **No hay entrada explícita a lockstep/sync en la raíz.** Detalles viven en el código (`OpenRA.Network`, `OpenRA.Server`) o en páginas "Hacking" / "Dedicated Server" del wiki.

## Limitación
- No accedí a deep wiki pages por 404 y porque WebFetch no me devolvió la página Architecture. La afirmación de OpenRA sobre determinismo es **conocimiento previo, no leído** — debe quedar [I] o [?] en la síntesis.
