# GD Optimizer v2 — notas técnicas

## Arquitectura
- `main.cpp`: crea un núcleo que reparte un tick (4 veces/s) a los módulos.
- `core/Module.hpp`: interfaz `Module` + `Registry`.
- `core/Config.hpp`: acceso a los ajustes de `mod.json`.
- `modules/*.cpp`: un archivo por optimización. Cada una tiene su interruptor.
- Regla de diseño: **nada de trabajo por frame salvo hooks mínimos**. Lo demás
  corre en el tick, no en el bucle de render.

## Módulos implementados

### FpsLimiter
Baja el intervalo de animación de `CCDirector` solo en pausa, menús y ventana
sin foco. Un frame en pausa/menú no cambia nada en pantalla pero gasta CPU
(visit de la escena, actualización de nodos) y GPU (redibujado). No actúa nunca
en un nivel ni en el editor, ni en la pantalla de carga (carga un paso por
frame). Nunca sube los FPS por encima del valor del juego u otro mod.

### TextureTrim (experimental, apagado por defecto)
`removeUnusedTextures` libera solo texturas cuya única referencia es la caché.
Se recargan solas al necesitarlas, por lo que el aspecto no cambia.

### FrameProfiler
Hook de `CCDirector::drawScene` (mide el tiempo de frame, incluye la espera de
vsync) y de `CCSpriteBatchNode::draw` (cuenta batches y quads). Con el
profiler apagado cada hook cuesta una comparación. Sirve para ver cuántos
draw calls y quads se envían realmente y detectar el peor frame.

## Lo que NO está implementado, y por qué

### Batching avanzado de sprites
GD ya agrupa los objetos en `CCSpriteBatchNode` (un draw call por batch, no por
objeto). Lo que queda es: batches separados por capa z y modo de mezcla
(necesarios para que el orden de dibujado y el glow salgan idénticos), y el
coste en CPU de actualizar el transform de cada sprite. Fusionar batches entre
capas obliga a reordenar quads, y eso puede cambiar el orden de mezcla, es
decir, el aspecto. Un renderer propio (el enfoque de Bismuth) es un proyecto
grande con riesgo visual. El profiler te dice cuántos batches y quads hay
antes de decidir si merece la pena.

### Culling de geometría
GD ya oculta objetos fuera de cámara por secciones
(`PlayLayer::updateVisibility`). Lo que sigue activo fuera de pantalla es
lógica de juego (triggers, movimiento, colisiones); saltársela cambia el estado
del nivel. Añadir un culling propio en `CCNode::visit` costaría más por nodo
(se ejecuta miles de veces por frame) de lo que ahorra.

### "Ganancia masiva de FPS"
Con hooks sobre el motor y sin tocar la estética, las ganancias realistas son
moderadas y concentradas en pausa/menús y en recursos. Ganancias grandes dentro
del nivel requieren reescribir el renderer.

## Demonlist
No puedo garantizar que la Demonlist lo acepte: lo decide su staff y sus reglas
cambian. Lo que sí hago es mantener el mod lejos de cualquier cosa que suelen
vigilar: no toca física, triggers, input, aleatoriedad ni lo que se muestra
(no oculta objetos, no quita efectos, no cambia FPS dentro del nivel). Antes de
usarlo en un récord, pregunta al staff de la lista o revisa sus reglas actuales
sobre mods.
