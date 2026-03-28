# Diario de Desarrollo — Neural Snake

Registro completo del proceso iterativo: cada problema, análisis y solución aplicada. Cada entrada sigue el formato: **problema → análisis → fix → resultado**.

---

## v1 — La IA da vueltas en bucle

### Problema
La primera versión del sistema NEAT: 500 serpientes evolucionando, 14 inputs, y fitness que recompensaba supervivencia:

```
fitness = score × 5000 + totalSteps + score² × 500
```

Las serpientes descubrieron que sobrevivir daba más fitness que arriesgarse a buscar comida. Aprendieron a dar vueltas en círculos maximizando `totalSteps`.

> **Video del problema:**

<video src="videos/1 iteracion.mp4" width="100%" controls muted></video>

### Fix
Eliminar `totalSteps` de la fitness. Añadir bonus por acercarse a la comida (distancia Manhattan).

**Lección:** nunca recompensar la supervivencia directamente.

---

## v2 — Estancada en 0-2 frutas

### Problema
Tras eliminar `totalSteps`, añadí approach bonus:

```
fitness = score × 5000 + score² × 500 + max(0, approachBonus)
```

Tras **2000 generaciones** apenas conseguían 0-2 frutas. La evolución estaba estancada.

> **Video del problema:**

<video src="videos/2iter.mp4" width="100%" controls muted></video>

### Fix
Requirió diagnóstico profundo → ver v3.

---

## v3 — Diagnóstico profundo: tres bugs críticos

### Problema
2000 generaciones y la IA no mejoraba. Tres bugs simultáneos:

1. **Fitness sin gradiente** — `std::max(0, bonus)` eliminaba señal negativa. NEAT no distinguía "casi llega" de "ni se acerca"
2. **Visión de 1 casilla** — Solo miraba adyacente (binario). Sin noción de distancia
3. **Topological sort roto** — Kahn's algorithm no contaba edges desde inputs en `inDeg`. Los nodos de salida se evaluaban antes de recibir señal

### Fix
1. Fitness con gradiente real — permitir negativo (suelo 0.001)
2. Raycast 8 direcciones × 3 canales = **28 inputs**
3. Fix topological sort — contar todas las edges en inDeg
4. Visualización de raycasts en tiempo real

> **Video del resultado:** la IA empieza a buscar comida

<video src="videos/3.mp4" width="100%" controls muted></video>

---

## v4 — Bucles de nuevo a 6 puntos

### Problema
Con raycast y fitness con gradiente, la serpiente llegó a 6 puntos pero se estancó en bucles. 200 pasos de margen (`maxStepsPerFood`) eran demasiado generosos — el riesgo de buscar la 7ª fruta no compensaba.

> **Video del problema:**

<video src="videos/4.mp4" width="100%" controls muted></video>

### Fix
1. `maxStepsPerFood` reducido a 100
2. Bonus de eficiencia: `(1 - pasos/maxSteps) × 1000` por fruta

```
fitness = score × 5000 + score² × 500 + efficiencyBonus + stepsToward × 1.5 - stepsAway × 2.0
```

---

## v5 — UI completa: 24 partidas simultáneas + modo Play

### Cambios
Reestructuración total de la interfaz:
- TRAIN: cuadrícula 6×4 con 24 partidas a x20 velocidad
- Panel central: MAX SCORE, total games, tiempo, PAUSE/RESUME
- PLAY: partida individual para probar modelo entrenado
- Modelo solo en memoria (no persiste al cerrar)
- Ventana maximizada al inicio

> **Video del resultado:** la IA entrena visible en 24 partidas

<video src="videos/5.mp4" width="100%" controls muted></video>

### Problema detectado
Al pasar a modo Play, la IA apenas lograba 2-3 puntos.

---

## v6 — IA estancada en 2-3 puntos

### Problema
En modo Play la IA no pasaba de 2-3 frutas. Tres causas:

1. **Evaluación ruidosa** — `gamesPerGenome = 1`, una sola partida con seed aleatorio
2. **Penalización excesiva** — `stepsAway × 2.0` penalizaba rodeos necesarios alrededor del cuerpo
3. **Penalización acumulativa** — contadores se acumulaban toda la partida

> **Video del problema:**

<video src="videos/6.mp4" width="100%" controls muted></video>

### Fix
1. `gamesPerGenome = 3` para fitness más fiable
2. Reset approach/retreat por fruta + penalización simétrica (1.0/1.0)
3. Evaluación paralela multi-thread (todos los cores CPU)
4. Recuadro azul en mejor partida en vivo

---

## v7 — "Siempre va hacia arriba"

### Problema
Entrenamiento super rápido (gracias a paralelización), max scores altos en menos de un minuto. Pero al probar en Play: la serpiente siempre iba hacia arriba y moría inmediatamente.

> **Video del problema:**

<video src="videos/7.mp4" width="100%" controls muted></video>

### Diagnóstico
Al resetear `stepsToward`/`stepsAway` por fruta, **se descartaban** los datos de approach anteriores. Solo quedaba la señal de la última fruta buscada (incompleta). La fitness era casi puramente `score × 5000` — sin señal direccional.

NEAT convergía en "siempre UP" porque con 500 genomas × muchas generaciones, alguno acertaba comida por casualidad. Un óptimo local degenerado.

### Fix
**Acumular approach bonus por fruta** en vez de descartarlo:

```
// Al comer:
approachBonus += stepsToward × 1.5 - stepsAway × 0.75
stepsToward = 0; stepsAway = 0;

// Fitness final:
fitness = score × 5000 + score² × 500 + efficiencyBonus + approachBonus
```

- Cada fruta contribuye independientemente al bonus total
- Penalización asimétrica (0.75 vs 1.5) para permitir rodeos

---

## v8 — 50+ puntos en 40 segundos de entrenamiento

### Resultado
Tras el fix del approach acumulado, **todo encajó**. En menos de 40 segundos la IA alcanzó más de 50 puntos en modo Play.

> **Video del resultado:** la IA juega de verdad

<video src="videos/8.mp4" width="100%" controls muted></video>

### Conclusión: la señal lo es todo

Mirando atrás, las 8 iteraciones cuentan la misma historia: **si NEAT no tiene señal clara de qué es "mejor", no puede evolucionar**.

| Iteración | Señal rota | Resultado |
|-----------|-----------|-----------|
| v1 | Recompensaba supervivencia | Bucles infinitos |
| v2 | `max(0, bonus)` eliminaba gradiente | Estancada en 0-2 |
| v3 | Visión binaria + topo sort roto | Red no procesaba inputs |
| v4 | maxSteps demasiado generoso | Bucles a 6 puntos |
| v6 | 1 partida/genoma → fitness ruidosa | Estancada en 2-3 |
| v7 | Approach descartado por fruta | Siempre va arriba |
| **v8** | **Approach acumulado correctamente** | **50+ puntos en 40s** |

La función de fitness es el "lenguaje" con el que le dices a la evolución qué quieres. Si ese lenguaje es ambiguo, la evolución encuentra atajos que técnicamente maximizan la fitness pero no hacen lo que tú quieres.

**Lección principal:** en neuroevolución, el 90% del trabajo no es el algoritmo — es **diseñar la fitness correctamente**.

---

## v9 — Se puede ganar + win rate

### Cambios
- Nuevo estado `GameState::WIN` cuando la serpiente llena el grid completo (297 frutas en 20×15)
- Bonus de 50.000 fitness por ganar
- Panel de train: **WIN RATE %**, contador de victorias, gráfica de win rate (0-100%)
- Overlay "YOU WIN!" en modo Play

**Objetivo:** ver la gráfica de win rate subir conforme el modelo mejora, hasta llegar al 100%.

### Nota sobre overfitting
A diferencia de deep learning, NEAT **no puede hacer overfitting**. No hay dataset que memorizar — cada partida genera posiciones de comida aleatorias, así que la IA siempre se enfrenta a situaciones nuevas. Lo peor que puede pasar es que la población se estanque en un óptimo local (deja de mejorar), pero nunca va a "desaprender" lo que ya sabe. Se puede dejar entrenando indefinidamente.

---

## v10 — Entrenamiento largo: bucles a 60+ puntos

### Problema
Tras dejar entrenando, la IA llegó a 60+ de max score pero después de 342 partidas showcase todas las serpientes se quedaron dando círculos. No morían (porque con `maxStepsPerFood = 100` aún les quedaban pasos) pero tampoco comían.

> **Video del problema:**

<video src="videos/9.mp4" width="100%" controls muted></video>

### Diagnóstico
Con una serpiente de 60+ celdas de cuerpo, el tablero de 300 celdas está bastante ocupado. La comida puede estar en una posición que requiere navegar alrededor de todo el cuerpo — fácilmente más de 100 pasos. Con `maxStepsPerFood = 100`, la serpiente no tenía suficientes pasos para encontrar la comida en situaciones complejas. Pero 100 pasos tampoco la mataban rápido porque seguía haciendo circuitos cortos.

### Fix
**`maxStepsPerFood = 300`** (= número de celdas del grid, 20×15).

Lógica: si la serpiente recorre todas las celdas del tablero sin encontrar comida, algo va mal. 300 pasos da margen suficiente para navegar alrededor de un cuerpo largo, pero impide bucles infinitos. El efficiency bonus sigue funcionando: comer en 10 pasos da ~967 bonus, comer en 200 da ~333 — la señal para comer rápido se mantiene.

### Resultado
Tras 5 minutos de entrenamiento con `maxStepsPerFood = 300`, la IA llegó a **67 puntos de max score** pero se volvió a estancar. Todas las serpientes haciendo bucles de nuevo.

> **Video del resultado:** estancada en 67 tras 5 min

<video src="videos/10.mp4" width="100%" controls muted></video>

### Análisis: ¿por qué se estanca en ~67?

El patrón es claro: la IA aprende rápido a comer hasta ~60-70 frutas, pero a partir de ahí el cuerpo ocupa ~22% del tablero y la navegación se vuelve un problema completamente diferente. Las posibles causas:

1. **La red es demasiado simple para navegación compleja** — Con 60+ celdas de cuerpo, la serpiente necesita planificar rutas, no solo "ir hacia la comida". Los raycasts dicen dónde hay pared/cuerpo/comida, pero no dan información sobre la *forma* del cuerpo ni si un camino tiene salida. La serpiente entra en callejones sin salida formados por su propio cuerpo.

2. **Pérdida de diversidad genética** — Tras muchas generaciones, toda la población converge en estrategias similares. Sin diversidad, no hay innovación para descubrir nuevas formas de navegar.

3. **La fitness no distingue "buen loop" de "mal loop"** — Una serpiente que hace un loop inteligente (Hamiltoniano parcial) y una que hace bucles tontos reciben penalización similar si no comen en 300 pasos.

4. **El approach bonus pierde utilidad** — Con cuerpo largo, la distancia Manhattan a la comida no refleja la distancia real (que requiere rodear el cuerpo). Acercarse en Manhattan puede significar meterse en un callejón sin salida.

### Nota sobre overfitting
NEAT **no puede hacer overfitting**. No hay dataset — cada partida genera posiciones de comida aleatorias. Lo peor es estancamiento en un óptimo local (que es exactamente lo que está pasando), pero nunca desaprende.

---

## v11 — Flood fill inputs: la serpiente sabe si un camino tiene salida

### Cambios
4 inputs nuevos (total: **32 inputs**). Para cada dirección cardinal (UP, RIGHT, DOWN, LEFT), se hace un **flood fill** (BFS) desde la celda adyacente a la cabeza, contando cuántas celdas vacías son accesibles. Normalizado por el total de celdas vacías.

```
inputs[28] = floodFill(head.x, head.y - 1) / totalEmpty  // UP
inputs[29] = floodFill(head.x + 1, head.y) / totalEmpty  // RIGHT
inputs[30] = floodFill(head.x, head.y + 1) / totalEmpty  // DOWN
inputs[31] = floodFill(head.x - 1, head.y) / totalEmpty  // LEFT
```

**Por qué funciona:** la causa raíz del estancamiento en v10 era que la serpiente no tenía información sobre si un camino tenía salida. Los raycasts dicen "hay cuerpo a 3 celdas" pero no "si vas ahí te quedas atrapada en un callejón sin salida formado por tu propio cuerpo". Con flood fill, la serpiente puede comparar cuánto espacio tiene en cada dirección y evitar entrar en zonas cerradas.

**Coste computacional:** O(grid) = O(300) por dirección × 4 = O(1200) por movimiento. Aceptable con multi-thread.

### Resultado
Seguía atascándose en bucles. El flood fill daba información espacial pero la serpiente no tenía noción de urgencia ni incentivo para explorar celdas nuevas.

> **Video del problema:**

<video src="videos/11.mp4" width="100%" controls muted></video>

---

## v12 — Anti-bucle: hambre + exploración + dirección cola

### Problema
Con flood fill la IA seguía haciendo bucles. Tres carencias:
1. **No sabe cuánto le queda para morir** — sin noción de urgencia, no arriesga
2. **Repetir celdas no penaliza** — dar vueltas en círculo no tiene coste en fitness
3. **No sabe dónde está su cola** — en snake avanzado, perseguir la cola es estrategia clave

### Cambios

#### 3 inputs nuevos (total: **35 inputs**)
```
inputs[32] = stepsSinceLastFood / maxStepsPerFood  // Hambre (0→1, urgencia)
inputs[33] = (tail.x - head.x) / gridW             // Dirección a cola X
inputs[34] = (tail.y - head.y) / gridH             // Dirección a cola Y
```

#### Exploration bonus en fitness
Entre fruta y fruta, se cuentan las celdas **únicas** visitadas vs pasos totales. Ratio alto = explorando, ratio bajo = loopeando.
```
explorationBonus += uniqueCells / stepsThisFruit × 500  (por fruta)
```

#### Fitness actualizada
```
fitness = score × 5000 + score² × 500 + efficiencyBonus + approachBonus + explorationBonus
```

### Lógica de cada cambio
- **Hambre**: la red sabe que cuando el input se acerca a 1, debe arriesgar más. Permite "cambio de estrategia" dinámico
- **Exploración**: NEAT aprende que visitar celdas nuevas = más fitness. Penaliza implícitamente los bucles
- **Cola**: saber dónde está la cola permite la estrategia de "perseguir tu cola" que es fundamental en snake avanzado para no encerrarte

### Resultado
Mejora significativa: de 67 → **103 puntos**. Los cambios funcionaron — la serpiente navega mejor su propio cuerpo y explora más. Pero tras varios minutos se estancó de nuevo en 103. Con el cuerpo ocupando ~34% del tablero, la navegación sigue siendo el cuello de botella.

> **Video del resultado:**

<video src="videos/12.mp4" width="100%" controls muted></video>

### Análisis: ¿por qué 103 y no más?
El salto 67→103 confirma que los inputs extra y el exploration bonus ayudan. Pero a 100+ celdas de cuerpo, aparecen nuevos problemas:

1. **La serpiente no sabe si la comida es alcanzable** — el flood fill dice cuánto espacio hay en cada dirección, pero no si la comida está en ese espacio. Puede elegir la dirección con más espacio pero que no lleva a la comida
2. **Pérdida de diversidad** — con `stagnationLimit=15`, las especies se eliminan rápido. Estrategias complejas (que necesitan más generaciones para madurar) mueren antes de florecer
3. **500 genomas puede no ser suficiente** — a estas alturas de complejidad, más genomas = más "intentos" por generación para descubrir innovaciones

---

## v13 — Food reachability + población 1000 + stagnation 25

### Problema
v12 llegó a 103, pero seguía estancándose. Tres carencias:
1. La serpiente no sabe si la comida es alcanzable (el flood fill dice espacio, no si la comida está ahí)
2. 500 genomas insuficientes para la complejidad de navegación avanzada
3. `stagnationLimit=15` mata especies con estrategias complejas antes de que maduren

### Cambios

#### Input nuevo: food reachable (total: **36 inputs**)
```
inputs[35] = 1.0 si la comida está en un espacio conectado accesible desde la cabeza, 0.0 si no
```
Reutiliza los 4 flood fills que ya hacemos — si cualquiera de las 4 BFS alcanza la celda de la comida, es alcanzable. Coste extra: 0 (ya se calcula).

#### Refactor flood fill
Ocupancy grid se construye **una sola vez** y se pasa a las 4 BFS. Antes se reconstruía 4 veces por movimiento.

#### Parámetros NEAT
- `populationSize`: 500 → **1000** — el doble de diversidad genética
- `stagnationLimit`: 15 → **25** — más tiempo para que estrategias complejas maduren

### Lógica
- **Food reachable**: la serpiente sabe si vale la pena ir hacia la comida o si debe reposicionarse primero. Antes iba directo a comida inalcanzable y moría en callejones sin salida
- **1000 genomas**: más probabilidad de que alguno descubra innovaciones de navegación. Con multi-thread el coste extra es asumible
- **Stagnation 25**: especies con potencial tienen 10 generaciones más para evolucionar antes de ser eliminadas

### Resultado
**Peor que v12**: solo 78 puntos en 7 minutos (vs 103 en v12). La población de 1000 hizo que cada generación tardase el doble, resultando en **menos generaciones** en el mismo tiempo. Menos evolución = peor resultado.

> **Video del resultado:**

<video src="videos/13.mp4" width="100%" controls muted></video>

### Análisis: ¿por qué empeoró?
1. **1000 genomas = generaciones más lentas** — con el doble de genomas + flood fill (4 BFS por movimiento), cada generación tarda mucho más. En 7 minutos hay muchas menos generaciones que con 500
2. **stagnationLimit=25 mantiene especies mediocres** — especies que no mejoran sobreviven 10 generaciones más, consumiendo slots de la población sin aportar
3. **El cuello de botella es velocidad de evolución, no diversidad** — v12 demostró que 500 genomas bastan para llegar a 103. El problema no era falta de diversidad sino falta de información (que ya se añadió con flood fill, hambre, cola)

**Lección:** más genomas no siempre es mejor. El tiempo de entrenamiento importa tanto como la diversidad — generaciones más rápidas = más evolución.

---

## v14 — Optimización: velocidad de evolución > cantidad de genomas

### Problema
v13 empeoró (103→78) porque duplicar la población hizo las generaciones demasiado lentas. **La velocidad de evolución importa más que la diversidad bruta.**

### Cambios

#### Revertir parámetros a los que funcionaban
- `populationSize`: 1000 → **500** (v12 llegó a 103 con 500)
- `stagnationLimit`: 25 → **15**
- `gamesPerGenome`: 3 → **2** (33% más rápido por generación)

#### Optimizar flood fill (el cuello de botella de rendimiento)
1. **Solo para serpientes largas** — flood fill solo cuando `body.size() > 30`. Para serpientes cortas, los raycasts bastan y se usan valores neutros (0.5)
2. **Caché cada 3 pasos** — en vez de 4 BFS por movimiento, se recalcula cada 3 pasos. La topología del cuerpo no cambia drásticamente en 3 pasos
3. **Invalidar caché al comer** — cuando come, la comida cambia de posición → forzar recálculo

#### Se mantiene todo lo útil
- 36 inputs (food reachable incluido)
- Exploration bonus en fitness
- Hambre + dirección cola

### Lógica
Cada generación ahora es **~3-4x más rápida** que v13:
- 500 vs 1000 genomas = 2x
- 2 vs 3 partidas = 1.5x
- Flood fill cacheado = ahorro variable (enorme para serpientes cortas)

Más generaciones por minuto = más evolución = mejor resultado esperado.

### Resultado
**Peor todavía: 49 puntos en 7 minutos** (vs 103 en v12). El cacheado del flood fill cada 3 pasos daba información espacial obsoleta, y desactivarlo para serpientes cortas confundía a NEAT (señal que cambia de significado a mitad de partida).

> **Video del resultado:**

<video src="videos/14.mp4" width="100%" controls muted></video>

**Lección:** no cachear información espacial crítica. El flood fill debe ser fresh cada movimiento — la serpiente necesita saber AHORA si un camino tiene salida, no hace 3 pasos.

---

## v15 — Aprovechar los 24 threads del i7-14650HX

### Problema
El threading anterior usaba chunks fijos: dividía 500 genomas en 24 bloques iguales (~21 cada uno). Pero los genomas que puntúan alto juegan más pasos → sus threads tardan mucho más mientras los demás esperan. **Load imbalance.**

Además, con 24 threads y solo 2 partidas/genoma (1000 evaluaciones totales), los threads estaban infrautilizados.

### Cambios

#### Work-stealing con atomic counter
En vez de chunks fijos, cada thread coge el siguiente genoma libre con `atomic<int>::fetch_add(1)`. Cuando un thread termina un genoma rápido (puntuación baja, pocos pasos), inmediatamente coge otro. **Todos los threads trabajan hasta que no quedan genomas.**

```cpp
std::atomic<int> nextJob{0};
auto worker = [&]() {
    while (true) {
        int i = nextJob.fetch_add(1);
        if (i >= n) break;
        // evaluar genoma i
    }
};
```

#### 4 partidas por genoma
Con 24 threads bien balanceados, 500 × 4 = 2000 evaluaciones se distribuyen eficientemente. Fitness más fiable sin coste real de tiempo.

### Impacto en i7-14650HX (16 cores / 24 threads)
- **Antes:** chunks de 21 genomas × 2 partidas = thread más lento bloquea. ~50% uso CPU efectivo
- **Ahora:** work-stealing × 4 partidas = todos los threads al 100% hasta que se acaban los genomas. ~95% uso CPU

### Resultado
Subió rápido hasta **80 puntos** gracias a la mayor velocidad de generaciones, pero después se estancó. El work-stealing cumplió su objetivo (generaciones más rápidas), pero no cambia los inputs ni la fitness — la serpiente sigue sin poder navegar cuerpos largos.

> **Video del resultado:** sube rápido a 80, luego se estanca

<video src="videos/15.mp4" width="100%" controls muted></video>

---

## v16 — Revertir a base v12 + BFS distance + death penalty + logs

### Problema
v14 demostró que cachear el flood fill destruye la señal espacial (103→49). v13 demostró que más genomas no siempre es mejor (103→78). Necesitamos volver a la base que funcionaba (v12) y añadir mejoras que no rompan lo que ya funciona.

### Cambios

#### Revertir a base v12
- Flood fill **fresco cada paso** (sin caching, sin threshold de body>30)
- `populationSize = 500`, `stagnationLimit = 15`
- `gamesPerGenome = 3` (balance entre fiabilidad y velocidad)

#### BFS pathfinding distance (input nuevo, total: **37 inputs**)
```
inputs[36] = 1.0 - BFS_distance_to_food / (gridW × gridH)
```
La distancia Manhattan no refleja la distancia real cuando el cuerpo bloquea caminos. Con BFS, la serpiente sabe la distancia **real** considerando obstáculos. Se obtiene gratis del flood fill que ya hacemos (la BFS ya encuentra la comida, solo hay que guardar la distancia).

#### Penalización por muerte por colisión
```
if (diedToBody && score > 10) fitness *= 0.9  // -10% penalty
```
Diferenciar timeout (la serpiente intentó pero no encontró) de colisión (navegó mal). Solo aplica a score > 10 para no penalizar serpientes novatas que aún están aprendiendo.

#### Work-stealing threading (mantenido de v15)
`atomic<int>::fetch_add(1)` para distribución dinámica de trabajo entre 24 threads.

#### Sistema de logs de entrenamiento
- Log automático cada vez que el **max score** sube
- Cada entrada registra: tiempo, max score, generación, partidas totales, win rate, especies, fitness
- Panel de log visible **solo al pausar** el entrenamiento (no molesta durante el training)
- Al detener/cerrar el programa, se guarda automáticamente en `logs/train_YYYYMMDD_HHMMSS.log`
- Formato tabular legible para análisis posterior

### Resultado
Pendiente de test.

> **Video del resultado:**

<video src="videos/16.mp4" width="100%" controls muted></video>

---

## v17 — La serpiente ve su cuerpo: densidad + visión local + escape a cola

### Problema
Análisis de los 37 inputs anteriores reveló 3 carencias críticas:

1. **No sabe la forma de su cuerpo** — los raycasts detectan el primer segmento en 8 líneas, pero no la distribución general. No distingue "cuerpo concentrado arriba-izquierda" de "cuerpo repartido por todo el tablero"
2. **No sabe si puede llegar a su cola** — la dirección a la cola existía (inputs 33-34), pero no si hay camino libre hasta ella. En snake avanzado, poder llegar a tu cola = tener escape garantizado (la cola se mueve)
3. **No ve los obstáculos inmediatos con detalle** — los raycasts solo ven en 8 líneas rectas con ángulos muertos de 45°. Un segmento de cuerpo a 1 celda en diagonal no detectada por ningún raycast puede matar a la serpiente

### Cambios

#### Mapa de densidad corporal: 12 inputs (total: **49 inputs**)
El tablero 20×15 se divide en una cuadrícula de 4×3 = **12 zonas** de 5×5 celdas. Cada input indica qué fracción de esa zona está ocupada por el cuerpo (0.0 = vacía, 1.0 = llena).

```
inputs[37..48] = bodyCountInZone / 25  (para cada zona 5×5)
```

**Disposición de zonas:**
```
┌──────┬──────┬──────┬──────┐
│ [37] │ [38] │ [39] │ [40] │  y=0..4
├──────┼──────┼──────┼──────┤
│ [41] │ [42] │ [43] │ [44] │  y=5..9
├──────┼──────┼──────┼──────┤
│ [45] │ [46] │ [47] │ [48] │  y=10..14
└──────┴──────┴──────┴──────┘
 x=0..4  x=5..9 x=10..14 x=15..19
```

La serpiente ahora "ve" la forma general de su cuerpo: si está enrollada en una esquina, si ocupa el centro, si hay zonas libres por las que puede navegar.

#### BFS distance a la cola: 1 input (total: **50 inputs**)
```
inputs[49] = 1.0 - BFS_distance_to_tail / (gridW × gridH)
```

BFS desde la cabeza hasta la cola, tratando la celda de la cola como **transitable** (porque cuando la serpiente llegue ahí, la cola ya se habrá movido). Si no hay camino, el input es 0.0.

- **1.0** = la cola está al lado, escape inmediato
- **0.5** = la cola está a ~150 pasos, camino largo pero existe
- **0.0** = no hay camino a la cola, estás potencialmente atrapada

#### Visión local 5×5 alrededor de la cabeza: 25 inputs (total: **75 inputs**)
Una cuadrícula de 5×5 centrada en la cabeza. Cada celda indica si hay obstáculo:
- **1.0** = cuerpo o pared (peligro)
- **0.0** = vacío (seguro)

```
inputs[50..74] = obstacle map, head at center [52+2*5+2 = input 62]

Ejemplo con cuerpo al sur y pared al norte:
  1.0  1.0  1.0  1.0  1.0   ← pared (head.y=1, row -2 fuera del grid)
  0.0  0.0  0.0  0.0  0.0
  0.0  0.0 [HEAD] 0.0  0.0
  0.0  0.0  1.0  0.0  0.0   ← segmento de cuerpo
  0.0  0.0  1.0  1.0  0.0   ← más cuerpo
```

Esto le da visión **completa** de los 5×5 = 25 celdas inmediatas, sin ángulos muertos. Puede ver callejones sin salida, esquinas del cuerpo, y obstáculos en cualquier dirección.

#### Botón COPY en panel de logs
Se añade un botón "COPY" en el panel de training log (visible al pausar) que copia todo el log al portapapeles del sistema.

#### v17b: "Super tocho" — 14 inputs extra (total: **89 inputs**)
Tras el análisis de carencias, se añaden los 4 inputs que un jugador humano usaría instintivamente:

**Dirección óptima hacia comida — one-hot (4 inputs, índices 75-78)**
```
inputs[75..78] = one-hot de la dirección del primer paso BFS hacia la comida
```
El jugador humano sabe "tengo que ir a la derecha para llegar a la manzana". Antes la serpiente solo sabía la distancia, no la dirección. Se calcula gratis de las 4 BFS que ya hacemos — la dirección con menor `foodDist` gana.

**Dirección óptima hacia cola — one-hot (4 inputs, índices 79-82)**
```
inputs[79..82] = one-hot de la dirección del primer paso BFS hacia la cola
```
Saber cómo escapar. "Para llegar a tu cola (tu ruta de escape), ve hacia arriba." Se calcula de las 4 BFS nuevas con cola pasable.

**Dirección relativa a comida — vector normalizado (2 inputs, índices 83-84)**
```
inputs[83] = (food.x - head.x) / gridW   // -1 a 1
inputs[84] = (food.y - head.y) / gridH   // -1 a 1
```
Los raycasts solo detectan comida si cae en una de las 8 líneas. Este vector le dice **siempre** dónde está la comida. Es el input más básico que faltaba — un jugador humano siempre sabe dónde está la manzana.

**Seguridad por dirección — binario (4 inputs, índices 85-88)**
```
inputs[85] = ¿si voy UP, puedo llegar a mi cola? (1 = sí, 0 = no)
inputs[86] = ¿si voy RIGHT, puedo llegar a mi cola?
inputs[87] = ¿si voy DOWN, puedo llegar a mi cola?
inputs[88] = ¿si voy LEFT, puedo llegar a mi cola?
```
**El input más importante de todos.** Un jugador humano nunca entra en un callejón sin salida a propósito. Con este input, la serpiente sabe: "si voy a la izquierda me quedo atrapada, pero a la derecha puedo escapar". Se calcula de las mismas 4 BFS hacia la cola.

### Coste computacional
- Mapa de densidad: O(body.size) — recorre el array de ocupación existente
- BFS a cola (4 direcciones): O(300) × 4 = O(1200) — reutilizado para inputs 49, 79-82, 85-88
- BFS a comida (4 direcciones): O(300) × 4 = O(1200) — ya existían, ahora también dan inputs 75-78
- Visión local 5×5: O(25)
- Dirección comida: O(1)
- **Total BFS por movimiento:** 8 (4 comida + 4 cola). Antes eran 5. +3 BFS extra. Con 24 threads, aceptable.

### Resultado
**74 puntos en 6:27, luego estancamiento total** (736k+ partidas sin mejorar).

```
=== Neural Snake Training Log ===
Duration: 06:27  |  Max Score: 74

[00:00] MAX 4   |  Gen 1   |  Games 1500    |  Species 1  |  Fit 11317
[00:02] MAX 15  |  Gen 18  |  Games 27253   |  Species 1  |  Fit 88461
[00:03] MAX 38  |  Gen 23  |  Games 34801   |  Species 1  |  Fit 373488
[00:04] MAX 46  |  Gen 26  |  Games 39318   |  Species 1  |  Fit 835675
[00:16] MAX 56  |  Gen 56  |  Games 84565   |  Species 1  |  Fit 1274467
[00:19] MAX 74  |  Gen 62  |  Games 93593   |  Species 1  |  Fit 2082633
(736k+ partidas después, sigue en 74)
```

### Análisis: ¿por qué empeoró respecto a v12 (103)?

**El problema es 1 sola especie durante todo el entrenamiento.** Con 89 inputs, la red inicial tiene 89×4 = 356 conexiones. Todos los genomas parten de la misma topología masiva → la distancia de compatibilidad entre ellos es pequeña → `compatThreshold = 3.0` los mete a todos en 1 especie → sin especiación no hay protección de innovaciones → convergencia prematura.

**Lección:** más inputs requiere ajustar la especiación. El threshold que funcionaba con 37 inputs (148 conexiones) no funciona con 89 inputs (356 conexiones). El espacio de búsqueda se multiplicó por 2.4x pero la diversidad evolutiva se mantuvo igual.

---

## v18 — Parámetros agresivos para 89 inputs

### Problema
v17 demostró que 89 inputs con los parámetros anteriores (500 genomas, compatThreshold 3.0) colapsa en 1 especie y se estanca en 74. Los inputs están bien — NEAT no tiene las herramientas para explotarlos.

### Cambios

#### Especiación ajustada
- `compatThreshold`: 3.0 → **6.0** — con 356 conexiones base, necesita un threshold mucho más alto para crear varias especies

#### Población y evaluación escaladas
- `populationSize`: 500 → **1000** — el doble de diversidad para un espacio de búsqueda 2.4x mayor
- `gamesPerGenome`: 3 → **4** — fitness más fiable con tantos inputs

#### Mutación más agresiva
- `addConnectionRate`: 0.05 → **0.08** — explora más conexiones nuevas entre los 89 inputs
- `addNodeRate`: 0.03 → **0.05** — crea más nodos ocultos para procesar la información extra
- `toggleRate`: 0.01 → **0.02** — más variación de topología

#### Especies más protegidas
- `stagnationLimit`: 15 → **20** — más tiempo para que estrategias complejas maduren
- `survivalFraction`: 0.20 → **0.25** — más padres sobreviven a cada generación
- `elitesPerSpecies`: 1 → **2** — protege los 2 mejores de cada especie

### Lógica
1000 genomas × 4 partidas = **4000 evaluaciones/generación** distribuidas en 24 threads con work-stealing. Generaciones más lentas, pero con más diversidad y especiación funcional, cada generación debería producir innovaciones reales.

### Resultado
**74 puntos en 10:37, luego estancamiento total.** Mismo resultado que v17, pero más lento.

```
=== Neural Snake Training Log ===
Duration: 10:37  |  Max Score: 74

[00:00] MAX 5   |  Gen 2   |  Games 8068     |  Species 1  |  Fit 18639
[00:09] MAX 11  |  Gen 18  |  Games 72555    |  Species 1  |  Fit 46564
[00:19] MAX 37  |  Gen 30  |  Games 121042   |  Species 1  |  Fit 383645
[00:32] MAX 54  |  Gen 43  |  Games 173090   |  Species 1  |  Fit 928136
[01:08] MAX 70  |  Gen 67  |  Games 269592   |  Species 1  |  Fit 1165865
[04:16] MAX 74  |  Gen 151 |  Games 609534   |  Species 1  |  Fit 1108500
(1.1M+ partidas después, sigue en 74)
```

### Análisis post-mortem: el colapso de especiación

**Sigue 1 sola especie** a pesar de `compatThreshold = 6.0`. El problema es más profundo que el threshold:

La función de compatibilidad normaliza por `N` (número de conexiones del genoma más grande):
```
distancia = C1 × excess/N + C2 × disjoint/N + C3 × avgWeightDiff
```

Con 89 inputs × 4 outputs = **356 conexiones iniciales**, `N = 356`. Los términos excess/disjoint se dividen por 356, haciéndolos insignificantes (~0.01). Solo queda `C3 × avgWeightDiff = 0.4 × ~0.4 ≈ 0.16`. **La distancia máxima entre genomas nunca supera ~0.5**, así que da igual que el threshold sea 6.0 o 600.0 — nadie lo supera.

**Causa raíz:** demasiados inputs → N enorme → normalización aplasta las diferencias → todos en 1 especie → sin diversidad → convergencia prematura.

**Lecciones:**
1. **Con NEAT, más inputs no es más información — es más ruido.** Cada input extra multiplica el espacio de búsqueda
2. **La normalización por N en la compatibilidad rompe la especiación con muchos inputs.** Hay que capear N
3. **Los parámetros agresivos (1000 genomas, mutación alta) no compensan una especiación rota** — solo hacen generaciones más lentas

---

## v19 — Fix especiación + inputs podados a lo esencial

### Problema
v17 y v18 demostraron que 89 inputs colapsa la especiación (siempre 1 especie). Dos cambios necesarios:
1. **Arreglar la normalización** en la función de compatibilidad
2. **Podar inputs** a los que realmente aportan valor sin redundancia

### Cambios

#### Fix compatibilidad: cap de N en 100
```cpp
// genome.cpp — Genome::compatibility()
float N = std::max(a.connections.size(), b.connections.size());
if (N < 20) N = 1;
else if (N > 100) N = 100;  // NEW: evita que N enorme aplaste las diferencias
```
Con N capado a 100, los términos excess/disjoint vuelven a tener peso incluso con muchas conexiones iniciales.

#### Inputs podados: 89 → 64

**Eliminados (25 inputs redundantes):**

| Input eliminado | Por qué sobra |
|---|---|
| Raycast body distance ×8 | La visión 5×5 cubre el cuerpo cercano mejor y sin ángulos muertos |
| Dirección a cola vector ×2 | BFS dirección a cola (one-hot) es estrictamente superior |
| Food reachable ×1 | Redundante: si BFS dir comida tiene dirección, es reachable |
| BFS distancia comida ×1 | BFS dirección comida ya implica la distancia óptima |
| BFS distancia cola ×1 | BFS dirección cola ya implica la distancia óptima |
| Densidad corporal 12 zonas ×12 | Caro (12 inputs) para poca señal; la visión 5×5 + seguridad cubren la navegación |

**Mantenidos (64 inputs de alto valor):**

| Índices | Inputs | Descripción |
|---------|--------|-------------|
| 0-15 | 16 | Raycast 8 dir × 2 canales (pared + comida) |
| 16-19 | 4 | Dirección actual (one-hot) |
| 20-23 | 4 | Flood fill espacio por dirección |
| 24 | 1 | Hambre (urgencia) |
| 25-28 | 4 | BFS dirección óptima a comida (one-hot) |
| 29-30 | 2 | Dirección relativa a comida (vector) |
| 31-34 | 4 | BFS dirección óptima a cola (one-hot) |
| 35-38 | 4 | Seguridad: ¿puedo llegar a mi cola si voy por ahí? |
| 39-63 | 25 | Visión local 5×5 (obstáculos) |

**64 inputs × 4 outputs = 256 conexiones iniciales** (vs 356 antes). Con N capado a 100, la compatibilidad funciona correctamente.

#### Parámetros mantenidos de v18
- `populationSize = 1000`, `gamesPerGenome = 4`
- Mutación agresiva: `addConnection 0.08`, `addNode 0.05`
- `stagnationLimit = 20`, `elitesPerSpecies = 2`
- `compatThreshold = 4.0` (ajustado para N capado)

### Resultado

**Max score: 82** en ~9:25 de entrenamiento. Todavía **1 sola especie** todo el rato.

```
[00:00] MAX 3   |  Gen 1   |  Species 1  |  Fit 14106
[00:04] MAX 13  |  Gen 16  |  Species 1  |  Fit 46764
[00:22] MAX 42  |  Gen 36  |  Species 1  |  Fit 302052
[01:03] MAX 60  |  Gen 51  |  Species 1  |  Fit 556610
[01:44] MAX 69  |  Gen 61  |  Species 1  |  Fit 870750
[03:54] MAX 78  |  Gen 95  |  Species 1  |  Fit 1469310
[09:25] MAX 82  |  Gen 161 |  Species 1  |  Fit 1648498
```

### Post-mortem

La poda de 89→64 inputs mejoró ligeramente (82 vs 74), pero el problema de fondo persiste: **siempre 1 especie**. Con C3=0.4 y N capado a 100, las distancias de compatibilidad siguen siendo ~0.15-0.20, muy por debajo de cualquier threshold razonable. Los genomas solo difieren en pesos (topología idéntica al inicio), y C3=0.4 no amplifica suficiente esas diferencias.

Se necesita subir C3 significativamente para que las diferencias de pesos generen distancias reales.

---

## v20 — Fix especiación: C3=1.5, compatThreshold=2.0

### Problema

Al subir C3 de 0.4 a 3.0 (v19 intento fallido), el entrenamiento se **congeló completamente**. Al darle a AI TRAIN nada se movía.

### Análisis

Con C3=3.0 y compatThreshold=1.5, incluso una diferencia media de pesos de 0.5 daba distancia = 3.0 × 0.5 = 1.5 = threshold. **Cada genoma se convertía en su propia especie** → 1000 especies de 1 miembro. En reproduce(), cada especie tenía `offspring=1`, que se llenaba con la copia élite. **Cero mutación, cero crossover, población congelada.**

### Fix

Punto medio entre los extremos:
- `C3 = 1.5` (antes 0.4 → 1 especie, 3.0 → 1000 especies)
- `compatThreshold = 2.0` (permite agrupación natural sin fragmentar)

Con C3=1.5: avg weight diff 0.5 → contribución 0.75. Diff 1.0 → contribución 1.5. Sumado a excess/disjoint, permite 3-50 especies naturales.

### Resultado

**Max score: 93** en 13:56. **Especiación funcional por primera vez con 64 inputs** — entre 25-57 especies dinámicas.

```
[00:00] MAX 3   |  Gen 1   |  Species 1   |  Fit 13148
[00:01] MAX 10  |  Gen 7   |  Species 5   |  Fit 35047
[00:07] MAX 14  |  Gen 17  |  Species 40  |  Fit 51750
[00:18] MAX 33  |  Gen 27  |  Species 50  |  Fit 284914
[00:51] MAX 65  |  Gen 48  |  Species 55  |  Fit 1007204
[01:19] MAX 73  |  Gen 62  |  Species 41  |  Fit 1293878
[05:55] MAX 88  |  Gen 150 |  Species 33  |  Fit 1941322
[09:06] MAX 93  |  Gen 204 |  Species 25  |  Fit 3499047
```

### Post-mortem (log extendido a 37:31)

**Max score final: 94** tras 37 minutos. Solo +1 punto en los últimos 28 minutos (93→94).

```
[09:06] MAX 93  |  Gen 204 |  Species 25  |  Fit 3499047
[19:22] MAX 94  |  Gen 334 |  Species 24  |  Fit 2587711
```

Resultados positivos:
- **Especiación funcional**: de 1 especie constante a 25-57 dinámicas.
- **Mejor score con 64 inputs**: 94 vs 82 (v19, 1 especie).
- **Progresión saludable inicial**: saltos grandes (33→65→88 en 6 min).

Problemas detectados:
- **Fitness BAJÓ** de 3.5M (gen 204) a 2.6M (gen 334) — el mejor genoma fue **perdido**. La especie que lo contenía probablemente fue eliminada por estagnación.
- **Sin élite global**: los élites son per-species. Si una especie estagna y se borra, su mejor genoma desaparece para siempre.
- **Aún por debajo de v12** (103 con 35 inputs).

> **Video del resultado:**

<video src="videos/20.mp4" width="100%" controls muted></video>

---

## v21 — Élite global

### Problema

En v20, la fitness bajó de 3.5M a 2.6M entre gen 204 y 334. El mejor genoma fue perdido cuando su especie fue eliminada por estagnación. El sistema de élites per-species no protege los mejores genomas a nivel global.

### Fix

Añadir **5 élites globales** en `reproduce()`: los 5 genomas con mayor fitness sobreviven siempre, independientemente de su especie. Se insertan primero en la nueva población y el presupuesto de offspring se reduce en 5.

```cpp
// Global elites: preserve top 5 genomes regardless of species
std::vector<int> globalOrder(genomes_.size());
std::iota(globalOrder.begin(), globalOrder.end(), 0);
std::partial_sort(globalOrder.begin(), globalOrder.begin() + 5, globalOrder.end(),
                  [&](int a, int b) { return genomes_[a].fitness > genomes_[b].fitness; });
for (int i = 0; i < 5; i++)
    newPop.push_back(genomes_[globalOrder[i]]);
```

### Resultado

**Max score: 156** en 68:42. **Récord absoluto** — destroza el anterior récord de 103 (v12) y el 94 (v20). Fitness estable y creciente durante todo el entrenamiento.

```
[00:00] MAX 2   |  Gen 1   |  Species 1   |  Fit 5491
[00:02] MAX 22  |  Gen 8   |  Species 7   |  Fit 107139
[00:10] MAX 38  |  Gen 19  |  Species 27  |  Fit 394071
[00:15] MAX 49  |  Gen 23  |  Species 35  |  Fit 924458
[01:53] MAX 62  |  Gen 58  |  Species 40  |  Fit 1416801
[02:18] MAX 73  |  Gen 66  |  Species 34  |  Fit 1712389
[04:36] MAX 92  |  Gen 99  |  Species 26  |  Fit 3069654
[07:56] MAX 106 |  Gen 144 |  Species 14  |  Fit 4014257
[10:15] MAX 119 |  Gen 178 |  Species 32  |  Fit 4738203
[30:08] MAX 122 |  Gen 360 |  Species 18  |  Fit 4617059
[40:20] MAX 137 |  Gen 472 |  Species 17  |  Fit 5268192
[49:01] MAX 146 |  Gen 558 |  Species 12  |  Fit 7272132
[58:07] MAX 156 |  Gen 631 |  Species 15  |  Fit 6837584
[68:42] MAX 156 |  Gen 729 |  Species 10  |  Fit 7926291  [PAUSED]
```

### Post-mortem

**Éxito rotundo.** La élite global fue el cambio más impactante de todo el proyecto:

- **156 vs 94** (v20 sin élite) y **vs 103** (v12, récord anterior). Un salto de +53 puntos sobre el mejor resultado histórico.
- **Fitness nunca cayó**: de 4.7M a 7.9M de forma sostenida. En v20 caía de 3.5M a 2.6M — el mejor genoma se perdía. Ahora se preserva siempre.
- **Progresión continua durante 68 minutos**: no hubo meseta real. Pasó de 119 (min 10) a 156 (min 58) — seguía escalando lento pero constante.
- **Especies se consolidan**: de 35-48 iniciales a 10-15 maduras. La diversidad se reduce naturalmente pero la élite global mantiene la calidad.
- **A min 68 la fitness sigue subiendo** (7.9M), sugiriendo que con más tiempo podría seguir mejorando.

**Conclusión**: preservar los mejores genomas independientemente de la especie es crítico. Sin élite global, el mejor genoma puede desaparecer cuando su especie estagna, perdiendo semanas de evolución acumulada.

> **Video del resultado:**

<video src="videos/21.mp4" width="100%" controls muted></video>

---

## v22 — Parámetros para cuerpo largo

### Problema

En v21, la serpiente llegó a 156 pero se estancó. A 150+ de cuerpo, la serpiente ocupa la mitad del tablero y necesita rodear su propio cuerpo para llegar a la comida.

### Fix (primer intento — fallido)

- `maxStepsPerFood`: 300 → 500 (timeout fijo más permisivo)
- `gamesPerGenome`: 4 → 6 (menos varianza)
- `stagnationLimit`: 20 → 30

### Resultado primer intento

**Max score: 98** en 17:26. Empeoró significativamente (156→98).

```
[00:21] MAX 38  |  Gen 23  |  Species 27  |  Fit 327254
[01:02] MAX 63  |  Gen 40  |  Species 44  |  Fit 775188
[05:02] MAX 95  |  Gen 87  |  Species 32  |  Fit 1883090
[10:32] MAX 98  |  Gen 129 |  Species 19  |  Fit 2014539
[17:26] MAX 98  |  Gen 163 |  Species 15  |  Fit 2193128  [PAUSED]
```

### Post-mortem primer intento

**maxStepsPerFood = 500 fijo fue el error.** Demasiado permisivo: serpientes que hacen bucles sobreviven 500 pasos en vez de 300, contaminando la población. v21 llegó a 156 con 300 steps — no era el cuello de botella real. Además, 6 games/genome hacía generaciones 50% más lentas (gen 163 en 17 min vs gen 178 en 10 min en v21).

### Fix (segundo intento)

- **maxSteps dinámico**: `200 + score × 2` — presión alta al inicio (206 steps a score 3), permisivo solo cuando lo necesita (500 steps a score 150)
- `gamesPerGenome`: revertido a **4**
- `stagnationLimit`: mantenido en **30**
- `populationSize`: 1000 → **2000** (más diversidad genética, más CPU)
- **UI**: Avg Score visible en panel central y en log

### Resultado
**Max score: 284** en 211:44 (3.5h). **95.6% del tablero** — a 13 celdas de ganar. Récord absoluto.

```
[00:00] MAX 7   |  Avg 0.0   |  Gen 1    |  Species 1    |  Fit 25084
[00:32] MAX 26  |  Avg 0.6   |  Gen 19   |  Species 85   |  Fit 222433
[03:07] MAX 59  |  Avg 21.0  |  Gen 37   |  Species 113  |  Fit 680952
[10:01] MAX 80  |  Avg 0.5   |  Gen 73   |  Species 77   |  Fit 1923954
[20:41] MAX 104 |  Avg 0.8   |  Gen 124  |  Species 56   |  Fit 3382876
[26:52] MAX 171 |  Avg 60.5  |  Gen 157  |  Species 45   |  Fit 7562418
[43:53] MAX 224 |  Avg 39.1  |  Gen 261  |  Species 37   |  Fit 7296923
[44:56] MAX 280 |  Avg 10.7  |  Gen 270  |  Species 43   |  Fit 11721308
[47:14] MAX 284 |  Avg 0.0   |  Gen 290  |  Species 52   |  Fit 16855636
[211:44] MAX 284|  Avg 50.0  |  Gen 1235 |  Species 46   |  Fit 37007588  [PAUSED]
```

### Post-mortem

**Resultado histórico.** Los tres cambios combinados desbloquearon un salto masivo:

- **284/297** (95.6% del tablero). De 156 (v21) a 284 — un salto de +128 puntos. La serpiente llena casi todo el grid.
- **Explosión entre min 26-45**: 124→171→207→224→280 en solo 18 minutos. El maxSteps dinámico permitió a las serpientes largas navegar sin morir por timeout, mientras que el maxSteps bajo al inicio mantuvo la presión contra bucles.
- **2000 genomas fue clave**: más diversidad → más probabilidad de encontrar las mutaciones raras que permiten navegación compleja. Especies estables en 37-52.
- **Fitness sigue subiendo** incluso tras 3.5h: 16.8M (min 47) → 37M (min 211). La red está mejorando consistencia (avg score) aunque el max score se estancó en 284.
- **El avg score fluctúa mucho** (0.0 a 60.5), lo que indica que el modelo aún no es consistente — a veces juega perfecto (284), a veces muere pronto.
- **Estancado en 284 durante 2.7h** (min 47 → min 211). Las últimas 13 celdas son extremadamente difíciles: requieren navegación casi perfecta con el cuerpo ocupando el 95% del espacio.

**Conclusión**: el maxSteps dinámico resolvió el dilema presión-vs-permisividad. Combinado con 2000 genomas y élite global, la serpiente encontró estrategias de navegación que antes eran imposibles.

> **Video del resultado:**

<video src="videos/22.mp4" width="100%" controls muted></video>

---

## Estado técnico actual

| Parámetro | Valor |
|-----------|-------|
| Población | **2000 genomas** |
| Inputs | **64** (ver desglose en v19) |
| Outputs | 4 (UP, RIGHT, DOWN, LEFT) |
| Partidas/genoma | 4 |
| maxStepsPerFood | **dinámico: 200 + score×2** |
| stagnationLimit | 30 |
| compatC3 | 1.5 |
| compatThreshold | 2.0 |
| N cap (compatibilidad) | 100 |
| Élites globales | 5 |
| Élites por especie | 2 |
| Flood fill | 8 BFS/movimiento (4 comida + 4 cola) |
| Evaluación | Work-stealing, 24 threads |
| Logs | Automáticos, botón COPY, snapshot en PAUSE, avg score |

---

## v23 — Endgame push: bonus cúbico + más élites

### Problema

En v22, la serpiente llegó a 284/297 (95.6%) pero se estancó 2.7h sin subir. El problema: la fitness no diferencia suficiente entre 284 y 297. A score 284, `s*5000 + s²*500 ≈ 41.7M`. A score 297: `≈ 45.6M`. Solo 4M de diferencia, y el bonus de victoria era 50K — totalmente insignificante comparado con 41M.

### Fix

1. **Bonus cúbico endgame** para scores >200: `(score-200)³ × 50`
   - Score 250: +6.25M
   - Score 280: +25.6M
   - Score 297: +45.6M
   - Crea un gradiente enorme hacia las últimas celdas
2. **Win bonus**: 50K → **500K** (ahora significativo)
3. **Élites globales**: 5 → **10** (preserva más soluciones top)

### Resultado

**Max score: 286/297** en 591 minutos (~10 horas). **96.3% del tablero.** Avg score: 142.7. Win rate: 0%.

```
[00:28] MAX 40   |  Avg 0.8    |  Gen 24    |  Species 94   |  Fit 398882
[03:15] MAX 75   |  Avg 2.4    |  Gen 59    |  Species 103  |  Fit 1538486
[14:12] MAX 103  |  Avg 0.2    |  Gen 121   |  Species 55   |  Fit 2886816
[55:18] MAX 142  |  Avg 68.6   |  Gen 326   |  Species 35   |  Fit 4987469
[96:10] MAX 188  |  Avg 0.0    |  Gen 510   |  Species 36   |  Fit 12512728
[235:31] MAX 276 |  Avg 79.8   |  Gen 1114  |  Species 33   |  Fit 27551472
[282:57] MAX 286 |  Avg 44.0   |  Gen 1448  |  Species 30   |  Fit 44751608
[591:18] MAX 286 |  Avg 142.7  |  Gen 3128  |  Species 38   |  Fit 57393976  [PAUSED]
```

### Post-mortem

286 vs 284 de v22 — solo +2 puntos a pesar de 10 horas de entrenamiento y el bonus cúbico. El bonus sí funcionó para **consistencia**: el avg score subió a 142.7 (vs fluctuante en v22), y la fitness alcanzó 57M. Pero el max score se estancó en 286 durante 5+ horas (min 283→591).

**Por qué no puede ganar:**

El problema es **fundamental**, no de parámetros. A 286+ de cuerpo, la serpiente ocupa el 96% del tablero. Las últimas 11 celdas requieren navegación tipo **Hamiltonian path** — un zigzag perfecto que visita cada celda exactamente una vez. Este patrón es demasiado específico para ser descubierto por mutación aleatoria en NEAT. La red neuronal tendría que aprender una regla geométrica perfecta, algo que está fuera del alcance de la optimización estocástica.

Además, el modelo **rara vez llega a 280+** en una partida normal, así que casi nunca practica el endgame. No hay suficiente presión evolutiva sobre los últimos movimientos.

### Posibles soluciones (no implementadas)

Las siguientes soluciones **podrían** cerrar el gap, pero serían en cierta forma **hacer trampa** — inyectar conocimiento humano sobre la solución óptima en vez de dejar que la IA lo descubra:

1. **Input de Hamiltonian path**: precalcular un ciclo Hamiltoniano del grid y darle a la red la dirección óptima como input. La red solo tendría que aprender a seguirlo cuando el cuerpo es largo.
2. **Partidas que empiezan con serpiente larga**: mezclar partidas normales con partidas que empiecen a score ~250, forzando al modelo a practicar el endgame.
3. **Híbrido IA + algoritmo**: usar la red NEAT para el juego normal pero cambiar a un algoritmo determinista (e.g., follow-the-wall) cuando el cuerpo supera el 90% del tablero.

Ninguna fue implementada. El resultado de **286/297 con NEAT puro** (sin heurísticas inyectadas) es el límite natural del algoritmo para este problema.

> **Video del resultado:**

<video src="videos/final.mp4" width="100%" controls muted></video>

---

## Conclusión del proyecto

De la v1 (serpientes dando vueltas en bucle) a la v23 (286/297, 96.3% del tablero), este proyecto recorrió **23 iteraciones** de diseño, debugging y optimización. Las lecciones más importantes:

| Lección | Versión |
|---------|---------|
| Nunca recompensar supervivencia directamente | v1 |
| La señal de fitness lo es todo | v8 |
| La especiación requiere parámetros calibrados al tamaño del genoma | v17-v20 |
| Preservar los mejores genomas es crítico (élite global) | v21 |
| El timeout dinámico resuelve el dilema presión-vs-permisividad | v22 |
| Más población = más diversidad = mejores resultados | v22 |
| NEAT tiene un límite práctico en problemas que requieren patrones exactos | v23 |

### Progresión histórica de max score

```
v1:  0    → v8:  50+  → v12: 103  → v21: 156  → v22: 284  → v23: 286
     ↑ bucles    ↑ señal     ↑ anti-loop   ↑ élite     ↑ dinámico   ↑ endgame
```

### Estado técnico final

| Parámetro | Valor |
|-----------|-------|
| Población | 2000 genomas |
| Inputs | 64 |
| Outputs | 4 (UP, RIGHT, DOWN, LEFT) |
| Partidas/genoma | 4 |
| maxStepsPerFood | dinámico: 200 + score×2 |
| stagnationLimit | 30 |
| compatC3 | 1.5 |
| compatThreshold | 2.0 |
| N cap (compatibilidad) | 100 |
| Élites globales | 10 |
| Élites por especie | 2 |
| Endgame bonus | (score-200)³×50 para score>200 |
| Win bonus | 500K |
| Flood fill | 8 BFS/movimiento (4 comida + 4 cola) |
| Evaluación | Work-stealing, todos los cores CPU |
| Activación | Sigmoid |
| Evaluación red | Feed-forward, orden topológico (Kahn's) |
