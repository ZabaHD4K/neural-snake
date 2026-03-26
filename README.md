# 🐍 Neural Snake — IA que aprende a jugar Snake con NEAT

Una implementación desde cero en **C++17** de una inteligencia artificial que aprende a jugar al clásico juego de Snake utilizando **NEAT** (NeuroEvolution of Augmenting Topologies). El proyecto incluye el juego completo con estética Google Snake, un modo para jugar manualmente y un modo de entrenamiento en tiempo real donde puedes ver cómo la IA evoluciona generación tras generación.

<p align="center">
  <img src="https://img.shields.io/badge/C++-17-blue?logo=cplusplus" />
  <img src="https://img.shields.io/badge/OpenGL-4.6-green?logo=opengl" />
  <img src="https://img.shields.io/badge/NEAT-Neuroevolution-orange" />
  <img src="https://img.shields.io/badge/ImGui-1.91.8-red" />
</p>

---

## Diario de desarrollo

Aquí documento el proceso iterativo de desarrollo: cada problema encontrado, el análisis, y la solución aplicada. El proyecto no salió bien a la primera — cada iteración enseñó algo nuevo sobre cómo diseñar sistemas de IA evolutiva.

### v1 — La IA da vueltas en bucle

La primera versión del sistema NEAT estaba lista: 500 serpientes evolucionando, 14 inputs (dirección a la comida + peligro en casillas adyacentes + dirección actual), y una función de fitness que recompensaba la supervivencia:

```
fitness = score × 5000 + totalSteps + score² × 500
```

**Resultado:** las serpientes descubrieron que sobrevivir daba más fitness que arriesgarse a buscar comida. Aprendieron a dar vueltas en círculos indefinidamente, maximizando `totalSteps` sin comer nunca.

**Lección:** nunca recompensar la supervivencia directamente. Si el agente puede ganar puntos sin hacer lo que quieres, lo hará.

https://github.com/user-attachments/assets/placeholder-video-1

> *`videos/1 iteracion.mp4`*

---

### v2 — Eliminamos la recompensa por supervivencia

**Cambio:** eliminé `totalSteps` de la fitness y añadí un bonus por acercarse a la comida (distancia Manhattan):

```
fitness = score × 5000 + score² × 500 + max(0, approachBonus)
```

**Resultado:** las serpientes ya no hacían bucles, pero tras **2000 generaciones** apenas conseguían 0-2 frutas. La evolución estaba estancada.

https://github.com/user-attachments/assets/placeholder-video-2

> *`videos/2iter.mp4`*

---

### v3 — Diagnóstico profundo: tres bugs críticos

Tras analizar por qué 2000 generaciones no bastaban, encontré tres problemas simultáneos:

#### Bug 1: Fitness sin gradiente
El `approachBonus` usaba `std::max(0, bonus)` — si la serpiente se alejaba más de lo que se acercaba, recibía literalmente **0 de fitness**. NEAT no podía distinguir entre una serpiente que muere en 1 paso y una que casi llega a la comida. Sin gradiente, la evolución era aleatoria.

#### Bug 2: Visión de 1 sola casilla
Los sensores de peligro solo miraban la casilla inmediatamente adyacente (binario: peligro sí/no). La serpiente no tenía noción de distancia: no sabía si una pared estaba a 1 o a 15 casillas.

#### Bug 3: Topological sort incorrecto en la red neuronal
El algoritmo de Kahn para ordenar la evaluación de nodos no contaba las conexiones provenientes de inputs al calcular los grados de entrada (`inDeg`). Resultado: nodos de salida conectados directamente a inputs se evaluaban **antes** de recibir la señal. La red procesaba datos en orden incorrecto.

**Solución aplicada:**

1. **Fitness con gradiente real** — Cada paso hacia la comida suma +1.5, cada paso alejándose resta -2.0. Se permite fitness negativa (con suelo en 0.001). Ahora NEAT puede distinguir "casi buena" de "basura":
```
fitness = score × 5000 + score² × 500 + stepsToward × 1.5 - stepsAway × 2.0
```

2. **Raycast en 8 direcciones** — Cada dirección (N, NE, E, SE, S, SW, W, NW) lanza un rayo que reporta 3 valores: distancia normalizada a pared (`1/dist`), distancia a cuerpo (`1/dist`, 0 si no hay), y si hay comida en esa línea (1/0). Total: 24 inputs de visión + 4 de dirección = **28 inputs**.

3. **Fix del topological sort** — Ahora las conexiones desde inputs cuentan en el cálculo de `inDeg`, asegurando que los nodos se evalúan después de recibir todas sus señales de entrada.

4. **Visualización de raycasts** — Añadí rendering en tiempo real de los 8 rayos durante el entrenamiento: líneas blancas hasta las paredes, puntos rojos donde detecta cuerpo, y líneas verdes hacia la comida visible.

https://github.com/user-attachments/assets/placeholder-video-3

> *`videos/3.mp4`*

---

### v4 — Bucles de nuevo a 6 puntos

Con los raycast y el fitness con gradiente, la serpiente aprendió a buscar comida y llegar a 6 puntos. Pero entonces se estancó: tras comer 6 frutas, volvía a dar vueltas en bucle.

**Diagnóstico:** con 6 frutas ya tenía `6×5000 + 36×500 = 48.000` de fitness. Los 200 pasos de margen (`maxStepsPerFood`) eran demasiado generosos: podía dar muchas vueltas sin penalización suficiente. La serpiente aprendió que el riesgo de morir buscando la 7a fruta no compensaba frente a la seguridad de hacer bucles.

**Solución:**
1. **`maxStepsPerFood` reducido a 100** — Muere antes si no come, menos tiempo para hacer bucles
2. **Bonus de eficiencia** — Recompensa extra por comer rápido: `(1 - pasos/maxSteps) × 1000`. Comer en 10 pasos da ~900 de bonus, comer en 90 pasos da ~100. Esto incentiva buscar la comida directamente en vez de dar rodeos.

```
fitness = score × 5000 + score² × 500 + efficiencyBonus + stepsToward × 1.5 - stepsAway × 2.0
```

https://github.com/user-attachments/assets/placeholder-video-4

> *`videos/4.mp4`*

---

## Características

### 🎮 Juego Snake completo
- Estética fiel al **Google Snake** (colores, manzana, ojos con pupilas direccionales)
- Modo manual jugable con teclado (flechas / WASD)
- Velocidad progresiva conforme comes más
- Sistema de puntuación y high score

### 🧠 NEAT (NeuroEvolution of Augmenting Topologies)
- Implementación completa del algoritmo NEAT desde cero
- **Evolución de topología**: la red neuronal empieza mínima y crece añadiendo nodos y conexiones
- **Especiación**: protege las innovaciones agrupando genomas similares
- **Crossover alineado por innovación**: combina genomas respetando la historia evolutiva
- Población de 500 individuos con selección por especies

### 📊 Entrenamiento en tiempo real
- Panel lateral con estadísticas en vivo (generación, fitness, score, especies)
- Gráfica de evolución del fitness (mejor y promedio)
- **Visualización de la red neuronal** con activaciones en tiempo real
- Replay del mejor genoma de cada generación
- Control de velocidad del replay + modo turbo
- Pausa con espacio y botón de stop

### 🧬 Red neuronal
- **28 entradas**: raycast en 8 direcciones × 3 canales (distancia a pared, distancia a cuerpo, comida visible) + dirección actual one-hot (4)
- **4 salidas**: arriba, derecha, abajo, izquierda (argmax decide el movimiento)
- Activación sigmoid, evaluación feed-forward con orden topológico (Kahn's algorithm)

---

## Arquitectura del proyecto

```
neural-snake/
├── src/
│   ├── main.cpp              # Ventana OpenGL + ImGui setup
│   ├── app.cpp / app.h       # Lógica principal, rendering, UI
│   ├── game/
│   │   └── snake_game.cpp/h  # Motor del juego Snake
│   ├── neat/
│   │   ├── genome.cpp/h      # Genoma: nodos, conexiones, mutación, crossover
│   │   ├── population.cpp/h  # Población: epoch, especiación, reproducción
│   │   ├── species.h         # Estructura de especies
│   │   └── neat_params.h     # Parámetros del algoritmo
│   ├── eval/
│   │   ├── network.cpp/h     # Fenotipo: construcción y forward pass
│   │   └── evaluator.cpp/h   # Evaluación de fitness
│   └── util/
│       └── random.h          # RNG thread-safe
├── extern/
│   └── glad/                 # OpenGL loader (GLAD 2)
├── videos/                   # Demos del progreso
├── CMakeLists.txt            # Build system
└── build_and_run.bat         # Script de compilación (Windows/MSVC)
```

---

## Función de fitness

```
fitness = score × 5000 + score² × 500 + efficiencyBonus + steps_toward × 1.5 - steps_away × 2.0
```

- **`score × 5000`** — Recompensa principal por cada manzana comida
- **`score² × 500`** — Bonus cuadrático que premia comer más (escala exponencial)
- **`efficiencyBonus`** — `(1 - pasos_hasta_comer / maxSteps) × 1000` por cada fruta. Premia comer rápido, penaliza dar rodeos
- **`steps_toward × 1.5`** — Bonus por cada paso que reduce la distancia Manhattan a la comida
- **`steps_away × 2.0`** — Penalización por cada paso que aumenta la distancia

> **Diseño clave:** la fitness tiene gradiente continuo y anti-bucle. Incluso serpientes que no comen reciben señal de mejora si se acercan a la comida. El bonus de eficiencia garantiza que las serpientes que comen rápido tienen ventaja evolutiva sobre las que dan rodeos. Timeout de 100 pasos sin comer = muerte.

---

## Compilación y ejecución

### Requisitos
- **Windows 10/11** con GPU compatible con OpenGL 4.6
- **MSVC** (Visual Studio 2022 Build Tools o Community)
- **CMake** ≥ 3.24
- **Ninja** (opcional, recomendado)

### Compilar

```bash
# Opción 1: Script automático (configura entorno MSVC)
build_and_run.bat

# Opción 2: Manual
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl
cmake --build build
./build/neural_snake.exe
```

> Las dependencias (GLFW, ImGui) se descargan automáticamente via CMake FetchContent.

---

## Controles

| Tecla | Acción |
|-------|--------|
| `Flechas` / `WASD` | Mover serpiente (modo Play) |
| `Espacio` | Iniciar / Pausar (ambos modos) |
| `ESC` | Salir |
| Panel: **PLAY / AI TRAIN** | Cambiar de modo |
| Panel: **Speed slider** | Velocidad del replay |
| Panel: **Turbo** | Entrenamiento a máxima velocidad |
| Panel: **Stop Training** | Detener el entrenamiento |

---

## Cómo funciona NEAT

1. Se crea una **población inicial** de 500 redes neuronales mínimas (solo input → output)
2. Cada red juega una partida de Snake y recibe un **fitness** basado en su rendimiento
3. Las redes se agrupan en **especies** por similitud genética
4. Las mejores redes de cada especie se **reproducen** (crossover + mutación)
5. Las mutaciones pueden **añadir nodos**, **añadir conexiones** o **modificar pesos**
6. Repetir desde el paso 2 → cada generación las serpientes juegan mejor

---

## Tecnologías

| Tecnología | Uso |
|------------|-----|
| **C++17** | Lenguaje principal |
| **OpenGL 4.6** | Rendering |
| **GLAD 2** | Loader de OpenGL |
| **GLFW 3.4** | Ventana y input |
| **Dear ImGui 1.91.8** | UI y rendering 2D (DrawList) |
| **NEAT** | Algoritmo de neuroevolución |
| **std::thread** | Entrenamiento en background |

---

## Roadmap

- [x] Raycast en 8 direcciones con visualización en tiempo real
- [x] Fitness con gradiente continuo (approach/retreat)
- [x] Pausa y stop del entrenamiento
- [ ] Guardar/cargar el mejor genoma a disco
- [ ] Gráfica de score por generación
- [ ] Soporte multiplataforma (Linux/macOS)
- [ ] Modo de replay guardado para compartir partidas

---

## Autor

**Alejandro Zabala** — [GitHub](https://github.com/ZabaHD4K)
