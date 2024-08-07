const char *version = "4.00"; // Versión del programa 
// Se realiza cambio a verificación por pulsadores estándar y se agregan servomotores

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

// Pines para los servomotores
#define PIN_SERVO_CUSPIDE 13 // Pin para el servomotor de la cúspide
#define PIN_SERVO_BOCA 14    // Pin para el servomotor de la boca

// Objetos Servo
Servo servoCuspide; // Servo para la cúspide del sombrero
Servo servoBoca;    // Servo para la boca del sombrero

bool pulsadorPresionado = false; // Bandera para verificar si se presionó algún pulsador

// Variables para el estado de los pulsadores y manejo del debounce
bool pulsadorSiPresionado = false; // Variable para el estado del pulsador "Sí"
bool pulsadorNoPresionado = false; // Variable para el estado del pulsador "No"
unsigned long ultimoTiempoSi = 0;  // Tiempo de la última lectura del pulsador "Sí"
unsigned long ultimoTiempoNo = 0;  // Tiempo de la última lectura del pulsador "No"
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
int puntajeTotal = 0;                                                                                                 // Puntaje acumulado
const int totalPreguntas = 8;                                                                                         // Número total de preguntas
const int puntosRespuesta[8] = {10, 20, 30, 40, 50, 60, 70, 80};                                                      // Puntuación de cada pregunta
const char *archivosPreguntas[totalPreguntas] = {"q1.mp3", "q2.mp3", "q3.mp3", "q4.mp3", "q5.mp3", "q6.mp3", "q7.mp3", "q8.mp3"}; // Archivos de audio para las preguntas
unsigned long ultimoUso = 0;                                                                                          // Variable para rastrear el último uso

// Variables para el control de los servomotores
unsigned long ultimoMovimientoCuspide = 0; // Último tiempo de movimiento de la cúspide
unsigned long ultimoMovimientoBoca = 0;    // Último tiempo de movimiento de la boca
const int intervaloMovimientoCuspide = 500; // Intervalo para el movimiento de la cúspide (en ms)
const int intervaloMovimientoBoca = 100;    // Intervalo para el movimiento de la boca (en ms)

//--------------------------------------------------------------------------SETUP--------------------------------------------------------------------------
void setup() {
  Serial.begin(115200); // Inicializa la comunicación serial a 115200 baudios
  Serial.println(version); // Imprime la versión del programa

  if (!SD.begin(CS)) { // Inicializa la tarjeta SD conectada al pin de selección CS
    Serial.println("Tarjeta SD no encontrada"); // Mensaje de error si no se encuentra la tarjeta SD
    return; // Termina la configuración si no se encuentra la tarjeta SD
  }

  // Configuración de los pines de los pulsadores como entradas con pull-up interno
  pinMode(PIN_SI, INPUT_PULLUP);
  pinMode(PIN_NO, INPUT_PULLUP);
  
  // Inicialización de los servomotores
  ESP32PWM::allocateTimer(0); // Asigna el timer 0 para los servos
  ESP32PWM::allocateTimer(1); // Asigna el timer 1 para los servos
  servoCuspide.setPeriodHertz(50); // Establece la frecuencia del servo de la cúspide
  servoBoca.setPeriodHertz(50);    // Establece la frecuencia del servo de la boca
  servoCuspide.attach(PIN_SERVO_CUSPIDE, 500, 2400); // Adjunta el servo de la cúspide al pin correspondiente
  servoBoca.attach(PIN_SERVO_BOCA, 500, 2400);       // Adjunta el servo de la boca al pin correspondiente

  // Crea instancias para el manejo de MP3, Salida de Audio y SD
  mp3 = new AudioGeneratorMP3();      // Crea una instancia del generador de audio MP3
  salida = new AudioOutputI2SNoDAC(); // Crea una instancia de la salida de audio sin DAC
  fuente = new AudioFileSourceSD();   // Crea una instancia de la fuente de audio desde la tarjeta SD
  salida->SetOutputModeMono(true);    // Configura la salida de audio en modo monoaural

  if (OTAhabilitado)
    initOTA(); // Inicia la configuración OTA si está habilitada
  
  reproducirIntroduccion(); // Reproduce el audio de introducción
}

//----------------------------------------------------------------------------------- LOOP---------------------------------------------------------------------
void loop() {
  OTAhabilitado ? ArduinoOTA.handle() : yield(); // Maneja la actualización OTA si está habilitada
  unsigned long tiempoActual = millis(); // Obtiene el tiempo actual

  if (mp3->isRunning()) { // Verifica si el audio está en reproducción
    if (!mp3->loop() && yaReprodujo) { // Si el audio ha terminado de reproducirse
      yaReprodujo = false; // Reinicia la bandera de reproducción
      mp3->stop();         // Detiene la reproducción
      fuente->close();     // Cierra el archivo de audio
      Serial.println("Audio Stop"); // Mensaje de parada de audio
      Serial.println("Archivo Cerrado"); // Mensaje de archivo cerrado
      // Detiene el movimiento de los servomotores
      servoCuspide.write(90); // Posición central para la cúspide
      servoBoca.write(90);    // Posición central para la boca
      yield(); // Pasa el control a otras tareas
    } else {
      // Mueve los servomotores mientras se reproduce el audio
      moverServoCuspide();
      moverServoBoca();
    }
  } else { // Si el audio no está en reproducción
    verificarRespuestaPulsadores(); // Verifica la respuesta del usuario mediante los pulsadores

    if (pulsadorPresionado) { // Si se presionó algún pulsador
      Serial.println("Algún pulsador fue presionado");
      pulsadorPresionado = false; // Reinicia la bandera de pulsador presionado 
      Serial.println("Puntaje total: " + String(puntajeTotal));

      if (aleatoreaReproducida) { // Verifica si se reprodujo una respuesta aleatoria
        if (preguntaActual < totalPreguntas) { // Si quedan preguntas por reproducir
          Serial.print("Llamado a pregunta: ");
          Serial.println(preguntaActual);
          reproducirPregunta(preguntaActual); // Reproduce la pregunta actual
          aleatoreaReproducida = false; // Reinicia la bandera de respuesta aleatoria
        } else { // Si no quedan preguntas por reproducir
          Serial.print("Llamado a reproducir Resultado Final:");
          reproducirResultadoFinal(); // Reproduce el resultado final basado en el puntaje
        }
      } else {
        reproducirRespuestaAleatoria(); // Reproduce una respuesta aleatoria
      }
    }
  }
}

// -------------------------------------------------------------------------------Fin loop----------------------------------
//-------------------------------------------------------------------------------------------Reproducciones-----------
void reproducirIntroduccion() {
  const char *archivoIntroduccion = "/intro.mp3"; // Ruta del archivo de introducción
  reproducirAudio(archivoIntroduccion); // Llama a la función de reproducción de audio genérica
  while (mp3->isRunning()){
    if (!mp3->loop() && yaReprodujo) { // Si el audio ha terminado de reproducirse
      yaReprodujo = false; // Reinicia la bandera de reproducción
      mp3->stop();         // Detiene la reproducción
      fuente->close();     // Cierra el archivo de audio
      Serial.println("Audio Stop"); // Mensaje de parada de audio
      Serial.println("Archivo Cerrado"); // Mensaje de archivo cerrado
      yield(); // Pasa el control a otras tareas
    } else {
      // Mueve los servomotores mientras se reproduce el audio de introducción
      moverServoCuspide();
      moverServoBoca();
    }
  }
  aleatoreaReproducida = true; // Marca que se reprodujo una respuesta aleatoria
  return;
}

void reproducirPregunta(int numeroPregunta) {
  Serial.print("Pregunta a reproducir:");
  Serial.println(numeroPregunta);
  char ruta[15];
  snprintf(ruta, sizeof(ruta), "/%s", archivosPreguntas[numeroPregunta - 1]); // Formatea la ruta del archivo de audio de la pregunta
  Serial.print("Ruta y archivo a reproducir:");
  Serial.println(ruta);
  reproducirAudio(ruta); // Llama a la función de reproducción de audio genérica
}

void reproducirRespuestaAleatoria() {
  char ruta[15];
  int numeroRespuesta = random(1, 22); // Genera un número aleatorio entre 1 y 22
  snprintf(ruta, sizeof(ruta), "/resp%d.mp3", numeroRespuesta); // Formatea la ruta del archivo de audio de la respuesta aleatoria
  Serial.print("Aleatoria: ");
  Serial.println(ruta);
  aleatoreaReproducida = true; // Marca que se reprodujo una respuesta aleatoria
  reproducirAudio(ruta); // Llama a la función de reproducción de audio genérica
  pulsadorPresionado = true;
}

void reproducirResultadoFinal() {
  const char *archivoResultado = obtenerResultadoFinal(); // Obtiene la ruta del archivo de resultado final
  reproducirAudio(archivoResultado); // Llama a la función de reproducción de audio genérica
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
}

//----------------------------------------------------------------------------------Verificación Pulsadores---------------------------------
void verificarRespuestaPulsadores() {
  unsigned long tiempoActual = millis(); // Obtener el tiempo actual

  // Revisa si se ha presionado el pulsador "Sí" y maneja el debounce
  if (digitalRead(PIN_SI) == LOW) { // Verifica si el pulsador "Sí" está presionado (activo en bajo)
    if ((tiempoActual - ultimoTiempoSi) > debounceDelay) { // Verifica si ha pasado el tiempo del debounce
      Serial.println("Pulsador 'Sí' presionado");
      pulsadorSiPresionado = true; // Marca el pulsador "Sí" como presionado
      puntajeTotal += puntosRespuesta[preguntaActual - 1]; // Suma los puntos de la respuesta "Sí" al puntaje
      preguntaActual++;              // Avanza a la siguiente pregunta
      ultimoTiempoSi = tiempoActual; // Actualiza el tiempo de la última lectura del pulsador "Sí"
      pulsadorPresionado = true; // Indica que se ha presionado un pulsador
    }
  }

  // Revisa si se ha presionado el pulsador "No" y maneja el debounce
  if (digitalRead(PIN_NO) == LOW) { // Verifica si el pulsador "No" está presionado (activo en bajo)
    if ((tiempoActual - ultimoTiempoNo) > debounceDelay) { // Verifica si ha pasado el tiempo del debounce
      Serial.println("Pulsador 'No' presionado");
      pulsadorNoPresionado = true; // Marca el pulsador "No" como presionado
      preguntaActual++;              // Avanza a la siguiente pregunta sin sumar puntos
      ultimoTiempoNo = tiempoActual; // Actualiza el tiempo de la última lectura del pulsador "No"
      pulsadorPresionado = true; // Indica que se ha presionado un pulsador
    }
  }
}

//------------------------------------------------------------------------------------ Cálculo de Resultado Final--------------------
const char *obtenerResultadoFinal() {
  // Determina la casa de Hogwarts basada en el puntaje total
  if (puntajeTotal >= 10 && puntajeTotal < 60) {
    return "/Gryffindor.mp3"; // Ruta del archivo para Gryffindor
  } else if (puntajeTotal >= 60 && puntajeTotal < 140) {
    return "/Ravenclaw.mp3"; // Ruta del archivo para Ravenclaw
  } else if (puntajeTotal >= 140 && puntajeTotal < 210) {
    return "/Hufflepuff.mp3"; // Ruta del archivo para Hufflepuff
  } else if (puntajeTotal >= 210 && puntajeTotal <= 280) {
    return "/Slytherin.mp3"; // Ruta del archivo para Slytherin
  } else {
    return "/Result_error.mp3"; // Ruta del archivo para errores o puntajes fuera de rango
  }
}

// -------------------------------------------------------------------------------------------OTA--------------------------------------------------------------------
void initOTA() {
  WiFi.begin(ssid, password); // Conecta a la red WiFi
  while (WiFi.status() != WL_CONNECTED) { // Espera hasta que la conexión se establezca
    delay(500); // Espera medio segundo antes de intentar nuevamente
    Serial.println("Conectando a WiFi..."); // Mensaje de conexión a WiFi
  }
  Serial.println("Conectado a la red WiFi"); // Mensaje de conexión exitosa

  // Configura los eventos de OTA
  ArduinoOTA.onStart([]() {
    String tipo;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      tipo = "sketch"; // Actualización de sketch
    } else {
      tipo = "filesystem"; // Actualización de sistema de archivos
    }
    Serial.println("Inicio de actualización OTA: " + tipo);
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("\nFin de la actualización OTA"); // Mensaje al finalizar la actualización
  });

  ArduinoOTA.onProgress([](unsigned int progreso, unsigned int total) {
    Serial.printf("Progreso: %u%%\r", (progreso / (total / 100))); // Mensaje de progreso de la actualización
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error [%u]: ", error); // Mensaje de error con código
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Error de autenticación"); // Error de autenticación
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Error de inicio"); // Error al iniciar
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Error de conexión"); // Error de conexión
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Error de recepción"); // Error de recepción
    } else if (error == OTA_END_ERROR) {
      Serial.println("Error de finalización"); // Error de finalización
    }
  });

  ArduinoOTA.begin(); // Inicia el servicio OTA
  Serial.println("Listo para actualización OTA"); // Mensaje de preparación para OTA
}

// Función para mover el servomotor de la cúspide
void moverServoCuspide() {
  unsigned long tiempoActual = millis();
  if (tiempoActual - ultimoMovimientoCuspide > intervaloMovimientoCuspide) {
    int posicion = random(45, 136); // Genera una posición aleatoria entre 45 y 135 grados
    servoCuspide.write(posicion);
    ultimoMovimientoCuspide = tiempoActual;
  }
}

// Función para mover el servomotor de la boca
void moverServoBoca() {
  unsigned long tiempoActual = millis();
  if (tiempoActual - ultimoMovimientoBoca > intervaloMovimientoBoca) {
    int posicion = random(70, 111); // Genera una posición aleatoria entre 70 y 110 grados
    servoBoca.write(posicion);
    ultimoMovimientoBoca = tiempoActual;
  }
}
