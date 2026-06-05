# TomeTik / ToME — Walkthrough básico

Guía práctica para pasarse el juego: cómo no morir, en qué orden bajar a las
mazmorras y a qué jefes hay que matar. Sirve tanto para jugar tú como para
entender (y retunear) la ruta que sigue el bot **AuToME** (`^V`). Las
profundidades, jefes y recompensas salen de `lib/edit/d_info.txt` (autoritativo);
los nombres de monstruos/objetos van en inglés porque así aparecen en pantalla.

---

## 1. Cómo se gana

ToME es un roguelike: muerte permanente, niveles generados al azar. **El objetivo
final es matar a Morgoth.** En concreto:

1. **Sauron, the Sorcerer** (nivel 99) — vive en lo profundo de Angband. Hay que
   matarlo *antes* de poder acabar con su amo.
2. **Morgoth, Lord of Darkness** (nivel 100) — en el fondo de **Angband**
   (profundidad ~100 de la escalera principal). Matarlo = **ganar la partida.**

Más allá, como contenido opcional de super-endgame:

3. **Melkor, Lord of Darkness** (nivel 150) — en **The Void** (prof. 128-150). El
   jefe más duro del juego; sólo para personajes ya legendarios tras Morgoth.

Todo lo demás (las mazmorras secundarias) es **opcional**: se hacen por su jefe y
su botín para llegar a Angband más fuerte, pero no son obligatorias para ganar.

---

## 2. Supervivencia: las reglas que te mantienen vivo

- **Profundidad ≈ tu nivel.** Regla de oro: no bajes mucho más rápido de lo que
  subes de nivel. Si estás en la profundidad N, conviene tener nivel de personaje
  ≳ N. Bajar "a ciegas" mata.
- **Luz SIEMPRE.** Sin antorcha/linterna encendida no ves ni esquivas. Lleva
  antorchas/aceite de repuesto; equipa una nueva en cuanto la actual se apague.
- **Comida.** No dejes que el hambre llegue a *faint*: te paraliza en mal momento.
  Lleva 4-5 raciones; come en *Hungry*.
- **HP: cura o huye.** Ten siempre *Cure Light/Serious/Critical Wounds*. Si caes
  por debajo de ~1/3 de HP, **bebe o escapa**, no sigas peleando. Un *Phase Door*
  o *Teleport* salva vidas.
- **Identifica y equípate.** *Scroll of Identify* sobre armas/armaduras/varas antes
  de venderlas o ponértelas. Equípate siempre la mejor arma/armadura/luz.
- **Word of Recall** = atajo pueblo↔mazmorra. Te devuelve al pueblo (a vender,
  reabastecer, vender botín) y luego te baja al punto más profundo que alcanzaste.
  Lleva 2-3 siempre.
- **Escaleras = control.** Bajas por `>` (FEAT_MORE), subes por `<`. Si un nivel es
  mortal o un callejón, **regenéralo**: sube una escalera y baja otra (level
  scumming) o haz recall.
- **Cuidado mortal con los paralizadores.** Los *floating eyes* (y similares) te
  paralizan al golpearlos en melee; sin *Free Action* es una trampa de muerte.
  **No los toques** hasta tener Free Action; rodéalos.
- **Vende para subir de equipo.** El dinero del botín se convierte en pociones de
  cura, pergaminos y mejor equipo. Pasa por las tiendas de Bree a menudo al
  principio.

---

## 3. El orden de juego (la escalera + desvíos)

La columna vertebral son **cuatro mazmorras principales** que forman una escalera
de profundidad continua **1 → 127**. Bájalas en orden, subiendo de nivel:

| # | Mazmorra | Prof. | Para qué |
|---|----------|-------|----------|
| 1 | **Barrow-Downs** | 1–10 | Arranque. Sube a nivel ~10-12, haz oro y equipo básico. |
| 2 | **Mirkwood** | 11–33 | Cuerpo medio del juego temprano. |
| 3 | **Mordor** | 34–66 | Río de lava, cavernas; sube a nivel ~50-60. |
| 4 | **Angband** | 67–127 | El endgame: Sauron (~99) y **Morgoth (~100)**. |

Antes de empezar conviene **grindear cerca de Bree** (Barrow-Downs) hasta tener
unos niveles, equipo y un colchón de pociones/comida. No tengas prisa por bajar.

### Desvíos opcionales (mazmorras de quest)

Intercaladas por nivel, dan **un jefe único** (`FINAL_GUARDIAN`) y normalmente
**un artefacto/objeto** (`FINAL_ARTIFACT`/`FINAL_OBJECT`) en el fondo. Buenas para
fortalecerte de camino a Angband. Ordenadas por dificultad aproximada:

| Mazmorra | Prof. | Jefe del fondo | Recompensa |
|----------|-------|----------------|------------|
| **Orc Cave** | 10–22 | Azog | **Wand of Thrain** (vara útil pronto) |
| **The Old Forest** | 13–25 | Old Man Willow | — |
| **Sandworm Lair** | 22–30 | Sandworm Queen | su armadura |
| **Heart of the Earth** | 25–36 | Golgarach, the Living Rock | — |
| **The Maze** | 25–37 | Minotaur of the Labyrinth | **Steel Helm of Hammerhand** |
| **The Helcaraxe** | 20–40 | White Balrog | — |
| **The Land of Rhun** | 26–40 | Ulfang the Black | — |
| **The Small Water Cave** | 32–34 | The Watcher in the Water | — |
| **Cirith Ungol** | 25–50 | **Shelob** | — |
| **Moria** | 30–50 | **Durin's Bane** (un Balrog) | — |
| **Submerged Ruins** | 35–50 | Ar-Pharazon the Golden | **Toris Mejistos** |
| **Illusory Castle** | 35–52 | The Glass Golem | **Helm of Knowledge** |
| **Paths of the Dead** | 40–70 | Feagwath | **Doomcaller** |
| **The Sacred Land of Mountains** | 45–70 | Trone (Thunderlord) | armadura thunderlord |
| **Dol Guldur** | 57–70 | **The Necromancer** (Sauron débil) | **Ring of Durin** |
| **Erebor** | 60–72 | **Glaurung** | — |

Y el contenido extremo, sólo tras Morgoth:

| Mazmorra | Prof. | Jefe |
|----------|-------|------|
| **Mount Doom** | 85–99 | — (volcán, inmunes al fuego) |
| **Nether Realm** | 666–696 | Tik'srvzllat → **Ring of Phasing** |
| **The Void** | 128–150 | **Melkor, Lord of Darkness** |

> **Consejo:** no entres a una de quest a su profundidad de fondo sin estar a la
> altura. Una buena referencia es tener nivel de personaje cercano a la profundidad
> *máxima* de esa mazmorra antes de intentar a su jefe.

---

## 4. Ruta recomendada de principio a fin

1. **Niveles 1-10 — Barrow-Downs.** Grindea, junta oro, compra cura/comida/recall,
   equípate. Cuando vayas sobrado, baja al fondo (prof. 10).
2. **~Nivel 8-12 — desvío a Orc Cave** por la *Wand of Thrain* (un ataque a
   distancia temprano vale oro).
3. **Niveles 11-33 — Mirkwood.** Sigue la escalera. Desvíos por nivel: Old Forest,
   Sandworm Lair (armadura), The Maze (*Helm of Hammerhand*).
4. **Niveles 34-66 — Mordor.** Pelea dura. Desvíos potentes: Illusory Castle
   (*Helm of Knowledge*), Submerged Ruins (*Toris Mejistos*), y si te ves fuerte
   Cirith Ungol/Moria por experiencia y botín.
5. **Niveles ~57-70 — Dol Guldur** por el **Ring of Durin** (anillo muy bueno) y
   Paths of the Dead por **Doomcaller**. Erebor (Glaurung) si aguantas.
6. **Niveles 67-100 — Angband.** El tramo final. Baja con cuidado, cura abundante,
   *Free Action*, resistencias. Mata a **Sauron (~99)** y luego a **Morgoth
   (~100)**. → **Ganaste.**
7. *(Opcional, post-juego)* **The Void** → **Melkor (150)**. Sólo si quieres el
   reto definitivo.

---

## 5. Quests del pueblo (Bree)

Bree tiene quests del alcalde/edificios. Algunas dan recompensa, pero **ojo**: la
quest de los ladrones (rogues) del alcalde te **quita el equipo** mientras dura —
de poco interés y peligrosa si dependes de tu equipo. El bot las omite. Como
jugador, hazlas sólo si sabes en qué te metes; no son necesarias para ganar.

---

## 6. Cómo lo hace AuToME (y cómo retunearlo)

El bot (`^V` para arrancar, `^N` para pedir consejo al Oráculo) sigue exactamente
esta ruta. Está en **`lib/scpt/autoplay.lua`** como la tabla `autoplay_route`, que
es **dato puro y editable sin recompilar**:

```lua
{ dungeon = 4, plev = 1, depth = 10, name = "the Barrow-Downs" },  -- etc.
```

- `dungeon` = índice de `lib/edit/d_info.txt`.
- `plev` = nivel de personaje mínimo antes de intentarla (gate de *supervivencia*,
  más estricto que el "puede entrar" del juego). **Súbelo** si el bot muere por
  bajar pronto; **bájalo** si quieres que arriesgue más.
- `depth` = profundidad objetivo; alcanzarla marca la entrada como hecha.

El motor en C (`autoplay_objective`, en `src/cmd1.c`) coge la **primera** entrada
no terminada y para la que tienes nivel; al limpiar una mazmorra salta a la
siguiente vía recall. Otros knobs de comportamiento (cura/comida a llevar, oro
mínimo para comprar, evitar paralizadores, etc.) están en `autoplay_config_table`
del mismo fichero. La tabla de amenaza por monstruo (`autoplay_avoid_table`) marca
NPCs a no atacar (p. ej. Farmer Maggot).

> **Realidad del bot:** AuToME es *borg-lite* melee (sin magia ni táctica a
> distancia), así que es frágil — suele caer mucho antes de Angband. La ruta está
> completa hasta Morgoth/Melkor como objetivo, pero los `plev` profundos son
> estimaciones; afínalos con partidas reales.
