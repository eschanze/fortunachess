# Fortuna Chess

**Proyecto Final ICI2240-2 (2025-1)**

**Autores:**
- Benjamín Bustos
- Esteban Schanze  
- Isabot Sonnier

**Profesor:** Ignacio Araya

## Descripción

Fortuna Chess es un programa de consola que busca simualr de manera fiel el juego clásico del ajedrez utilizando Tipos de Datos Abstractos (TDAs) y la reprentación de tablero conocida como **0x88**. Permite al usuario jugar una partida contra otra persona de forma local (en el mismo computador) o enfrentarse a un oponente controlado por la máquina. Al iniciar el programa, el usuario accede a un menú donde puede seleccionar entre distintas modalidades de juego.

## Estructura del proyecto
```
fortuna-chess/
│
├── main.c # Menú principal, lógica del juego y bucle de partida
├── bot.c # Implementación del bot de ajedrez (minimax, evaluación)
├── chess.c # Reglas del juego, movimientos legales, validación, generación, y utilidades de tablero
├── zobrist.c # Generación de claves Zobrist compatibles con formato PolyGlot (book.bin)
├── hashtable.c # Implementación de TDA hashtable para almacenamiento de libro de aperturas
├── stack.c # Implementación de TDA pila para historial de movimientos y deshacer
│
├── bot.h # Definiciones de las funciones para el bot
├── chess.h # Definiciones de tipos y funciones del motor de ajedrez
├── zobrist.h # Definición de función Zobrist Hashing
├── hashtable.h # Deficiones de la estructura hashtable
├── stack.h # Definiciones de la estructura pila
│
└── README.md # Documentación del proyecto
```

## Cómo compilar y ejecutar

### Requisitos previos

- **Compilador de C**:
  - En Windows: MinGW-GCC, MSVC (cl.exe) o GCC a través de WSL
  - En Linux/macOS: GCC o Clang
- **Recomendado**: GCC (compilador), Visual Studio Code con la extensión C/C++ (IDE)

### Compilación y ejecución

**Paso 0: Descargar el proyecto**  
Descargue el archivo `.zip` del proyecto y descomprímalo en el directorio de su preferencia.

**Paso 1: Abrir el proyecto en su entorno de desarrollo**  
(Si está usando Visual Studio Code)

1. Abra VS Code  
2. Seleccione la carpeta del proyecto (`File > Open Folder...`)  
3. Abra el terminal integrado (`Terminal > New Terminal`)  

**Paso 2: Compilar el proyecto**
- Usando GCC (Linux, macOS o Windows con MinGW/Git Bash):

  ```bash
  gcc *.c -o fortunachess
  ```

- Usando GCC, pero para un mejor rendimiento:
  ```bash
  gcc -O3 -march=native -flto *.c -o fortunachess
  ```

- Usando el compilador de Visual Studio (cl.exe), en Visual Studio Developer PowerShell:
  ```bash
  cl /Fe:fortunachess.exe main.c chess.c bot.c zobrist.c hashtable.c stack.c
  ```
**Paso 3: Ejecute la aplicación**
- Ejecute el siguiente comando, dentro del directorio del proyecto
  ```bash
  ./fortunachess
  ```
**Alternativa sin VS Code:**

1. Abra una terminal o línea de comandos
2. Navegue hasta el directorio del proyecto
3. Compile y ejecute con los comandos anteriores

## Implementación técnica

El desarrollo de la aplicación se sustentó en el uso de tres TDAs fundamentales vistos durante el curso: pila, tabla hash y grafo. Cada uno de estos TDAs fue integrado de forma concreta y funcional dentro del motor de ajedrez, permitiendo modelar estructuras complejas como el historial de jugadas, la evaluación estratégica de posiciones y la exploración del espacio de estados del juego. A continuación, se describe cómo fue utilizada cada estructura, su propósito específico dentro del sistema y su importancia en el rendimiento y funcionalidad general de la aplicación.

**Grafo (implícito)**
- Implementación: Se utilizó un grafo implícito para representar el espacio de estados del juego. Cada nodo corresponde a un estado completo del tablero, modelado mediante la estructura gamestate_t, que incluye la posición actual de las piezas, el turno, los derechos de enroque, entre otros datos relevantes. A partir de un nodo, se generan todos los nodos adyacentes (es decir, las posibles jugadas legales) mediante la función generate_moves, que produce una lista de movimientos válidos. Al aplicar cada uno de esos movimientos, se crean nuevos estados, es decir, nuevos nodos del grafo.
- Uso en la aplicación: Esta estructura implícita es fundamental para el modo de juego contra la máquina, donde se implementó el algoritmo minimax con poda alpha-beta. Este algoritmo explora el grafo de forma recursiva (una variación del recorrido en profundidad, DFS), evaluando los estados hoja con una función de evaluación heurística y propagando los valores hacia arriba para determinar el mejor movimiento. La representación implícita mediante estructuras de datos y funciones permitió simular eficientemente la búsqueda sin necesidad de construir explícitamente todo el grafo en memoria.

**Hashtable**
- Implementación: Se implementó una tabla hash que asocia posiciones del tablero con listas de movimientos previamente evaluados. Para identificar unívocamente cada estado del juego, se utilizó Zobrist Hashing, una técnica de hashing eficiente diseñada específicamente para juegos como el ajedrez. Cada combinación de pieza y casilla tiene asociado un valor aleatorio predefinido; al combinar estos valores se obtiene un número de 64 bits que representa el estado completo del tablero de forma única y rápida de calcular.
- Uso en la aplicación: La tabla hash se utilizó para almacenar un libro de jugadas pre calculadas que se carga al iniciar el programa desde un archivo binario (book.bin). Este archivo contiene más de 200.000 posiciones, cada una identificada por su hash Zobrist y asociada a una lista de movimientos óptimos con distintos niveles de prioridad estratégica. Durante el modo Jugador vs Máquina, si el estado actual del tablero coincide con una entrada en la tabla, el bot puede seleccionar una jugada directamente desde el libro, acelerando la apertura y ofreciendo respuestas de mayor calidad en las primeras fases del juego. El archivo corresponde a un formato de licencia libre ampliamente utilizado en motores de ajedrez, llamado PolyGlot.

**Pila (Stack)**
- Implementación: Se utilizó una pila (chess_stack_t) para almacenar el historial completo de movimientos durante la partida. Cada vez que el usuario o la IA realiza una jugada, se guarda en la pila una estructura history_entry_t que almacena no solo el movimiento realizado, sino también todo el estado previo del juego: tablero, derechos de enroque, turno, casilla de captura al paso, etc.
- Uso en la aplicación:	La pila permite implementar de manera eficiente la funcionalidad de deshacer movimiento (comando "deshacer"), ya que basta con desapilar la última entrada para restaurar el estado exacto anterior. Además, se utiliza para mostrar el historial de jugadas al finalizar la partida. Esta estructura fue clave para lograr una navegación fluida entre estados y para facilitar el desarrollo de herramientas adicionales de análisis.

## Funcionalidades


#### Menú interactivo en la terminal
- **Menú principal** con selección entre:
  - Jugador vs Jugador (PvP)
  - Jugador vs CPU (PvE)
  - Salir del juego
- **Submenús** para:
  - Selección de control de tiempo (`Blitz`, `Rápido`, o `Sin tiempo`)
  - Elección de color de piezas (`Blancas`, `Negras`, o `Aleatorio`)

#### Lógica de juego
- Soporte completo para movimientos estándar de ajedrez (incluyendo enroques, promoción, en passant)
- Validación de legalidad de los movimientos y chequeo de jaque
- Comandos dentro del juego:
  - `ayuda`: muestra los comandos disponibles
  - `historial`: muestra el número de movimientos realizados (próximamente lista completa)
  - `deshacer`: revierte el último movimiento
  - `salir`: termina la partida

#### Temporizador
- Cuenta regresiva por jugador cuando se juega con tiempo
- Finalización automática si un jugador agota su tiempo

#### Oponente CPU
- Oponente bot básico usando búsqueda **minimax** (basado en grafos implícitos) con profundidad configurable
- Evaluación simple basada en material

#### Libro de aperturas (PolyGlot)
- Implementación de **Zobrist hHshing** compatible con formato PolyGlot
- Carga de un archivo `book.bin` con 256.000 aperturas para sugerencia de movimientos en posiciones conocidas

#### Benchmarking
- Soporte para **PERFT benchmarking** desde FEN personalizado
- Pruebas automáticas de:
  - Generación de movimientos
  - Hashing de posiciones
  - Lookup de aperturas en la tabla hash

### Problemas conocidos

- El comando `historial` aún no imprime los movimientos realizados, solo la cantidad de movimientos en la pila.
- No se detectan la condición de fin de partida cuando se repiten tres movimientos (regla de triple repetición).
- En Windows, los caracteres especiales podrían no mostrarse correctamente si no se configura la consola para UTF-8.

## Aspectos a mejorar / Funcionalidades futuras

- [ ] Mostrar lista completa del historial de movimientos con formato algebraico.
- [ ] Agregar soporte para tablas por triple repetición
- [ ] Mejorar la interfaz de línea de comandos (incluir limpieza de pantalla y diseño más interactivo).
- [ ] Incorporar detección de mate y ahogado directamente en el motor (`chess.c`)
- [ ] Optimizar el rendimiento del bot, especificamente la función de evaluación.
- [ ] Incluir más niveles de dificultad para el modo Jugador vs. Máquina.

## Ejemplo de Uso

**Paso 1: Menú Principal**

Pregunta al ususario si desea jugar modo PvP o PvE, después de su elección comienza la partida.
```text
◼◻◼◻◼◻◼◻◼◻◼◻◼◻◼◻◼◻◼◻◼◻◼◻◼◻◼◻◼◻◼◻◼◻◼◻◼◻◼◻◼
◻                 ♜   𝓕𝓞𝓡𝓣𝓤𝓝𝓐   𝓒𝓗𝓔𝓢𝓢   ♜              ◻
◼◻◼◻◼◻◼◻◼◻◼◻◼◻◼◻◼◻◼◻◼◻◼◻◼◻◼◻◼◻◼◻◼◻◼◻◼◻◼◻◼
Bienvenido/a, elija una opción:
1. Jugador vs Jugador (PvP) ⚔︎
2. Jugador vs CPU (PvE)⌨︎
3. Salir :(
1
```
Se inicia partida Jugador vs Jugador.

**Paso 2: Submenú - Formato Tiempo**

Pregunta al ususario el formato de tiempo que desea jugar.
```text
⏱︎ FORMATO DE TIEMPO ⏱︎
1. Blitz (3 min)
2. Rápido (10 min)
3. Sin tiempo
4. Volver al menú principal
Elija una opción:
1
```
Formato tiempo será Blitz (3 minutos).

**Paso 3: Submenú - Selección de Piezas**

Pregunta al ususario con que color de piezas desea jugar, el jugador 1 es quien elige.
```text
𖣯 SELECCIÓN DE PIEZAS 𖣯
1. Blancas
2. Negras
3. Aleatorio
4. Volver al menú principal
Elija una opción:
1
```
Jugador 1 a elegido piezas Blancas.

**Paso 4: Comienza la Partida**

La partida comienza teniendo en cuenta las opciones anteriormente escogidas. 
```text
♚ INICIANDO PARTIDA ♛
Jugador 1: Blancas
Formato: Blitz
Modo: vs Jugador
¡Que comience el juego! :)


    a b c d e f g h
  +-----------------+
8 | r n b q k b n r | 8
7 | p p p p p p p p | 7
6 | . . . . . . . . | 6
5 | . . . . . . . . | 5
4 | . . . . . . . . | 4
3 | . . . . . . . . | 3
2 | P P P P P P P P | 2
1 | R N B Q K B N R | 1
  +-----------------+
    a b c d e f g h

Turno: Blancas (Jugador 1)
Derechos de enroque: KQkq
[ DEBUG ] Hash de la posición: 117617679412923

Ingrese movimientos en formato: e2e4
Escriba 'ayuda' para ver todos los comandos disponibles
Escriba 'salir' para salir

Ingrese movimiento o comando:
```

**Paso 5: Ingresar movimiento**

El jugador debe ingresar un movimiento siguiendo el formato dado (e2e4).
```text
Ingrese movimiento o comando: e2e4
Tiempo restante - Blancas: 2:52 | Negras: 3:00

    a b c d e f g h
  +-----------------+
8 | r n b q k b n r | 8
7 | p p p p p p p p | 7
6 | . . . . . . . . | 6
5 | . . . . . . . . | 5
4 | . . . . P . . . | 4
3 | . . . . . . . . | 3
2 | P P P P . P P P | 2
1 | R N B Q K B N R | 1
  +-----------------+
    a b c d e f g h

Turno: Negras (Jugador 2)
Derechos de enroque: KQkq
[ DEBUG ] Hash de la posición: 115590454846740
Casilla en passant: e3

Ingrese movimiento o comando:
```

**Paso 6: Ingresar comando**

El jugador puede ingresar un comando ('ayuda', 'salir', 'deshacer').
```text
Casilla en passant: e3

Ingrese movimiento o comando: deshacer
Tiempo restante - Blancas: 2:52 | Negras: 2:03

    a b c d e f g h
  +-----------------+
8 | r n b q k b n r | 8
7 | p p p p p p p p | 7
6 | . . . . . . . . | 6
5 | . . . . . . . . | 5
4 | . . . . . . . . | 4
3 | . . . . . . . . | 3
2 | P P P P P P P P | 2
1 | R N B Q K B N R | 1
  +-----------------+
    a b c d e f g h

Turno: Blancas (Jugador 1)
Derechos de enroque: KQkq
[ DEBUG ] Hash de la posición: 117617679412923

Movimiento deshecho.
Ingrese movimiento o comando:
````

## Contribuciones

- **Esteban Schanze** estuvo a cargo del diseño de las estructuras principales del sistema, incluyendo gamestate_t, move_t y history_entry_t. Implementó la generación de movimientos legales, el algoritmo de hashing Zobrist, y la estructura de tabla hash para el libro de aperturas. También desarrolló la lógica de búsqueda mediante grafos implícitos (algoritmo minimax con poda alpha-beta), y realizó pruebas, ajustes finales y mejoras del bot.
    - Auto-evaluación: 3 (Aporte excelente).
- **Isabot Sonnier** se encargó de la representación visual del tablero, la implementación de menús y submenús interactivos, y la redacción de múltiples secciones del informe (descripción de la aplicación, TDAs, funciones básicas, experiencia de usuario, entre otras). Además, implementó la evaluación de posiciones y mejoró la presentación visual del sistema.
    - Auto-evaluación: 3 (Aporte excelente).
- **Benjamín Bustos** desarrolló las funciones relacionadas a la aplicación y verificación de movimientos, así como el uso del TDA pila para implementar el historial de jugadas y la funcionalidad de deshacer. También fue responsable de modularizar el código, implementar el temporizador y colaborar en la redacción final del informe y las conclusiones.
    - Auto-evaluación: 3 (Aporte excelente).

Las tareas de definición del proyecto, redacción conceptual inicial, revisión final del informe y preparación de la presentación fueron realizadas en conjunto por todo el equipo.