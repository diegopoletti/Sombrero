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
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  servoCuspide.setPeriodHertz(50);
  servoBoca.setPeriodHertz(50);
  servoCuspide.attach(PIN_SERVO_CUSPIDE, 500, 2400);
  servoBoca.attach(PIN_SERVO_BOCA, 500, 2400);

  configurarLED();

  // Inicializa los componentes de audio
  mp3 = new AudioGeneratorMP3();
  salida = new AudioOutputI2SNoDAC();
  fuente = new AudioFileSourceSD();
  salida->SetOutputModeMono(true);

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
  OTAhabilitado ? ArduinoOTA.handle() : yield(); // Maneja OTA si está habilitado, de lo contrario cede el control
  unsigned long tiempoActual = millis(); // Obtiene el tiempo actual

  if (mp3->isRunning()) {
    if (!mp3->loop() && yaReprodujo) {
      yaReprodujo = false;
      mp3->stop();
      fuente->close();
      Serial.println("Audio Stop");
      Serial.println("Archivo Cerrado");
      servoCuspide.write(90); // Posición neutral para el servo de la cúspide
      servoBoca.write(90); // Posición neutral para el servo de la boca
      yield();
    } else {
      moverServoCuspide(); // Mueve el servo de la cúspide
      moverServoBoca(); // Mueve el servo de la boca
    }
  } else {
    verificarRespuestaPulsadores(); // Verifica si se presionó algún pulsador

    if (pulsadorPresionado) {
      Serial.println("Algún pulsador fue presionado");
      pulsadorPresionado = false;
      Serial.println("Puntajes: G:" + String(puntajeGryffindor) + " S:" + String(puntajeSlytherin) + " R:" + String(puntajeRavenclaw) + " H:" + String(puntajeHufflepuff));

      if (modoPicanteActivado) {
        reproducirModoPicante(); // Reproduce el modo picante
        modoPicanteActivado = false; // Desactiva el modo picante después de reproducirlo
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
          grabarResultadoCSV(); // Graba el resultado en el archivo CSV
        }
      } else {
        reproducirRespuestaAleatoria();
        aleatoreaReproducida = true;
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
void reproducirPregunta(int numeroPregunta) {
  Serial.print("Pregunta a reproducir:");
  Serial.println(numeroPregunta);
  char ruta[15];
  snprintf(ruta, sizeof(ruta), "/%s", archivosPreguntas[numeroPregunta]); // Formatea la ruta del archivo de audio de la pregunta
  Serial.print("Ruta y archivo a reproducir:");
  Serial.println(ruta);
  reproducirAudio(ruta); // Llama a la función de reproducción de audio genérica
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
  for (int i = 0; i < archivosPicantesPorSesion; i++) {
    int numeroArchivo = random(1, totalArchivosPicantes + 1); // Genera un número aleatorio entre 1 y totalArchivosPicantes
    char ruta[20];
    snprintf(ruta, sizeof(ruta), "/picante%d.mp3", numeroArchivo);
    reproducirAudio(ruta);
    while (mp3->isRunning()) {
      if (!mp3->loop()) break;
    }
  }
}
/**
 * @brief Reordena aleatoriamente las preguntas, respuestas y casas
 */
void reordenarPreguntas() {
  for (int i = 0; i < totalPreguntas; i++) {
    int j = random(i, totalPreguntas);
    
    // Intercambiar archivos de preguntas
    char* temp = archivosPreguntas_aleatorio[i];
    archivosPreguntas_aleatorio[i] = archivosPreguntas_aleatorio[j];
    archivosPreguntas_aleatorio[j] = temp;
    
    // Intercambiar respuestas correctas
    bool tempBool = respuestasCorrectas_aleatorio[i];
    respuestasCorrectas_aleatorio[i] = respuestasCorrectas_aleatorio[j];
    respuestasCorrectas_aleatorio[j] = tempBool;
    
    // Intercambiar casas por pregunta
    int tempInt = casasPorPregunta_aleatorio[i];
    casasPorPregunta_aleatorio[i] = casasPorPregunta_aleatorio[j];
    casasPorPregunta_aleatorio[j] = tempInt;
  }
}
const char *obtenerResultadoFinal() {
  int puntajesMaximos[4] = {puntajeGryffindor, puntajeSlytherin, puntajeRavenclaw, puntajeHufflepuff}; // Arreglo con los puntajes de todas las casas
  int casaGanadora = 0; // Índice de la casa ganadora
  int segundoLugar = 0; // Índice de la casa en segundo lugar
  int contadorEmpates = 0; // Contador de empates

  // Encontrar la casa con mayor puntaje y contar empates
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

  // Manejar empates
  if (contadorEmpates >= 2) {
    LedPWM(128, 0, 128); // Color púrpura para Muggles
    return "/muggle.mp3"; // Archivo de audio para Muggles
  } else if (contadorEmpates == 1) {
    casaGanadora = random(2) == 0 ? casaGanadora : segundoLugar; // Elegir aleatoriamente entre las dos casas empatadas
  }

  // Asignar color y archivo de audio según la casa ganadora
  switch (casaGanadora) {
    case 0:
      LedPWM(255, 0, 0); // Rojo para Gryffindor
      return "/gryffindor.mp3";
    case 1:
      LedPWM(0, 255, 0); // Verde para Slytherin
      return "/slytherin.mp3";
    case 2:
      LedPWM(0, 0, 255); // Azul para Ravenclaw
      return "/ravenclaw.mp3";
    case 3:
      LedPWM(255, 255, 0); // Amarillo para Hufflepuff
      return "/hufflepuff.mp3";
    default:
      LedPWM(0, 0, 0); // Apagar LED en caso de error
      return "/error.mp3";
  }
}


void configurarLED() {
  // Configurar los canales LEDC
  ledcSetup(CANAL_LEDC_0, FRECUENCIA_BASE_LEDC, LEDC_TIMER_8_BIT);
  ledcSetup(CANAL_LEDC_1, FRECUENCIA_BASE_LEDC, LEDC_TIMER_8_BIT);
  ledcSetup(CANAL_LEDC_2, FRECUENCIA_BASE_LEDC, LEDC_TIMER_8_BIT);
  
  // Asociar los pines GPIO con los canales LEDC
  ledcAttachPin(PIN_LED_ROJO, CANAL_LEDC_0);
  ledcAttachPin(PIN_LED_VERDE, CANAL_LEDC_1);
  ledcAttachPin(PIN_LED_AZUL, CANAL_LEDC_2);
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
  unsigned long tiempoActual = millis();
  if (tiempoActual - ultimoMovimientoCuspide >= intervaloMovimientoCuspide) {
    int posicion = random(70, 111); // Genera una posición aleatoria entre 70 y 110 grados
    servoCuspide.write(posicion);
    ultimoMovimientoCuspide = tiempoActual;
  }
}
/**
 * @brief Mueve el servo de la boca
 */
void moverServoBoca() {
  unsigned long tiempoActual = millis();
  if (tiempoActual - ultimoMovimientoBoca >= intervaloMovimientoBoca) {
    int posicion = random(80, 101); // Genera una posición aleatoria entre 80 y 100 grados
    servoBoca.write(posicion);
    ultimoMovimientoBoca = tiempoActual;
  }
}

/**
 * @brief Graba el resultado del juego en un archivo CSV.
 * 
 * Esta función crea o actualiza un archivo CSV en la tarjeta SD
 * con los puntajes finales de cada casa.
 */
void grabarResultadoCSV() {
  File archivo = SD.open("/resultados.csv", FILE_APPEND);
  if (archivo) {
    archivo.print(puntajeGryffindor);
    archivo.print(",");
    archivo.print(puntajeSlytherin);
    archivo.print(",");
    archivo.print(puntajeRavenclaw);
    archivo.print(",");
    archivo.println(puntajeHufflepuff);
    archivo.close();
    Serial.println("Resultado grabado en CSV"); // Mensaje de confirmación
  } else {
    Serial.println("Error al abrir el archivo CSV"); // Mensaje de error
  }
}
