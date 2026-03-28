# Neural Snake — IA que aprende a jugar Snake con NEAT

Una implementación **desde cero** en C++17 de una inteligencia artificial que aprende a jugar al clásico juego de Snake utilizando **NEAT** (NeuroEvolution of Augmenting Topologies). Sin librerías de IA, sin frameworks de machine learning — solo C++, matemáticas y evolución.

**Resultado final: 286/297 (96.3% del tablero) con NEAT puro.**

<p align="center">
  <img src="https://img.shields.io/badge/C++-17-blue?logo=cplusplus" />
  <img src="https://img.shields.io/badge/OpenGL-4.6-green?logo=opengl" />
  <img src="https://img.shields.io/badge/NEAT-Neuroevolution-orange" />
  <img src="https://img.shields.io/badge/ImGui-1.91.8-red" />
  <img src="https://img.shields.io/badge/Score-286%2F297-brightgreen" />
</p>

---

## Demo

> Video del modelo final jugando (v23 — 286 puntos):

<video src="videos/final.mp4" width="100%" controls muted></video>

---

## Diario de desarrollo

23 versiones iterativas, cada una con su problema, análisis y solución. **[Ver diario completo →](DIARY.md)**

| Versión | Max Score | Cambio clave |
|---------|-----------|--------------|
| v1 | 0 | Eliminar recompensa por supervivencia (IA daba vueltas en bucle) |
| v3 | 6 | Raycast 28 inputs, fix topo sort, fitness con gradiente |
| v8 | 50+ | La señal correcta de fitness lo es todo |
| v12 | 103 | Hambre + exploration bonus + dirección cola (35 inputs) |
| v17 | 74 | 89 inputs pero 1 sola especie — N normalización rota |
| v19 | 82 | Cap N=100, poda 89→64 inputs |
| v20 | 94 | Fix especiación: C3=1.5, compatThreshold=2.0 |
| v21 | **156** | Elite global: top genomas sobreviven siempre |
| v22 | **284** | maxSteps dinámico (200+score×2), población 2000 |
| v23 | **286** | Bonus cúbico endgame, win bonus 500K, 10 élites |

---

## Cómo funciona

### El algoritmo NEAT

[NEAT](http://nn.cs.utexas.edu/downloads/papers/stanley.cec02.pdf) (NeuroEvolution of Augmenting Topologies) evoluciona simultáneamente la **topología** y los **pesos** de redes neuronales. A diferencia de algoritmos que optimizan redes de arquitectura fija, NEAT empieza con redes mínimas y las hace crecer.

```
Generación 1:     64 inputs ──→ 4 outputs  (solo conexiones directas)
                         ...evolución...
Generación 500:   64 inputs ──→ [hidden nodes] ──→ 4 outputs  (topología compleja)
```

**Ciclo evolutivo:**

1. **Evaluación**: cada genoma juega 4 partidas de Snake y recibe un fitness
2. **Especiación**: genomas similares se agrupan en especies (protege innovaciones)
3. **Selección**: dentro de cada especie, los mejores sobreviven
4. **Reproducción**: crossover alineado por innovación + mutaciones
5. **Mutaciones**: modificar pesos, añadir conexiones, añadir nodos, activar/desactivar conexiones

### La red neuronal

**64 entradas** que le dan a la serpiente una visión completa del tablero:

| Inputs | Cantidad | Descripción |
|--------|----------|-------------|
| Raycast 8 direcciones | 16 | Distancia a pared + comida visible por dirección (×2 canales) |
| Dirección actual | 4 | One-hot: UP, RIGHT, DOWN, LEFT |
| Flood fill | 4 | Espacio accesible si voy en cada dirección (normalizado) |
| Hambre | 1 | Urgencia: 0 (acaba de comer) → 1 (a punto de morir por timeout) |
| BFS dirección comida | 4 | One-hot: mejor primer paso hacia la comida por BFS |
| Vector a comida | 2 | Dirección relativa normalizada (dx/W, dy/H) |
| BFS dirección cola | 4 | One-hot: mejor primer paso hacia la cola (con cola marcada como pasable) |
| Seguridad | 4 | Por dirección: ¿puedo llegar a mi cola si voy por ahí? (1/0) |
| Visión local 5×5 | 25 | Grid 5×5 centrado en la cabeza: 1=obstáculo, 0=libre |

**4 salidas**: UP, RIGHT, DOWN, LEFT. Argmax decide el movimiento.

**Activación**: sigmoid. **Evaluación**: feed-forward con orden topológico (Kahn's algorithm).

### Función de fitness

```
fitness = score × 5000 + score² × 500 + efficiencyBonus + approachBonus + explorationBonus
        + endgameBonus                    (si score > 200)
        - 10% penalización               (si muere por colisión con score > 10)
        + 500,000                         (si gana)
```

| Componente | Fórmula | Propósito |
|------------|---------|-----------|
| Base lineal | `score × 5000` | Recompensa principal por manzana |
| Base cuadrática | `score² × 500` | Premia comer más (rendimientos crecientes) |
| Eficiencia | `(1 - pasos/maxSteps) × 1000` por fruta | Premia comer rápido |
| Approach | `stepsToward × 1.5 - stepsAway × 0.75` por fruta | Recompensa acercarse a la comida (asimétrico para permitir rodeos) |
| Exploración | `uniqueCells / stepsThisFruit × 500` por fruta | Penaliza bucles implícitamente |
| Endgame | `(score-200)³ × 50` si score>200 | Gradiente fuerte hacia las últimas celdas |

**Diseño clave**: approach y exploración se acumulan **por fruta** en vez de globalmente. El timeout es **dinámico**: `200 + score × 2` pasos sin comer = muerte. Esto presiona a las serpientes cortas a ser eficientes mientras permite a las largas rodear su cuerpo.

### Especiación

La función de compatibilidad entre genomas:

```
d = C1 × excess / N + C2 × disjoint / N + C3 × avgWeightDiff
```

Con `N` capado a 100 para evitar que genomas con muchas conexiones (64 inputs × 4 outputs = 256) produzcan distancias artificialmente bajas. `C3=1.5` fue calibrado experimentalmente: 0.4 producía 1 sola especie, 3.0 producía 1000 micro-especies.

### Élite global

Los **10 mejores genomas** de la población sobreviven siempre, independientemente de la especie. Sin esto, el mejor genoma podía perderse cuando su especie era eliminada por estagnación (v20→v21: de 94 a 156 puntos solo con este cambio).

---

## Características

### Juego Snake completo
- Estética fiel al **Google Snake** (colores, manzana, ojos con pupilas direccionales)
- Modo manual jugable con teclado (flechas / WASD)
- **Se puede ganar** llenando todo el grid (297 frutas en 20×15)

### Entrenamiento en tiempo real
- **24 partidas simultáneas** en cuadrícula 6×4 a ×20 velocidad
- Panel central con MAX SCORE, avg score, tiempo, win rate
- Evaluación **multi-thread con work-stealing** (todos los cores CPU al 100%)
- Recuadro azul en la mejor partida en vivo
- **Training log** con timestamps, especies, fitness — copiable al clipboard
- Pausa con snapshot del estado actual al log

### Modo Play
- Prueba la IA entrenada en modo Play con raycasts visibles
- Al pulsar PLAY durante el entrenamiento, usa el mejor modelo actual
- Auto-restart para ver múltiples partidas seguidas
- Pantalla de victoria con borde dorado cuando la IA completa el tablero

### Guardar / Cargar modelo
- **SAVE MODEL**: guarda el mejor genoma actual en `models/best.genome` (+ copia con timestamp)
- **LOAD MODEL**: carga un modelo previamente guardado y lo usa para jugar
- El modelo pre-entrenado está disponible en la carpeta `models/` del repositorio

---

## Arquitectura del proyecto

```
neural-snake/
├── src/
│   ├── main.cpp              # Ventana OpenGL + ImGui setup
│   ├── app.cpp / app.h       # Lógica principal, rendering, UI, modos Play/Train
│   ├── game/
│   │   └── snake_game.cpp/h  # Motor del juego Snake completo
│   ├── neat/
│   │   ├── genome.cpp/h      # Genoma: nodos, conexiones, mutación, crossover, compatibilidad
│   │   ├── population.cpp/h  # Población: epoch, especiación, reproducción, élite global
│   │   ├── species.h         # Estructura de especies
│   │   └── neat_params.h     # Todos los parámetros del algoritmo
│   ├── eval/
│   │   ├── network.cpp/h     # Fenotipo: construcción y forward pass (Kahn's topo sort)
│   │   └── evaluator.cpp/h   # 64 inputs, fitness, BFS, flood fill
│   └── util/
│       └── random.h          # RNG thread-safe
├── extern/
│   └── glad/                 # OpenGL loader (GLAD 2)
├── models/                   # Modelos entrenados (.genome)
├── videos/                   # Demos del progreso por versión
├── DIARY.md                  # Diario completo: 23 versiones iterativas
├── CMakeLists.txt            # Build system (FetchContent para GLFW + ImGui)
└── build_and_run.bat         # Script de compilación (Windows/MSVC)
```

---

## Compilación y ejecución

### Requisitos
- **Windows 10/11** con GPU compatible con OpenGL 4.6
- **MSVC** (Visual Studio 2022 Build Tools o Community)
- **CMake** ≥ 3.24
- **Ninja** (opcional, recomendado)

### Compilar y ejecutar

```bash
# Script automático (configura entorno MSVC, compila y ejecuta)
build_and_run.bat

# O manualmente:
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl
cmake --build build
./build/neural_snake.exe
```

> Las dependencias (GLFW 3.4, Dear ImGui 1.91.8) se descargan automáticamente via CMake FetchContent.

---

## Controles

| Tecla | Acción |
|-------|--------|
| `Flechas` / `WASD` | Mover serpiente (modo Play manual) |
| `Espacio` | Iniciar / Pausar |
| `ESC` | Salir |
| **PLAY** | Ver IA jugar con el mejor modelo (para entrenamiento y vuelve a Play) |
| **AI TRAIN** | Iniciar entrenamiento NEAT |
| **PAUSE / RESUME** | Pausar/reanudar (PAUSE guarda snapshot al log) |
| **WATCH AI PLAY** | Ver IA jugar en modo Play con raycasts |

---

## Parámetros finales

| Parámetro | Valor | Notas |
|-----------|-------|-------|
| Población | 2000 | Más diversidad genética |
| Inputs / Outputs | 64 / 4 | Ver tabla de inputs arriba |
| Games/genome | 4 | Promedio para reducir varianza |
| maxStepsPerFood | 200 + score×2 | Dinámico: presión alta al inicio, permisivo con cuerpo largo |
| Stagnation limit | 30 generaciones | Antes de eliminar una especie |
| Compat C1/C2/C3 | 1.0 / 1.0 / 1.5 | C3 calibrado para 64 inputs |
| Compat threshold | 2.0 | Produce 30-50 especies típicamente |
| N cap | 100 | Evita que N grande aplaste las diferencias |
| Global elites | 10 | Sobreviven siempre, independiente de especie |
| Species elites | 2 | Top 2 por especie |
| Survival fraction | 0.25 | 25% de cada especie puede reproducirse |
| Crossover rate | 0.75 | 75% de hijos por crossover, 25% por copia+mutación |
| Weight mutate | 0.80 | Probabilidad de mutar pesos |
| Add connection | 0.08 | Probabilidad de nueva conexión |
| Add node | 0.05 | Probabilidad de nuevo nodo oculto |
| Endgame bonus | (s-200)³×50 | Gradiente fuerte >200 puntos |
| Win bonus | 500,000 | Incentivo por completar el tablero |

---

## Tecnologías

| Tecnología | Versión | Uso |
|------------|---------|-----|
| **C++17** | — | Lenguaje principal |
| **OpenGL** | 4.6 | Rendering |
| **GLAD 2** | — | Loader de OpenGL |
| **GLFW** | 3.4 | Ventana y input |
| **Dear ImGui** | 1.91.8 | UI, rendering 2D (DrawList), gráficas |
| **NEAT** | custom | Implementación completa desde cero |
| **std::thread** | C++17 | Evaluación multi-thread con work-stealing |
| **BFS / Flood fill** | — | 8 BFS por movimiento para inputs espaciales |

---

## Lecciones aprendidas

Tras 23 iteraciones, las conclusiones más importantes:

1. **La señal de fitness lo es todo.** Un cambio en la función de fitness tiene más impacto que cualquier cambio de arquitectura o parámetros. El salto de 6 a 50+ puntos fue solo por mejorar el fitness.

2. **Preservar los mejores genomas es crítico.** Sin élite global, el mejor genoma puede desaparecer cuando su especie estagna. Este cambio solo fue de 94 a 156 puntos.

3. **El timeout dinámico resuelve dilemas.** `200 + score×2` presiona a las serpientes cortas a ser eficientes mientras permite a las largas navegar. Timeout fijo alto (500) permitía bucles; fijo bajo (300) mataba serpientes largas legítimas.

4. **Más diversidad > más generaciones.** Duplicar la población (1000→2000) tuvo más impacto que duplicar el tiempo de entrenamiento. Cada generación explora más soluciones.

5. **NEAT tiene un límite en patrones exactos.** Problemas que requieren secuencias geométricas perfectas (como llenar el 100% del tablero) están fuera del alcance de la optimización estocástica. 286/297 es el límite práctico.

---

## Autor

**Alejandro Zabala** — [GitHub](https://github.com/ZabaHD4K)
