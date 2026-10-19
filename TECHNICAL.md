# GD Optimizer v2.1 — análisis técnico

Documento de diseño y viabilidad. Explica cada sistema pedido: cómo
funcionaría, cómo interactúa con CPU, memoria y GPU, y por qué se incluye o no.

## Contexto técnico
- GD 2.2081 usa una versión modificada de cocos2d-x 2.x con OpenGL. El contexto
  lo crea GLFW y su versión depende de la máquina (no se puede asumir 4.3).
- Los objetos del nivel son nodos `CCSprite` agrupados en `CCSpriteBatchNode`:
  cada batch es un draw call con los quads de muchos sprites de la misma textura.
- Cada frame, la CPU: ejecuta la lógica (triggers, movimiento, colisiones),
  actualiza transformaciones de sprites y recorre el árbol de nodos (`visit`).
  La GPU: dibuja los batches, con mezcla (blending) y glow.
- Por eso el módulo `GpuProbe` y el `FrameProfiler` existen: antes de diseñar,
  hay que saber si el cuello de botella es CPU o GPU en TU máquina y TUS niveles.

## 1. Culling con compute shaders
**Diseño teórico:** un SSBO con la caja (AABB) de cada objeto; un compute
shader (grupos de 64 hilos) compara cada caja con el rectángulo de cámara y
escribe los visibles con un contador atómico; después `glMultiDrawElementsIndirect`
dibuja solo esos.
**Por qué no sale a cuenta aquí:**
- Requiere OpenGL 4.3. Muchos contextos de GD no lo garantizan (ver GpuProbe).
- La CPU sigue calculando y subiendo las posiciones cada frame (triggers de
  movimiento): el coste que se quiere evitar no desaparece.
- El culling de GD ya es por secciones y solo toca lo cercano a la cámara; el
  trabajo ahorrado es pequeño.
- Si la lógica necesita el resultado, hay que leerlo de vuelta de la GPU
  (sincronización que bloquea la CPU o añade un frame de latencia).
- Los objetos no viven en memoria de GPU: habría que reconstruir el renderizado
  entero para que el culling GPU tenga sentido.
**Conclusión:** en un juego con miles (no millones) de objetos, es muy probable
que sea más lento que el culling por CPU actual.

## 2. Instancing avanzado
**Diseño teórico:** un buffer de instancias (transformación, color, UV) y un
`glDrawElementsInstanced` por tipo de geometría.
**Por qué no reduce lo que parece:** GD ya dibuja cada batch con una sola
llamada; los draw calls son decenas, no miles. El instancing reduce datos de
vértices, no draw calls adicionales. Además GD separa batches por capa z y modo
de mezcla porque el orden de dibujado y el glow dependen de ello: fusionarlos
cambia el aspecto, que es justo lo que no se puede tocar. Los cuellos de botella
típicos son la CPU (transformaciones, visit) y el relleno de píxeles (overdraw
de glow), no el número de draw calls.

## 3. Hooks de bajo nivel en el pipeline de Cocos2d
Los hot paths (`CCNode::visit`, `CCSprite::updateTransform`) se ejecutan decenas
de miles de veces por frame. Un hook añade una redirección (trampolín) a cada
llamada; ese coste suele superar lo que se ahorra salvo que el hook elimine
trabajo grande. Por eso este mod solo hookea puntos de baja frecuencia
(`drawScene`, una vez por frame) y solo para medir. Cualquier hook más invasivo
debe justificarse con datos del profiler.

## 4. Memory pooling y cero asignaciones
GD es C++ con conteo de referencias, no tiene recolector de basura: no existen
pausas de GC. Los microcongelamientos vienen de carga de texturas, guardado a
disco, audio y picos de lógica. Un pool propio no puede sustituir las
asignaciones del juego sin reemplazar el asignador global (`new/delete`,
`malloc/free`), algo inseguro desde un mod porque el juego libera objetos con
su propio runtime. Dentro de este mod no hay asignaciones por frame (los
contadores del profiler son enteros estáticos).

## 5. Scheduling de nodos decorativos
Saltarse o espaciar la actualización de decoraciones animadas cambia su estado
al reaparecer en pantalla (fase de la animación, partículas), es decir, su
aspecto. Solo sería invisible si la animación se pudiera reconstruir de forma
analítica a partir del tiempo, algo que no ocurre con acciones y partículas del
motor.

## 6. Ajustes en el juego
Implementado: Geode genera la interfaz desde `mod.json`; cada módulo tiene su
interruptor independiente.

## Qué sí se hace, y la hoja de ruta
1. **Hecho:** límite de FPS fuera de la partida, profiler (FPS, batches, quads,
   peor frame) y GpuProbe (capacidades de la GPU).
2. **Siguiente paso:** medir niveles pesados. Si el frame medio es alto con la
   GPU poco cargada, el cuello es CPU; si sube con el tamaño de pantalla o el
   glow, es la GPU.
3. **Con datos:** optimizaciones puntuales y seguras sobre el cuello real.
4. **Solo si hace falta:** un renderer propio es un proyecto aparte (semanas de
   trabajo), con riesgos: orden de mezcla idéntico, compatibilidad con otros
   mods, drivers y software de captura (OBS).

## Demonlist
No puedo garantizar su aprobación: lo decide el staff y sus reglas cambian. Un
renderer propio sería lo más probable de llamar su atención. Antes de usar
cualquier mod en un récord, consulta al staff.
