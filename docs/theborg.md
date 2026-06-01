# Del autoexplore al "Borg" — automatizar el juego entero

> Notas (2026-05-31). Continuación natural de [autoexplore.md](autoexplore.md): si sobre
> el autoexplore apilas reglas (coger objetos, pelear, comer, curarse/huir, ir al pueblo
> a reabastecerse), ¿tienes un bot? Sí — y en el linaje Angband tiene nombre propio: el
> **Borg**.

## Qué es el Borg

El **Borg** lo escribió originalmente Ben Harrison para Angband: un módulo que se compila
con un flag (`ALLOW_BORG`) y **juega solo**, de principio a fin, tomando todas las
decisiones sin intervención humana. Hay borgs para muchísimas variantes del árbol
Angband. No es hipotético: es un camino muy trillado, y empieza justo donde acaba el
autoexplore.

Dato útil: el Borg no es solo un juguete. Se usa para **probar el balance** del juego —
lanzas N borgs y ves a qué profundidad mueren, qué builds sobreviven, etc. Automatizar tu
propio single-player es perfectamente legítimo.

## El espectro (no es un sí/no)

```
walk (1 paso) → run (pasillo) → autoexplore (1 objetivo: lo desconocido)
   → travel-to (1 objetivo elegido) → ... → Borg (todos los objetivos, sin humano)
```

- **Autoexplore** es **reactivo y mono-objetivo**: "ve a lo desconocido, párate si pasa
  algo".
- **Un bot** es **deliberativo y multi-objetivo**: cada turno tiene que *elegir entre
  metas que compiten* (¿exploro, peleo, como, huyo, voy al pueblo?).

Ese salto —de "una meta" a "arbitrar entre metas"— es el verdadero cambio de
arquitectura. Necesitas un **sistema de prioridades**: behavior tree, utility AI o una
máquina de estados que cada turno puntúe las opciones y ejecute la ganadora.

## Lo fácil vs. lo difícil

Las reglas que uno imagina (coger objetos, comer, curarse) **son la parte fácil**: tres
líneas cada una. Lo difícil —donde el Borg de Angband tiene *miles* de líneas— es el
**juicio** detrás de cada regla.

### "Atacar a enemigos de igual o menor nivel" → heurística que te mata

En Angband/ToME el peligro **no es proporcional al nivel del monstruo**:

- un *breeder* te desborda multiplicándose,
- un *unique* fuera de profundidad te one-shotea,
- un grupo te rodea,
- un monstruo con aliento/hechizo te mata a rango aunque sea "de bajo nivel".

El Borg no mira el nivel: calcula una **función de peligro** ("danger") que estima el
**daño esperado por turno de todo lo que tienes en vista a la vez**, contra tus HP, tus
escapes (teleport, curación) y tu posición. Decidir *"¿esta pelea es ganable?"* es el
problema central, y es difícil.

### "Huir cuando la salud está baja" → ¿huir hacia dónde?

Implica: ¿hay escaleras cerca? ¿un pergamino de teleport? ¿el pasillo de atrás está
despejado? Es **pathfinding bajo amenaza + gestión de recursos**, no un umbral de HP.

### "Volver al pueblo a comprar" → un sub-bot entero

Saber qué falta (gestión de inventario), pathfind a las escaleras, navegar las tiendas,
decidir qué comprar con tu oro, y volver.

## La conexión con autoexplore: todo son flow maps

El Borg reusa **la misma pieza** que sostiene el autoexplore: los **flow maps / Dijkstra
maps** (ver [autoexplore.md](autoexplore.md)). Le sirven para:

- "ir al monstruo más cercano",
- "ir al objeto / a las escaleras / a la tienda",
- "huir del peligro" (rodar *cuesta arriba*, alejándose de las amenazas).

Todo es el mismo flood-fill con distintas **fuentes**. Por eso autoexplore es el cimiento
natural: una vez tienes "rodar hacia/desde un conjunto de celdas", el bot son **capas de
decisión** encima de eso.

## Orden sano de implementación

1. **autoexplore** — rodar hacia lo desconocido + parar.
2. **travel-to** — rodar hacia un objetivo elegido. (El A* de `pathfind.c` brilla aquí.)
3. **acciones automáticas sueltas** — auto-pickup, auto-eat. Fáciles e independientes.
4. **el árbitro de decisiones** — utility/priority loop. *Aquí nace el bot de verdad.*
5. **la función de peligro** — lo que separa un bot que sobrevive de uno que muere en DL5.

## Referencias

- [RogueBasin — Borg (Angband)](https://www.roguebasin.com/index.php/Borg) — el Borg de Ben Harrison y descendientes.
- [APWBorg / Angband Borg](https://github.com/angband/angband) — variantes mantienen el módulo borg dentro del árbol.
- [RogueBasin — The Incredible Power of Dijkstra Maps](https://www.roguebasin.com/index.php/The_Incredible_Power_of_Dijkstra_Maps) — la pieza compartida.
- Ver también [autoexplore.md](autoexplore.md) en este mismo directorio.
