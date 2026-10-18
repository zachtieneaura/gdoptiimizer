# Notas técnicas: qué hace cada optimización y por qué

## Lo que sí está implementado

### Límite de FPS en pausa / menús / sin foco  (punto 4, parte segura)
GD renderiza a la misma velocidad en pausa que jugando. Un frame en pausa no
cambia nada en pantalla, pero cuesta CPU (actualizar nodos, recorrer
escena) y GPU (redibujar). Bajar el intervalo de animación de `CCDirector`
solo en esos estados recorta ese gasto sin tocar shaders, glow ni texturas.
- Nunca se limita en la pantalla de carga (carga un paso por frame).
- Nunca se sube el FPS por encima del que tenga el juego u otro mod.
- Si otro mod cambia el FPS mientras el límite está activo, se respeta.

### Liberar texturas sin referencias  (punto 3, parte segura, experimental)
`removeUnusedTextures` libera solo texturas con una única referencia (la de la
caché). Se pueden volver a cargar al instante cuando hagan falta, así que el
aspecto no cambia. Va desactivado por defecto porque la ganancia es pequeña y
no puedo garantizar que ningún recurso del juego dependa de esa referencia.

### Registro de picos de frame  (punto 3, herramienta)
Mide el `dt` de cada frame del nivel y avisa en el log si supera un umbral. Es
la forma fiable de saber qué provoca un stutter antes de intentar arreglarlo.

### Menú de ajustes  (punto 5)
Geode genera la interfaz en el juego a partir de `settings` en `mod.json`.

## Lo que NO está implementado, y por qué

### Culling lógico de objetos fuera de pantalla  (punto 1)
GD ya hace culling por secciones: `PlayLayer::updateVisibility` solo activa
los objetos cercanos a la cámara. Lo que queda ejecutándose fuera de pantalla
(triggers, movimientos, colisiones, pulsos de color) es **lógica de juego**: si
se salta, el estado del nivel diverge de lo que el creador diseñó (un trigger
que no se ejecutó afecta a lo que ves cuando la cámara llega). Eso rompe el
requisito de no alterar el nivel, y puede invalidar completados.

### Reescribir la iteración de la lista de objetos  (punto 2)
Ese bucle está dentro de funciones como `GJBaseGameLayer::update`. Sustituirlo
con un hook obliga a reimplementar esa función entera, que cambia entre
versiones del juego y se rompe con cada actualización. El riesgo de desincronizar
físicas y triggers es alto y el beneficio, incierto sin perfilar antes.

### "GC" y evitar asignaciones  (punto 3)
GD es C++ con conteo de referencias (cocos2d-x): no hay recolector de basura
que pause el juego. Los stutters vienen de otras causas (carga de texturas,
guardado a disco, audio, picos de lógica en niveles pesados). Por eso el mod
incluye el registro de picos: primero medir, luego arreglar la causa concreta.

### Interpolación de frames con físicas desacopladas  (punto 4, parte avanzada)
Interpolar el render sin tocar la simulación exige separar el paso de física
del render (timestep fijo). Eso cambia cómo se calcula cada frame de juego y
afecta a la jugabilidad. No es compatible con "no alterar físicas", así que no
lo incluyo.

## Siguiente paso recomendado
1. Activa "Registrar picos de frame" y juega los niveles que te dan stutters.
2. Pasa las líneas del log (`geode/logs`) y se ve en qué momentos ocurren.
3. Con eso se puede atacar la causa concreta con un hook pequeño y seguro.
