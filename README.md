# 🐍 Neural Snake — IA que aprende a jugar Snake con NEAT

Una implementación desde cero en **C++17** de una inteligencia artificial que aprende a jugar al clásico juego de Snake utilizando **NEAT** (NeuroEvolution of Augmenting Topologies). El proyecto incluye el juego completo con estética Google Snake, un modo para jugar manualmente y un modo de entrenamiento en tiempo real donde puedes ver cómo la IA evoluciona generación tras generación.

<p align="center">
  <img src="https://img.shields.io/badge/C++-17-blue?logo=cplusplus" />
  <img src="https://img.shields.io/badge/OpenGL-4.6-green?logo=opengl" />
  <img src="https://img.shields.io/badge/NEAT-Neuroevolution-orange" />
  <img src="https://img.shields.io/badge/ImGui-1.91.8-red" />
</p>

---

## Demostración

### Primera iteración — La IA da vueltas en bucle
En las primeras pruebas, la función de fitness recompensaba la supervivencia, por lo que la serpiente aprendía a evitar la muerte dando vueltas en círculos en vez de buscar comida.

https://github.com/user-attachments/assets/placeholder-video-1

> *`videos/1 iteracion.mp4`*

### Segunda iteración — Fitness corregido
Tras ajustar la función de fitness para premiar acercarse a la comida y eliminar la recompensa por tiempo de vida, la serpiente empieza a buscar activamente la manzana.

https://github.com/user-attachments/assets/placeholder-video-2

> *`videos/2iter.mp4`*

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
- **14 entradas**: dirección de la comida (2), peligro en 8 direcciones (8), dirección actual one-hot (4)
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
fitness = score × 5000 + score² × 500 + approach_bonus
```

- **`score × 5000`** — Recompensa principal por cada manzana comida
- **`score² × 500`** — Bonus cuadrático que premia comer más (escala exponencial)
- **`approach_bonus`** — Pequeño incentivo por acercarse a la comida paso a paso; penaliza alejarse. Esto evita que la serpiente aprenda a dar vueltas en bucle.

> No se recompensa el tiempo de supervivencia. Esto es clave: sin esta decisión, las serpientes aprenden que sobrevivir da más fitness que arriesgarse a buscar comida.

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

- [ ] Guardar/cargar el mejor genoma a disco
- [ ] Inputs más avanzados (raycast en múltiples direcciones, distancia al cuerpo)
- [ ] Gráfica de score por generación
- [ ] Soporte multiplataforma (Linux/macOS)
- [ ] Modo de replay guardado para compartir partidas

---

## Autor

**Alejandro Zabala** — [GitHub](https://github.com/ZabaHD4K)
