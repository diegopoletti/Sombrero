# Análisis del Juego del Sombrero Seleccionador de Harry Potter

## Funcionamiento General

Este programa es un juego interactivo basado en el universo de Harry Potter, específicamente en la ceremonia de selección del Sombrero Seleccionador. Utiliza un microcontrolador ESP32 para controlar varios componentes como pulsadores, servomotores, un LED RGB y un sistema de audio. El juego hace preguntas a los participantes y, basándose en sus respuestas, asigna puntos a las diferentes casas de Hogwarts (Gryffindor, Slytherin, Ravenclaw y Hufflepuff). Al final, determina la casa ganadora.

## Análisis de Funciones Principales

### 1. Función setup()

Esta función se ejecuta una vez al inicio del programa. Su propósito es configurar todos los componentes necesarios para el juego.

```cpp
void setup() {
  Serial.begin(115200);
  Serial.println(version);

  if (!SD.begin(CS)) {
    Serial.println("Tarjeta SD no encontrada");
    return;
  }

  randomSeed(analogRead(0));
  reordenarPreguntas();

  pinMode(PIN_SI, INPUT_PULLUP);
  pinMode(PIN_NO, INPUT_PULLUP);
  pinMode(PIN_PICANTE, INPUT_PULLUP);
  
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  servoCuspide.setPeriodHertz(50);
  servoBoca.setPeriodHertz(50);
  servoCuspide.attach(PIN_SERVO_CUSPIDE, 500, 2400);
  servoBoca.attach(PIN_SERVO_BOCA, 500, 2400);

  configurarLED();

  mp3 = new AudioGeneratorMP3();
  salida = new AudioOutputI2SNoDAC();
  fuente = new AudioFileSourceSD();
  salida->SetOutputModeMono(true);

  if (OTAhabilitado)
    initOTA();
  
  reproducirIntroduccion();
}
```

#### Explicación para estudiantes:
Imagina que estás preparando un juego de mesa. Antes de empezar a jugar, necesitas sacar todas las piezas, colocar el tablero y asegurarte de que todo esté listo. Eso es lo que hace la función setup(). Prepara todo lo necesario para que el juego funcione:

- Inicia la comunicación con la computadora (Serial.begin)
- Verifica si la tarjeta de memoria está conectada (SD.begin)
- Prepara los botones que usarán los jugadores (pinMode)
- Configura los "músculos" del sombrero (servomotores)
- Prepara las luces y el sonido
- Y finalmente, reproduce una introducción al juego

#### Prueba de ejemplo:
Si conectaras el ESP32 a tu computadora y abrieras el monitor serial, verías mensajes como:
```
4.03
Tarjeta SD encontrada
Configuración completada
Reproduciendo introducción...
```

### 2. Función loop()

Esta es la función principal del juego que se ejecuta continuamente.

```cpp
void loop() {
  OTAhabilitado ? ArduinoOTA.handle() : yield();
  unsigned long tiempoActual = millis();

  if (mp3->isRunning()) {
    if (!mp3->loop() && yaReprodujo) {
      yaReprodujo = false;
      mp3->stop();
      fuente->close();
      Serial.println("Audio Stop");
      Serial.println("Archivo Cerrado");
      servoCuspide.write(90);
      servoBoca.write(90);
      yield();
    } else {
      moverServoCuspide();
      moverServoBoca();
    }
  } else {
    verificarRespuestaPulsadores();

    if (pulsadorPresionado) {
      Serial.println("Algún pulsador fue presionado");
      pulsadorPresionado = false;
      Serial.println("Puntajes: G:" + String(puntajeGryffindor) + " S:" + String(puntajeSlytherin) + " R:" + String(puntajeRavenclaw) + " H:" + String(puntajeHufflepuff));

      if (modoPicanteActivado) {
        reproducirModoPicante();
        modoPicanteActivado = false;
      } else if (aleatoreaReproducida) {
        if (preguntaActual <= totalPreguntas) {
          Serial.print("Llamado a pregunta: ");
          Serial.println(preguntaActual);
          reproducirPregunta(preguntaActual);
          aleatoreaReproducida = false;
        } else {
          const char* archivoResultado = obtenerResultadoFinal();
          Serial.print("Resultado final: ");
          Serial.println(archivoResultado);
          reproducirAudio(archivoResultado);
          grabarResultadoCSV();
        }
      } else {
        reproducirRespuestaAleatoria();
        aleatoreaReproducida = true;
      }
    }
  }

  if (digitalRead(PIN_PICANTE) == LOW && !modoPicanteActivado) {
    modoPicanteActivado = true;
    pulsadorPresionado = true;
  }
}
```

#### Explicación para estudiantes:
Imagina que eres el árbitro de un juego. Constantemente estás vigilando lo que sucede y reaccionando a las acciones de los jugadores. Eso es lo que hace la función loop():

1. Primero, verifica si se está reproduciendo algún sonido. Si es así, mueve la "boca" y la "cúspide" del sombrero para que parezca que está hablando.

2. Si no se está reproduciendo sonido, revisa si alguien presionó un botón.

3. Si se presionó un botón:
   - Si el "modo picante" está activado, reproduce frases divertidas.
   - Si no, reproduce la siguiente pregunta o una respuesta aleatoria.
   - Si ya se hicieron todas las preguntas, anuncia el resultado final y guarda los puntajes.

4. También está pendiente de si alguien activa el "modo picante".

#### Prueba de ejemplo:
Si un jugador presiona el botón "Sí":
```
Algún pulsador fue presionado
Puntajes: G:10 S:0 R:0 H:0
Llamado a pregunta: 2
Reproduciendo: /q2.mp3
```

### 3. Función reproducirPregunta()

Esta función se encarga de reproducir el audio de una pregunta específica.

```cpp
void reproducirPregunta(int numeroPregunta) {
  Serial.print("Pregunta a reproducir:");
  Serial.println(numeroPregunta);
  char ruta[15];
  snprintf(ruta, sizeof(ruta), "/%s", archivosPreguntas[numeroPregunta]);
  Serial.print("Ruta y archivo a reproducir:");
  Serial.println(ruta);
  reproducirAudio(ruta);
}
```

#### Explicación para estudiantes:
Imagina que tienes un reproductor de música con varias canciones numeradas. Esta función es como decirle al reproductor "reproduce la canción número 3". Pero en lugar de canciones, son preguntas del juego.

1. Primero, imprime en la consola qué número de pregunta va a reproducir.
2. Luego, crea la "dirección" donde está guardada esa pregunta en la tarjeta de memoria.
3. Finalmente, le dice al sistema de audio que reproduzca esa pregunta.

#### Prueba de ejemplo:
Si llamamos `reproducirPregunta(2)`, veríamos en la consola:
```
Pregunta a reproducir:2
Ruta y archivo a reproducir:/q2.mp3
```

### 4. Función verificarRespuestaPulsadores()

Esta función revisa si se presionó algún botón y actualiza los puntajes según la respuesta.

```cpp
void verificarRespuestaPulsadores() {
  unsigned long tiempoActual = millis();

  int lecturaSi = digitalRead(PIN_SI);
  int lecturaNo = digitalRead(PIN_NO);

  if (lecturaSi == LOW && !pulsadorSiPresionado && (tiempoActual - ultimoTiempoSi > debounceDelay)) {
    pulsadorSiPresionado = true;
    pulsadorPresionado = true;
    ultimoTiempoSi = tiempoActual;
    if (respuestasCorrectas[preguntaActual - 1]) {
      switch (casasPorPregunta[preguntaActual - 1]) {
        case 0: puntajeGryffindor += puntosRespuesta; break;
        case 1: puntajeSlytherin += puntosRespuesta; break;
        case 2: puntajeRavenclaw += puntosRespuesta; break;
        case 3: puntajeHufflepuff += puntosRespuesta; break;
      }
    }
    preguntaActual++;
  } else if (lecturaSi == HIGH) {
    pulsadorSiPresionado = false;
  }

  // (Código similar para el botón "No")
}
```

#### Explicación para estudiantes:
Esta función es como un árbitro que está pendiente de las respuestas de los jugadores:

1. Revisa si se presionó el botón "Sí" o "No".
2. Si se presionó un botón, verifica si la respuesta es correcta.
3. Si es correcta, suma puntos a la casa correspondiente.
4. Avanza a la siguiente pregunta.

El "debounce delay" es un truco para evitar que el botón se "rebote" y se cuente más de una vez por accidente.

#### Prueba de ejemplo:
Si un jugador presiona "Sí" y es la respuesta correcta para Gryffindor:
```
Pulsador Sí presionado
Respuesta correcta
Puntos para Gryffindor: 10
Avanzando a la siguiente pregunta
```

### 5. Función obtenerResultadoFinal()

Esta función determina qué casa ganó al final del juego.

```cpp
const char *obtenerResultadoFinal() {
  int puntajesMaximos[4] = {puntajeGryffindor, puntajeSlytherin, puntajeRavenclaw, puntajeHufflepuff};
  int casaGanadora = 0;
  int segundoLugar = 0;
  int contadorEmpates = 0;

  for (int i = 1; i < 4; i++) {
    if (puntajesMaximos[i] > puntajesMaximos[casaGanadora]) {
      segundoLugar = casaGanadora;
      casaGanadora = i;
      contadorEmpates = 0;
    } else if (puntajesMaximos[i] == puntajesMaximos[casaGanadora]) {
      segundoLugar = i;
      contadorEmpates++;
    }
  }

  if (contadorEmpates >= 2) {
    LedPWM(128, 0, 128);
    return "/muggle.mp3";
  } else if (contadorEmpates == 1) {
    casaGanadora = random(2) == 0 ? casaGanadora : segundoLugar;
  }

  switch (casaGanadora) {
    case 0: LedPWM(255, 0, 0); return "/gryffindor.mp3";
    case 1: LedPWM(0, 255, 0); return "/slytherin.mp3";
    case 2: LedPWM(0, 0, 255); return "/ravenclaw.mp3";
    case 3: LedPWM(255, 255, 0); return "/hufflepuff.mp3";
    default: LedPWM(0, 0, 0); return "/error.mp3";
  }
}
```

#### Explicación para estudiantes:
Esta función es como el juez final del concurso:

1. Compara los puntajes de todas las casas.
2. Determina cuál tiene el puntaje más alto.
3. Si hay un empate entre dos casas, elige una al azar.
4. Si hay un empate entre más de dos casas, declara que todos son "muggles" (no magos).
5. Cambia el color del LED según la casa ganadora:
   - Rojo para Gryffindor
   - Verde para Slytherin
   - Azul para Ravenclaw
   - Amarillo para Hufflepuff
6. Devuelve el nombre del archivo de audio que anuncia al ganador.

#### Prueba de ejemplo:
Si Gryffindor gana:
```
Casa ganadora: Gryffindor
LED: Rojo
Reproduciendo: /gryffindor.mp3
```

Estas son las funciones principales del juego. Cada una tiene un papel importante en hacer que el juego funcione de manera fluida y divertida. El código combina elementos de programación, electrónica y lógica de juego para crear una experiencia interactiva basada en el mundo de Harry Potter.

