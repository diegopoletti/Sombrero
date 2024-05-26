const char *version = "2.78";
#include "AudioFileSourceSD.h"   // Fuente de audio desde la tarjeta SD
#include "AudioGeneratorMP3.h"   // Generador de audio MP3
#include "AudioOutputI2SNoDAC.h" // Salida de audio sin DAC
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include <WiFi.h>
#include <ArduinoOTA.h>

bool OTAhabilitado = true; // variable que se utilizara para inabilitar la función OTA si no se pudo lograr la conexion WIFI

// Configuración de la red WiFi
const char *ssid = "DPP";          // Nombre de la red WiFi
const char *password = "monigote"; // Contraseña de la red WiFi

// Pines del Bus SPI para la conexión de la Tarjeta SD.
#define SCK 18  // Clock desde Micro a SD.
#define MISO 19 // Data Out de la SD al Micro (DO).
#define MOSI 23 // Data input de la SD desde el Micro (DI).
#define CS 5    // Chip Select Desde el Micro selecciona la Tarjeta.

// Pines touch para respuestas se debe hacer un PAD dado que el hardware Touch ya lo tiene el ESP32
#define TOUCH_PIN_SI T0       // GPIO 4 - Configura el pin touch para "Sí"
#define TOUCH_PIN_NO T3       // GPIO 15 - Configura el pin touch para "No"
bool touchPresionado = false; // Bandera para verificar si se presionó algún botón Touch

#define PWM_PIN 12 // GPIO 12 - Pin para la salida PWM

// Variables para el manejo del audio
AudioGeneratorMP3 *mp3;            // Generador de audio MP3
AudioFileSourceSD *fuente;         // Fuente de archivo de audio desde la tarjeta SD
AudioOutputI2SNoDAC *salida;       // Salida de audio sin Conversor DAC
bool yaReprodujo = false;          // Bandera para verificar si ya se reprodujo un audio
bool aleatoreaReproducida = false; // bandera para verificar si corresponde reproducción aleatorea.

// Variables para el manejo de la lógica del cuestionario
int preguntaActual = 0;                                                                                                 // Pregunta actual, comienza por la primera pregunta
int puntaje = 0;                                                                                                        // Puntaje acumulado
const int totalPreguntas = 7;                                                                                           // Número total de preguntas
const int puntosRespuesta[2] = {10, 0};                                                                                 // Puntaje para respuestas Si/No
const char *archivosPreguntas[totalPreguntas] = {"q1.mp3", "q2.mp3", "q3.mp3", "q4.mp3", "q5.mp3", "q6.mp3", "q7.mp3"}; // Archivos de audio para las preguntas
unsigned long ultimoUso = 0;                                                                                            // Variable para rastrear el último uso

//--------------------------------------------------------------------------SETUP--------------------------------------------------------------------------
void setup() {
  Serial.begin(115200); // Inicializa la comunicación serial a 115200 baudios
  Serial.println(version); 
  if (!SD.begin(CS)) { // Inicializa la tarjeta SD conectada al pin de Selección CS
    Serial.println("Tarjeta SD no encontrada");
    return; // Si no se encuentra la tarjeta SD, termina la configuración
  }

  mp3 = new AudioGeneratorMP3();      // Crea una instancia del generador de audio MP3
  salida = new AudioOutputI2SNoDAC(); // Crea una instancia de la salida de audio sin DAC
  fuente = new AudioFileSourceSD();
  salida->SetOutputModeMono(true); // Especifica que el audio, de ser estereo reroducirá monoaural

  if (OTAhabilitado)
    initOTA(); // Inicia configuración OTA para programación en el aire.

  reproducirIntroduccion(); // Reproduce el audio de introducción
}

//----------------------------------------------------------------------------------- LOOP---------------------------------------------------------------------
void loop() {
  OTAhabilitado ? ArduinoOTA.handle() : yield(); // Maneja la actualización OTA, solo si la condición OTAhabilitado es Verdadera
  unsigned long tiempoActual = millis(); // Obtiene el tiempo actual

  if (mp3->isRunning()) { // Verifica si el audio está en reproducción
    if (!mp3->loop() && yaReprodujo) { // Si el audio ha terminado de reproducirse
      yaReprodujo = false; // Reinicia la bandera de reproducción
      mp3->stop();         // Detén la reproducción
      fuente->close();     // Se cierra el archivo previamente reproducido.
      Serial.println("Audio Stop");
      Serial.println("Archivo Cerrado");
      yield(); // pasa el control para otras tareas de wifi y/o Bluetooth
    }
  } else { // Verifica si el audio no está en reproducción

    verificarRespuestaTouch(); // Verifica la respuesta del usuario mediante los pines touch

    if (touchPresionado) { // Si fue presionado algún botón
      Serial.println("Algún touch fue presionado");
      touchPresionado = false; // Reinicia la bandera de touch presionado

      if (aleatoreaReproducida) { // se asegura que antes de reproducir una pregunta se reproduzca una respuesta aleatoria.
        if (preguntaActual < totalPreguntas) { // si todavía falta reproducir preguntas 
          Serial.print("Llamado a pregunta: ");
          Serial.println(preguntaActual);
          reproducirPregunta(preguntaActual); // Reproduce la pregunta
          aleatoreaReproducida = false;
        } else { // ya se acabaron las preguntas por lo cual reproduce la inferencia acerca de que casa corresponde.
          Serial.print("Llamado a reproducir Resultado Final:");
          reproducirResultadoFinal(); // Reproduce el resultado final basado en el puntaje
        }
      } else {
        reproducirRespuestaAleatoria();
      }
    }
  }
}

// -------------------------------------------------------------------------------Fin loop----------------------------------
//-------------------------------------------------------------------------------------------Reproducciones-----------
void reproducirIntroduccion() {
  const char *archivoIntroduccion = "/intro.mp3"; // Ruta del archivo de introducción
  reproducirAudio(archivoIntroduccion); // Llamada a la función de reproducción de audio genérica
}

void reproducirPregunta(int numeroPregunta) {
  Serial.print("Pregunta a reproducir:");
  Serial.println(numeroPregunta);
  char ruta[15];
  snprintf(ruta, sizeof(ruta), "/%s", archivosPreguntas[numeroPregunta]); // Formatea la ruta del archivo de audio de la pregunta
  Serial.print("Ruta y archivo a reproducir:");
  Serial.println(ruta);
  reproducirAudio(ruta); // Llamada a la función de reproducción de audio genérica
}

void reproducirRespuestaAleatoria() {
  char ruta[15];
  int numeroRespuesta = random(1, 23);                           // Genera un número aleatorio entre 1 y 23
  snprintf(ruta, sizeof(ruta), "/resp%d.mp3", numeroRespuesta); // Formatea la ruta del archivo de audio de la respuesta aleatoria
  Serial.print("Aleatorea: ");
  Serial.println(ruta);
  aleatoreaReproducida = true; // se reprodujo respuesta aleatoria.
  reproducirAudio(ruta); // Llamada a la función de reproducción de audio genérica
}

void reproducirResultadoFinal() {
  const char *archivoResultado = obtenerResultadoFinal(); // Obtiene la ruta del archivo de resultado final
  reproducirAudio(archivoResultado); // Llamada a la función de reproducción de audio genérica
}

// Función genérica para reproducir un archivo de audio
void reproducirAudio(const char *ruta) {
  if (!SD.exists(ruta)) { // Verifica si el archivo existe en la tarjeta SD
    Serial.println("Archivo no encontrado");
    return; // Si no se encuentra el archivo, termina la función
  }

  if (!fuente->open(ruta)) { // Abre el archivo de audio
    Serial.println("Error al abrir el archivo");
    return; // Si no se pudo abrir el archivo, termina la función
  }

  yield();                    // Pasa el control para otras tareas de WiFi y/o Bluetooth
  mp3->begin(fuente, salida); // Inicia la reproducción del audio
  yaReprodujo = true;         // Marca que ya se está reproduciendo un audio
}
----------------------------------------------------------------------------------Verificación Touch---------------------------------
void verificarRespuestaTouch() {
  // Revisa si se ha tocado el sensor de touch "Sí"
  if (touchRead(TOUCH_PIN_SI) < 30) { // Ajusta este umbral según sea necesario
    Serial.println("Touch 'Sí' presionado");
    touchPresionado = true;
    puntaje += puntosRespuesta[0]; // Suma los puntos de la respuesta "Sí" al puntaje
    preguntaActual++;              // Avanza a la siguiente pregunta
    delay(300);                    // Agrega un pequeño retraso para evitar lecturas múltiples
  }

  // Revisa si se ha tocado el sensor de touch "No"
  if (touchRead(TOUCH_PIN_NO) < 30) { // Ajusta este umbral según sea necesario
    Serial.println("Touch 'No' presionado");
    touchPresionado = true;
    puntaje += puntosRespuesta[1]; // Suma los puntos de la respuesta "No" al puntaje
    preguntaActual++;              // Avanza a la siguiente pregunta
    delay(300);                    // Agrega un pequeño retraso para evitar lecturas múltiples
  }
}
------------------------------------------------------------------------------------ Calculo de REsultado Final--------------------
const char *obtenerResultadoFinal() {
  if (puntaje <= 10) {
    return "/result4.mp3"; // Ruta del archivo de resultado para puntajes de 0 a 10
  } else if (puntaje <= 20) {
    return "/result3.mp3"; // Ruta del archivo de resultado para puntajes de 11 a 20
  } else if (puntaje <= 30) {
    return "/result2.mp3"; // Ruta del archivo de resultado para puntajes de 21 a 30
  } else {
    return "/result1.mp3"; // Ruta del archivo de resultado para puntajes de 31 a 40
  }
}

// -------------------------------------------------------------------------------------------OTA--------------------------------------------------------------------

void initOTA() {
  // Conexión a la red WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Conectando a WiFi...");
  }
  Serial.println("Conectado a la red WiFi");

  // Configuración de OTA
  ArduinoOTA.onStart([]() {
    String tipo;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      tipo = "sketch";
    } else { // U_SPIFFS
      tipo = "filesystem";
    }
    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Inicio de actualización OTA: " + tipo);
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("\nFin de la actualización OTA");
  });

  ArduinoOTA.onProgress([](unsigned int progreso, unsigned int total) {
    Serial.printf("Progreso: %u%%\r", (progreso / (total / 100)));
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error [%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Error de autenticación");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Error de inicio");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Error de conexión");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Error de recepción");
    } else if (error == OTA_END_ERROR) {
      Serial.println("Error de finalización");
    }
  });

  ArduinoOTA.begin();
  Serial.println("Listo para actualización OTA");
}
