const char *version = "4.03"; // Versión del programa 
// Se realiza cambio a verificación por pulsadores estándar y se agregan servomotores
// Se modifica el sistema de puntuación para ser más equitativo entre las casas
// Se agrega modo picante y grabación de resultados en CSV

#include "AudioFileSourceSD.h"   // Biblioteca para fuente de audio desde la tarjeta SD
#include "AudioGeneratorMP3.h"   // Biblioteca para generar audio MP3
#include "AudioOutputI2SNoDAC.h" // Biblioteca para salida de audio sin DAC
#include "FS.h"                  // Biblioteca del sistema de archivos
#include "SD.h"                  // Biblioteca para tarjeta SD
#include "SPI.h"                 // Biblioteca para interfaz SPI
#include <WiFi.h>                // Biblioteca para WiFi
#include <ArduinoOTA.h>          // Biblioteca para actualizaciones OTA
#include <ESP32Servo.h>          // Biblioteca para controlar servomotores en ESP32

bool OTAhabilitado = false; // Variable para habilitar/deshabilitar OTA

// Configuración de la red WiFi
const char *ssid = "";     // Nombre de la red WiFi
const char *password = ""; // Contraseña de la red WiFi

// Pines del Bus SPI para la conexión de la Tarjeta SD
#define SCK 18   // Pin de reloj para SPI
#define MISO 19  // Pin de salida de datos para SPI
#define MOSI 23  // Pin de entrada de datos para SPI
#define CS 5     // Pin de selección de chip para SPI

// Pines para los pulsadores (ahora activos en bajo)
#define PIN_SI 4  // GPIO 4 - Pin para "Sí"
#define PIN_NO 15 // GPIO 15 - Pin para "No"
#define PIN_PICANTE 34 // GPIO 34 - Pin para modo picante

// Pines para los servomotores
#define PIN_SERVO_CUSPIDE 13 // Pin para el servomotor de la cúspide
#define PIN_SERVO_BOCA 14    // Pin para el servomotor de la boca

// Pines para LED RGB
#define PIN_LED_ROJO 25    // Pin GPIO para el LED rojo
#define PIN_LED_VERDE 26   // Pin GPIO para el LED verde
#define PIN_LED_AZUL 27    // Pin GPIO para el LED azul

#define CANAL_LEDC_0 0     // Canal LEDC para el LED rojo
#define CANAL_LEDC_1 1     // Canal LEDC para el LED verde
#define CANAL_LEDC_2 2     // Canal LEDC para el LED azul

#define LEDC_TIMER_8_BIT 8 // Usar timer de 8 bits para PWM
#define FRECUENCIA_BASE_LEDC 5000 // Frecuencia base para PWM en Hz

// Objetos Servo
Servo servoCuspide; // Servo para la cúspide del sombrero
Servo servoBoca;    // Servo para la boca del sombrero

bool pulsadorPresionado = false; // Bandera para verificar si se presionó algún pulsador

// Variables para el estado de los pulsadores y manejo del debounce
bool pulsadorSiPresionado = false; // Variable para el estado del pulsador "Sí"
bool pulsadorNoPresionado = false; // Variable para el estado del pulsador "No"
bool pulsadorPicantePresionado = false; // Variable para el estado del pulsador "Picante"
unsigned long ultimoTiempoSi = 0;  // Tiempo de la última lectura del pulsador "Sí"
unsigned long ultimoTiempoNo = 0;  // Tiempo de la última lectura del pulsador "No"
unsigned long ultimoTiempoPicante = 0; // Tiempo de la última lectura del pulsador "Picante"
const unsigned long debounceDelay = 120; // Tiempo de espera para el debounce en milisegundos

#define PWM_PIN 12 // GPIO 12 - Pin para la salida PWM

// Variables para el manejo del audio
AudioGeneratorMP3 *mp3;       // Generador de audio MP3
AudioFileSourceSD *fuente;    // Fuente de archivo de audio desde la tarjeta SD
AudioOutputI2SNoDAC *salida;  // Salida de audio sin DAC
bool yaReprodujo = false;     // Bandera para verificar si ya se reprodujo un audio
bool aleatoreaReproducida = false; // Bandera para verificar si se reprodujo una respuesta aleatoria

// Variables para el manejo de la lógica del cuestionario
int preguntaActual = 1;                                                                                               // Índice de la pregunta actual
const int totalPreguntas = 8;                                                                                         // Número total de preguntas
const int puntosRespuesta = 10;                                                                                       // Puntuación fija para cada pregunta
const char *archivosPreguntas[totalPreguntas] = {"q1.mp3", "q2.mp3", "q3.mp3", "q4.mp3", "q5.mp3", "q6.mp3", "q7.mp3", "q8.mp3"}; // Archivos de audio para las preguntas
unsigned long ultimoUso = 0;                                                                                          // Variable para rastrear el último uso

// Variables para el control de los servomotores
unsigned long ultimoMovimientoCuspide = 0; // Último tiempo de movimiento de la cúspide
unsigned long ultimoMovimientoBoca = 0;    // Último tiempo de movimiento de la boca
const int intervaloMovimientoCuspide = 500; // Intervalo para el movimiento de la cúspide (en ms)
const int intervaloMovimientoBoca = 100;    // Intervalo para el movimiento de la boca (en ms)

// Variables para el puntaje de cada casa
int puntajeGryffindor = 0; // Puntaje para Gryffindor
int puntajeSlytherin = 0; // Puntaje para Slytherin
int puntajeRavenclaw = 0; // Puntaje para Ravenclaw
int puntajeHufflepuff = 0; // Puntaje para Hufflepuff

// Arreglo de respuestas correctas (true para Sí, false para No)
const bool respuestasCorrectas[totalPreguntas] = {true, false, true, false, true, false, true, false}; // Respuestas correctas para cada pregunta
/*Este arreglo tiene 8 elementos, uno para cada pregunta del juego (recordemos que totalPreguntas = 8).

Ahora, veamos algunos ejemplos de cómo funciona esto en la práctica:

Ejemplo 1:

Pregunta 1: "¿Te gusta la magia?"
Respuesta correcta: true (Sí)
Si el jugador presiona el botón "Sí", obtendrá puntos.
Si el jugador presiona el botón "No", no obtendrá puntos.
Ejemplo 2:

Pregunta 2: "¿Prefieres estar solo?"
Respuesta correcta: false (No)
Si el jugador presiona el botón "No", obtendrá puntos.
Si el jugador presiona el botón "Sí", no obtendrá puntos.
Ejemplo 3:

Pregunta 3: "¿Te gustan los desafíos?"
Respuesta correcta: true (Sí)
Si el jugador presiona el botón "Sí", obtendrá puntos.
Si el jugador presiona el botón "No", no obtendrá puntos.
Ejemplo 4:

Pregunta 4: "¿Temes a la oscuridad?"
Respuesta correcta: false (No)
Si el jugador presiona el botón "No", obtendrá puntos.
Si el jugador presiona el botón "Sí", no obtendrá puntos.
En el código, cuando el jugador responde a una pregunta, se compara su respuesta con la respuesta correcta almacenada en respuestasCorrectas. Esto se hace en la función verificarRespuestaPulsadores():
*/
// Arreglo de casas correspondientes a cada pregunta
const int casasPorPregunta[totalPreguntas] = {0, 1, 2, 3, 0, 1, 2, 3}; // 0: Gryffindor, 1: Slytherin, 2: Ravenclaw, 3: Hufflepuff

// Arreglos para almacenar el orden aleatorio
char* archivosPreguntas_aleatorio[totalPreguntas];
bool respuestasCorrectas_aleatorio[totalPreguntas];
int casasPorPregunta_aleatorio[totalPreguntas];

// Variables para el modo picante
bool modoPicanteActivado = false; // Bandera para indicar si el modo picante está activado
const int totalArchivosPicantes = 10; // Número total de archivos picantes disponibles
const int archivosPicantesPorSesion = 3; // Número de archivos picantes a reproducir por sesión

/**
 * @brief Configura los componentes iniciales del sistema.
 * 
 * Esta función inicializa la comunicación serial, configura la tarjeta SD,
 * los pines de entrada/salida, los servomotores, el audio y, si está habilitado, 
 * las actualizaciones OTA.
 */
void setup() {
  Serial.begin(115200); // Inicia la comunicación serial
  Serial.println(version); // Imprime la versión del programa

  // Intenta iniciar la tarjeta SD
  if (!SD.begin(CS)) {
    Serial.println("Tarjeta SD no encontrada");
    return;
  }

  randomSeed(analogRead(0)); // Inicializa la semilla aleatoria
  reordenarPreguntas(); // Reordena las preguntas al inicio

  // Configura los pines de los pulsadores como entradas con pull-up
  pinMode(PIN_SI, INPUT_PULLUP);
  pinMode(PIN_NO, INPUT_PULLUP);
  pinMode(PIN_PICANTE, INPUT_PULLUP); // Configura el pin del modo picante
  
  // Configura los timers para los servomotores

  ESP32PWM::allocateTimer(0); // Asignar el temporizador 0 para el control de PWM
  ESP32PWM::allocateTimer(1); // Asignar el temporizador 1 para el control de PWM
  servoCuspide.setPeriodHertz(50); // Establecer la frecuencia del servo en 50 Hz para el servoCuspide
  servoBoca.setPeriodHertz(50); // Establecer la frecuencia del servo en 50 Hz para el servoBoca
  servoCuspide.attach(PIN_SERVO_CUSPIDE, 500, 2400); // Conectar el servoCuspide al pin definido, con un rango de pulso de 500 a 2400 microsegundos
  servoBoca.attach(PIN_SERVO_BOCA, 500, 2400); // Conectar el servoBoca al pin definido, con un rango de pulso de 500 a 2400 microsegundos

  configurarLED(); // Llama a la función para configurar el LED

  // Inicializa los componentes de audio
  mp3 = new AudioGeneratorMP3(); // Crea una nueva instancia del generador de audio MP3
  salida = new AudioOutputI2SNoDAC(); // Crea una nueva instancia de salida de audio I2S sin DAC
  fuente = new AudioFileSourceSD(); // Crea una nueva fuente de archivo de audio desde la tarjeta SD
  salida->SetOutputModeMono(true); // Configura la salida de audio en modo mono

  // Si OTA está habilitado, inicia la configuración OTA
  if (OTAhabilitado)
    initOTA();
  
  reproducirIntroduccion(); // Reproduce la introducción del juego
}

/**
 * @brief Función principal que se ejecuta continuamente.
 * 
 * Esta función maneja la lógica principal del juego, incluyendo la reproducción de audio,
 * el movimiento de los servomotores, la verificación de las respuestas y el avance del juego.
 */
void loop() {
  // Maneja OTA si está habilitado, de lo contrario cede el control
  OTAhabilitado ? ArduinoOTA.handle() : yield(); 
  
  // Obtiene el tiempo actual en milisegundos desde que comenzó el programa
  unsigned long tiempoActual = millis(); 

  // Verifica si el reproductor de MP3 está en funcionamiento
  if (mp3->isRunning()) {
    // Si el MP3 no está en bucle y ya se reprodujo, detiene la reproducción
    if (!mp3->loop() && yaReprodujo) {
      yaReprodujo = false; // Reinicia la bandera de reproducción
      mp3->stop(); // Detiene la reproducción de audio
      fuente->close(); // Cierra la fuente de audio
      Serial.println("Audio Stop"); // Imprime en el monitor serie que el audio se detuvo
      Serial.println("Archivo Cerrado"); // Imprime que el archivo de audio se cerró
      servoCuspide.write(90); // Posición neutral para el servo de la cúspide
      servoBoca.write(90); // Posición neutral para el servo de la boca
      yield(); // Cede el control para permitir otras tareas
    } else {
      moverServoCuspide(); // Mueve el servo de la cúspide
      moverServoBoca(); // Mueve el servo de la boca
    }
  } else {
    // Verifica si se presionó algún pulsador
    verificarRespuestaPulsadores(); 

    // Si algún pulsador fue presionado
    if (pulsadorPresionado) {
      Serial.println("Algún pulsador fue presionado"); // Imprime que un pulsador fue activado
      pulsadorPresionado = false; // Reinicia la bandera de pulsador presionado
      // Imprime los puntajes de las casas
      Serial.println("Puntajes: G:" + String(puntajeGryffindor) + " S:" + String(puntajeSlytherin) + " R:" + String(puntajeRavenclaw) + " H:" + String(puntajeHufflepuff));

      // Si el modo picante está activado
      if (modoPicanteActivado) {
        reproducirModoPicante(); // Reproduce el modo picante
        modoPicanteActivado = false; // Desactiva el modo picante después de reproducirlo
      } else if (aleatoreaReproducida) {
        // Si hay preguntas restantes
        if (preguntaActual <= totalPreguntas) {
          Serial.print("Llamado a pregunta: "); // Imprime el número de la pregunta actual
          Serial.println(preguntaActual);
          reproducirPregunta(preguntaActual); // Reproduce la pregunta actual
          aleatoreaReproducida = false; // Reinicia la bandera de reproducción aleatoria
        } else {
          const char* archivoResultado = obtenerResultadoFinal(); // Obtiene el resultado final
          Serial.print("Resultado final: "); // Imprime el resultado final
          Serial.println(archivoResultado);
          reproducirAudio(archivoResultado); // Reproduce el audio del resultado final
          grabarResultadoCSV(); // Graba el resultado en el archivo CSV
        }
      } else {
        reproducirRespuestaAleatoria(); // Reproduce una respuesta aleatoria
        aleatoreaReproducida = true; // Marca que se ha reproducido una respuesta aleatoria
      }
    }
  }

  // Verifica si se presionó el botón de modo picante
  if (digitalRead(PIN_PICANTE) == LOW && !modoPicanteActivado) {
    modoPicanteActivado = true;
    pulsadorPresionado = true;
  }
}
//-------------------------------------------------------------------------------------------Reproducciones-----------
void reproducirIntroduccion() {
  const char *archivoIntroduccion = "/intro.mp3"; // Ruta del archivo de introducción
  reproducirAudio(archivoIntroduccion); // Llama a la función de reproducción de audio genérica
  while (mp3->isRunning()) {
    if (!mp3->loop() && yaReprodujo) { // Si el audio ha terminado de reproducirse
      yaReprodujo = false; // Reinicia la bandera de reproducción
      mp3->stop();         // Detiene la reproducción
      fuente->close();     // Cierra el archivo de audio
      Serial.println("Audio Stop"); // Mensaje de parada de audio
      Serial.println("Archivo Cerrado"); // Mensaje de archivo cerrado
      yield(); // Pasa el control a otras tareas
    }
  }
  aleatoreaReproducida = true; // Marca que se reprodujo una respuesta aleatoria
  return;
  }
void reproducirRespuestaAleatoria() {
  char ruta[15];
  int numeroRespuesta = random(1, 22); // Genera un número aleatorio entre 1 y 22
  snprintf(ruta, sizeof(ruta), "/resp%d.mp3", numeroRespuesta); // Formatea la ruta del archivo de audio de la respuesta aleatoria
  Serial.print("Aleatorea: ");
  Serial.println(ruta);
  aleatoreaReproducida = true; // Marca que se reprodujo una respuesta aleatoria
  reproducirAudio(ruta); // Llama a la función de reproducción de audio genérica
  pulsadorPresionado = true;
}

void reproducirPregunta(int numeroPregunta) {
  Serial.print("Pregunta a reproducir:"); // Imprime un mensaje en el monitor serial
  Serial.println(numeroPregunta); // Imprime el número de la pregunta a reproducir
  char ruta[15]; // Declara un arreglo de caracteres para almacenar la ruta del archivo
  snprintf(ruta, sizeof(ruta), "/%s", archivosPreguntas[numeroPregunta]); // Formatea la ruta del archivo de audio de la pregunta
  Serial.print("Ruta y archivo a reproducir:"); // Imprime un mensaje en el monitor serial sobre la ruta
  Serial.println(ruta); // Imprime la ruta del archivo que se va a reproducir
  reproducirAudio(ruta); // Llama a la función de reproducción de audio genérica con la ruta especificada
}
void verificarRespuestaPulsadores() {
  unsigned long tiempoActual = millis(); // Obtiene el tiempo actual

  int lecturaSi = digitalRead(PIN_SI); // Lee el estado del pulsador "Sí"
  int lecturaNo = digitalRead(PIN_NO); // Lee el estado del pulsador "No"

  // Maneja el pulsador "Sí"
  if (lecturaSi == LOW && !pulsadorSiPresionado && (tiempoActual - ultimoTiempoSi > debounceDelay)) {
    pulsadorSiPresionado = true;
    pulsadorPresionado = true;
    ultimoTiempoSi = tiempoActual;
    if (respuestasCorrectas[preguntaActual - 1]) { // Si la respuesta es correcta
      switch (casasPorPregunta[preguntaActual - 1]) {
        case 0: puntajeGryffindor += puntosRespuesta; break; // Suma puntos a Gryffindor
        case 1: puntajeSlytherin += puntosRespuesta; break; // Suma puntos a Slytherin
        case 2: puntajeRavenclaw += puntosRespuesta; break; // Suma puntos a Ravenclaw
        case 3: puntajeHufflepuff += puntosRespuesta; break; // Suma puntos a Hufflepuff
      }
    }
    preguntaActual++; // Avanza a la siguiente pregunta
  } else if (lecturaSi == HIGH) {
    pulsadorSiPresionado = false;
  }

  // Maneja el pulsador "No"
  if (lecturaNo == LOW && !pulsadorNoPresionado && (tiempoActual - ultimoTiempoNo > debounceDelay)) {
    pulsadorNoPresionado = true;
    pulsadorPresionado = true;
    ultimoTiempoNo = tiempoActual;
    if (!respuestasCorrectas[preguntaActual - 1]) { // Si la respuesta es correcta
      switch (casasPorPregunta[preguntaActual - 1]) {
        case 0: puntajeGryffindor += puntosRespuesta; break; // Suma puntos a Gryffindor
        case 1: puntajeSlytherin += puntosRespuesta; break; // Suma puntos a Slytherin
        case 2: puntajeRavenclaw += puntosRespuesta; break; // Suma puntos a Ravenclaw
        case 3: puntajeHufflepuff += puntosRespuesta; break; // Suma puntos a Hufflepuff
      }
    }
    preguntaActual++; // Avanza a la siguiente pregunta
  } else if (lecturaNo == HIGH) {
    pulsadorNoPresionado = false;
  }
}
void reproducirAudio(const char *ruta) {
  if (!SD.exists(ruta)) { // Verifica si el archivo existe en la tarjeta SD
    Serial.println("Archivo no encontrado"); // Mensaje de error si el archivo no existe
    return; // Termina la función si el archivo no existe
  }

  if (!fuente->open(ruta)) { // Abre el archivo de audio
    Serial.println("Error al abrir el archivo"); // Mensaje de error si no se puede abrir el archivo
    return; // Termina la función si no se puede abrir el archivo
  }

  yield(); // Pasa el control a otras tareas
  mp3->begin(fuente, salida); // Inicia la reproducción del audio
  yaReprodujo = true; // Marca que se está reproduciendo un audio
  
  // Mueve los servomotores mientras se reproduce el audio
       moverServoCuspide();
      moverServoBoca();
}
/**
 * @brief Reproduce el modo picante.
 * 
 * Esta función selecciona aleatoriamente 3 archivos de audio picantes
 * y los reproduce en secuencia.
 */
void reproducirModoPicante() {
  // Inicia un bucle que se repetirá para cada archivo picante en la sesión
  for (int i = 0; i < archivosPicantesPorSesion; i++) {
    // Genera un número aleatorio entre 1 y totalArchivosPicantes
    int numeroArchivo = random(1, totalArchivosPicantes + 1); 
    char ruta[20]; // Declara un arreglo de caracteres para almacenar la ruta del archivo
    // Formatea la ruta del archivo de audio picante usando el número aleatorio generado
    snprintf(ruta, sizeof(ruta), "/picante%d.mp3", numeroArchivo);
    // Llama a la función para reproducir el audio en la ruta especificada
    reproducirAudio(ruta);
    // Mientras el reproductor de mp3 esté en funcionamiento
    while (mp3->isRunning()) {
      // Intenta hacer un bucle en la reproducción del audio
      if (!mp3->loop()) break; // Si no se puede hacer el bucle, sale del bucle
    }
  }
}

/**
 * @brief Reordena aleatoriamente las preguntas, respuestas y casas
 * En este código, la función reordenarPreguntas se encarga de mezclar las preguntas, respuestas y casas de manera aleatoria. Cada sección del código tiene comentarios que explican el propósito de cada línea.
 * Iteración: Se recorre cada pregunta desde 0 hasta el total de preguntas.
 * Índice Aleatorio: Se genera un índice aleatorio j que se utiliza para intercambiar elementos.
 * Intercambio de Archivos de Preguntas: Se intercambian los archivos de preguntas entre la posición actual y la posición aleatoria.
 * Intercambio de Respuestas Correctas: Se realiza un intercambio similar para las respuestas correctas.
 * Intercambio de Casas: Finalmente, se intercambian las casas asociadas a cada pregunta.
 * Cada comentario proporciona claridad sobre lo que está sucediendo en el código, facilitando su comprensión y mantenimiento.
 */
void reordenarPreguntas() {
  // Iterar a través de todas las preguntas
  for (int i = 0; i < totalPreguntas; i++) {
    // Generar un índice aleatorio entre i y totalPreguntas
    int j = random(i, totalPreguntas);
    
    // Intercambiar archivos de preguntas
    char* temp = archivosPreguntas_aleatorio[i]; // Guardar temporalmente el archivo de la pregunta actual
    archivosPreguntas_aleatorio[i] = archivosPreguntas_aleatorio[j]; // Asignar el archivo de la pregunta aleatoria a la posición actual
    archivosPreguntas_aleatorio[j] = temp; // Colocar el archivo guardado en la posición aleatoria
    
    // Intercambiar respuestas correctas
    bool tempBool = respuestasCorrectas_aleatorio[i]; // Guardar temporalmente la respuesta correcta de la pregunta actual
    respuestasCorrectas_aleatorio[i] = respuestasCorrectas_aleatorio[j]; // Asignar la respuesta correcta de la pregunta aleatoria a la posición actual
    respuestasCorrectas_aleatorio[j] = tempBool; // Colocar la respuesta guardada en la posición aleatoria
    
    // Intercambiar casas por pregunta
    int tempInt = casasPorPregunta_aleatorio[i]; // Guardar temporalmente la casa de la pregunta actual
    casasPorPregunta_aleatorio[i] = casasPorPregunta_aleatorio[j]; // Asignar la casa de la pregunta aleatoria a la posición actual
    casasPorPregunta_aleatorio[j] = tempInt; // Colocar la casa guardada en la posición aleatoria
  }
}

const char *obtenerResultadoFinal() {
  // Arreglo con los puntajes de todas las casas
  int puntajesMaximos[4] = {puntajeGryffindor, puntajeSlytherin, puntajeRavenclaw, puntajeHufflepuff}; 
  // Índice de la casa ganadora
  int casaGanadora = 0; 
  // Índice de la casa en segundo lugar
  int segundoLugar = 0; 
  // Contador de empates
  int contadorEmpates = 0; 

  // Encontrar la casa con mayor puntaje y contar empates
  for (int i = 1; i < 4; i++) {
    // Si el puntaje actual es mayor que el de la casa ganadora
    if (puntajesMaximos[i] > puntajesMaximos[casaGanadora]) {
      segundoLugar = casaGanadora; // Actualizar segundo lugar
      casaGanadora = i; // Actualizar casa ganadora
      contadorEmpates = 0; // Reiniciar contador de empates
    } 
    // Si hay un empate con la casa ganadora
    else if (puntajesMaximos[i] == puntajesMaximos[casaGanadora]) {
      segundoLugar = i; // Actualizar segundo lugar al actual
      contadorEmpates++; // Incrementar contador de empates
    }
  }

  // Manejar empates
  if (contadorEmpates >= 2) {
    LedPWM(128, 0, 128); // Color púrpura para Muggles
    return "/muggle.mp3"; // Archivo de audio para Muggles
  } 
  // Si hay un empate entre dos casas
  else if (contadorEmpates == 1) {
    // Elegir aleatoriamente entre las dos casas empatadas
    casaGanadora = random(2) == 0 ? casaGanadora : segundoLugar; 
  }

  // Asignar color y archivo de audio según la casa ganadora
  switch (casaGanadora) {
    case 0:
      LedPWM(255, 0, 0); // Rojo para Gryffindor
      return "/gryffindor.mp3"; // Retornar archivo de audio de Gryffindor
    case 1:
      LedPWM(0, 255, 0); // Verde para Slytherin
      return "/slytherin.mp3"; // Retornar archivo de audio de Slytherin
    case 2:
      LedPWM(0, 0, 255); // Azul para Ravenclaw
      return "/ravenclaw.mp3"; // Retornar archivo de audio de Ravenclaw
    case 3:
      LedPWM(255, 255, 0); // Amarillo para Hufflepuff
      return "/hufflepuff.mp3"; // Retornar archivo de audio de Hufflepuff
    default:
      LedPWM(0, 0, 0); // Apagar LED en caso de error
      return "/error.mp3"; // Retornar archivo de audio de error
  }
}


void configurarLED() {
  // Configurar los canales LEDC con la frecuencia y resolución especificadas
  ledcSetup(CANAL_LEDC_0, FRECUENCIA_BASE_LEDC, LEDC_TIMER_8_BIT); // Configura el canal 0
  ledcSetup(CANAL_LEDC_1, FRECUENCIA_BASE_LEDC, LEDC_TIMER_8_BIT); // Configura el canal 1
  ledcSetup(CANAL_LEDC_2, FRECUENCIA_BASE_LEDC, LEDC_TIMER_8_BIT); // Configura el canal 2
  
  // Asociar los pines GPIO con los canales LEDC para controlar los LEDs
  ledcAttachPin(PIN_LED_ROJO, CANAL_LEDC_0); // Asocia el pin del LED rojo al canal 0
  ledcAttachPin(PIN_LED_VERDE, CANAL_LEDC_1); // Asocia el pin del LED verde al canal 1
  ledcAttachPin(PIN_LED_AZUL, CANAL_LEDC_2); // Asocia el pin del LED azul al canal 2
}

/**
 * @brief Controla el color de un LED RGB utilizando PWM por hardware.
 * 
 * @param rojo Valor de intensidad para el color rojo (0-255).
 * @param verde Valor de intensidad para el color verde (0-255).
 * @param azul Valor de intensidad para el color azul (0-255).
 * 
 * @details
 * Esta función utiliza el controlador LEDC del ESP32 para generar señales PWM
 * eficientes por hardware. Cada componente de color (rojo, verde, azul) se
 * controla independientemente utilizando un canal LEDC dedicado.
 * 
 * Ventajas de esta implementación:
 * 1. Mayor eficiencia: Utiliza PWM por hardware, lo que reduce la carga de la CPU.
 * 2. Mayor precisión: El control de tiempo por hardware es más preciso que la
 *    emulación por software.
 * 3. Sin parpadeo: La alta frecuencia de PWM (5000 Hz) evita el parpadeo visible.
 * 4. Flexibilidad: Permite ajustar fácilmente la frecuencia PWM si es necesario.
 * 
 * Esta implementación es específica para ESP32 y aprovecha sus capacidades de
 * hardware para un control de LED más eficiente y preciso.
 */
void LedPWM(int rojo, int verde, int azul) {
  // Escribir los valores PWM en los canales LEDC correspondientes
  ledcWrite(CANAL_LEDC_0, rojo);
  ledcWrite(CANAL_LEDC_1, verde);
  ledcWrite(CANAL_LEDC_2, azul);
}
/**
 * @brief Inicializa la configuración OTA
 */
void initOTA() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Conexión fallida! Reiniciando...");
    delay(5000);
    ESP.restart();
  }

  ArduinoOTA.setHostname("ESP32-Sombrero");
  ArduinoOTA.begin();
}
/**
 * @brief Mueve el servo de la cúspide
 */
void moverServoCuspide() {
  unsigned long tiempoActual = millis(); // Captura el tiempo actual en milisegundos
  // Verifica si ha pasado el intervalo necesario desde el último movimiento
  if (tiempoActual - ultimoMovimientoCuspide >= intervaloMovimientoCuspide) {
    int posicion = random(70, 111); // Genera una posición aleatoria entre 70 y 110 grados
    servoCuspide.write(posicion); // Mueve el servo a la posición generada
    ultimoMovimientoCuspide = tiempoActual; // Actualiza el tiempo del último movimiento
  }
}

/**
 * @brief Mueve el servo de la boca
 */
void moverServoBoca() {
  unsigned long tiempoActual = millis(); // Captura el tiempo actual en milisegundos
  // Verifica si ha pasado el intervalo necesario desde el último movimiento
  if (tiempoActual - ultimoMovimientoBoca >= intervaloMovimientoBoca) {
    int posicion = random(80, 101); // Genera una posición aleatoria entre 80 y 100 grados
    servoBoca.write(posicion); // Mueve el servo a la posición generada
    ultimoMovimientoBoca = tiempoActual; // Actualiza el tiempo del último movimiento
  }
}


/**
 * @brief Graba el resultado del juego en un archivo CSV.
 * 
 * Esta función crea o actualiza un archivo CSV en la tarjeta SD
 * con los puntajes finales de cada casa.
 */
void grabarResultadoCSV() {
  const char* nombreArchivo = "/resultados.csv"; // Definimos el nombre del archivo CSV
  File archivo = SD.open(nombreArchivo, FILE_READ); // Intentamos abrir el archivo en modo lectura
  bool archivoExiste = archivo; // Verificamos si el archivo se abrió correctamente
  archivo.close(); // Cerramos el archivo después de la verificación

  if (!archivoExiste) {
    // Si el archivo no existe, lo creamos y añadimos el encabezado
    archivo = SD.open(nombreArchivo, FILE_WRITE); // Abrimos el archivo en modo escritura
    if (archivo) {
      archivo.println("Gryffindor,Slytherin,Ravenclaw,Hufflepuff"); // Escribimos el encabezado en el archivo
      archivo.close(); // Cerramos el archivo después de escribir
      Serial.println("Archivo CSV creado con encabezado"); // Mensaje de confirmación en el monitor serial
    } else {
      Serial.println("Error al crear el archivo CSV"); // Mensaje de error si no se pudo crear el archivo
      return; // Salimos de la función si hubo un error
    }
  }

  // Abrimos el archivo en modo de añadir
  archivo = SD.open(nombreArchivo, FILE_APPEND); // Abrimos el archivo en modo añadir
  if (archivo) {
    // Obtenemos la fecha y hora actual (asumiendo que tienes un RTC configurado)
   // DateTime now = rtc.now(); // Obtenemos la fecha y hora actual del RTC

    // Escribimos los datos
    archivo.print(puntajeGryffindor); // Escribimos el puntaje de Gryffindor
    archivo.print(","); // Añadimos una coma como separador
    archivo.print(puntajeSlytherin); // Escribimos el puntaje de Slytherin
    archivo.print(","); // Añadimos una coma como separador
    archivo.print(puntajeRavenclaw); // Escribimos el puntaje de Ravenclaw
    archivo.print(","); // Añadimos una coma como separador
    archivo.print(puntajeHufflepuff); // Escribimos el puntaje de Hufflepuff
    archivo.print(","); // Añadimos una coma como separador
    /*
    // Añadimos la fecha y hora
    archivo.print(now.year()); // Escribimos el año actual
    archivo.print("/"); // Añadimos una barra como separador
    archivo.print(now.month()); // Escribimos el mes actual
    archivo.print("/"); // Añadimos una barra como separador
    archivo.print(now.day()); // Escribimos el día actual
    archivo.print(","); // Añadimos una coma como separador
    archivo.print(now.hour()); // Escribimos la hora actual
    archivo.print(":"); // Añadimos dos puntos como separador
    archivo.print(now.minute()); // Escribimos los minutos actuales
    archivo.print(":"); // Añadimos dos puntos como separador
    archivo.println(now.second()); // Escribimos los segundos actuales y añadimos una nueva línea
*/
    archivo.close(); // Cerramos el archivo después de escribir
    Serial.println("Resultado grabado en CSV"); // Mensaje de confirmación en el monitor serial
  } else {
    Serial.println("Error al abrir el archivo CSV para escribir"); // Mensaje de error si no se pudo abrir el archivo
  }
}
