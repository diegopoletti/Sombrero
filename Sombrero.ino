const char *version = "3.00"; // Versión del programa 
//se realiza cambio a verificación por pulsadores estandard.
#include "AudioFileSourceSD.h"   // Biblioteca para fuente de audio desde la tarjeta SD
#include "AudioGeneratorMP3.h"   // Biblioteca para generar audio MP3
#include "AudioOutputI2SNoDAC.h" // Biblioteca para salida de audio sin DAC
#include "FS.h"                  // Biblioteca del sistema de archivos
#include "SD.h"                  // Biblioteca para tarjeta SD
#include "SPI.h"                 // Biblioteca para interfaz SPI
#include <WiFi.h>                // Biblioteca para WiFi
#include <ArduinoOTA.h>          // Biblioteca para actualizaciones OTA

bool OTAhabilitado = false; // Variable para habilitar/deshabilitar OTA

// Configuración de la red WiFi
const char *ssid = "";       // Nombre de la red WiFi
const char *password = ""; // Contraseña de la red WiFi

// Pines del Bus SPI para la conexión de la Tarjeta SD
#define SCK 18   // Pin de reloj para SPI
#define MISO 19  // Pin de salida de datos para SPI
#define MOSI 23  // Pin de entrada de datos para SPI
#define CS 5     // Pin de selección de chip para SPI

// Pines touch para respuestas
#define TOUCH_PIN_SI T0       // GPIO 4 - Pin touch para "Sí"
#define TOUCH_PIN_NO T3       // GPIO 15 - Pin touch para "No"
bool touchPresionado = true; // Bandera para verificar si se presionó algún botón Touch

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
int puntajeTotal = 0;                                                                                                // Puntaje acumulado
const int totalPreguntas = 8;                                                                                        // Número total de preguntas
const int puntosRespuesta[8] = {10, 20, 30, 40, 50, 60, 70, 80};                                                     // Puntuación de cada pregunta
const char *archivosPreguntas[totalPreguntas] = {"q1.mp3", "q2.mp3", "q3.mp3", "q4.mp3", "q5.mp3", "q6.mp3", "q7.mp3", "q8.mp3"}; // Archivos de audio para las preguntas
unsigned long ultimoUso = 0;                                                                                         // Variable para rastrear el último uso

//--------------------------------------------------------------------------SETUP--------------------------------------------------------------------------
void setup() {
  Serial.begin(115200); // Inicializa la comunicación serial a 115200 baudios
  Serial.println(version); // Imprime la versión del programa
  if (!SD.begin(CS)) { // Inicializa la tarjeta SD conectada al pin de selección CS
    Serial.println("Tarjeta SD no encontrada"); // Mensaje de error si no se encuentra la tarjeta SD
    return; // Termina la configuración si no se encuentra la tarjeta SD
  }
 // Configuración de los pines de los pulsadores como entradas
  pinMode(PIN_SI, INPUT);
  pinMode(PIN_NO, INPUT);
  
  //crea insatancias para el manejo de MP3, Salida de Audio y SD
  mp3 = new AudioGeneratorMP3();      // Crea una instancia del generador de audio MP3
  salida = new AudioOutputI2SNoDAC(); // Crea una instancia de la salida de audio sin DAC
  fuente = new AudioFileSourceSD();   // Crea una instancia de la fuente de audio desde la tarjeta SD
  salida->SetOutputModeMono(true); // Configura la salida de audio en modo monoaural

  if (OTAhabilitado)
    initOTA(); // Inicia la configuración OTA si está habilitada
  
  reproducirIntroduccion(); // Reproduce el audio de introducción
 // touchPresionado =true; hace que ingrese por primera vez a reproducir la pregunta.
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
      yield(); // Pasa el control a otras tareas
    }
  } else { // Si el audio no está en reproducción

    verificarRespuestaTouch(); // Verifica la respuesta del usuario mediante los pines touch

    if (touchPresionado) { // Si se presionó algún botón touch
      Serial.println("Algún touch fue presionado");
      touchPresionado = false; // Reinicia la bandera de touch presionado 
      Serial.println("Puntaje total: " + puntajeTotal);

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
    }
  }
  //reproducirPregunta(preguntaActual);
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
  touchPresionado = true;
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
//----------------------------------------------------------------------------------Verificación Touch---------------------------------
void verificarRespuestaTouch() {
  // 27-06-24 se decide cambiar a pulsadores estandar ya que el cableado puede ser largo y generar problemas con el sistema de pulsado touch
  /*// Revisa si se ha tocado el sensor de touch "Sí"
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
}*/
  unsigned long tiempoActual = millis(); // Obtener el tiempo actual

  // Revisa si se ha presionado el pulsador "Sí" y maneja el debounce
  if (digitalRead(PIN_SI) == HIGH) { // Verifica si el pulsador "Sí" está presionado
    if ((tiempoActual - ultimoTiempoSi) > debounceDelay) { // Verifica si ha pasado el tiempo del debounce
      Serial.println("Pulsador 'Sí' presionado");
      pulsadorSiPresionado = true; // Marca el pulsador "Sí" como presionado
      puntaje += puntosRespuesta[0]; // Suma los puntos de la respuesta "Sí" al puntaje
      preguntaActual++;              // Avanza a la siguiente pregunta
      ultimoTiempoSi = tiempoActual; // Actualiza el tiempo de la última lectura del pulsador "Sí"
    }
  }

  // Revisa si se ha presionado el pulsador "No" y maneja el debounce
  if (digitalRead(PIN_NO) == HIGH) { // Verifica si el pulsador "No" está presionado
    if ((tiempoActual - ultimoTiempoNo) > debounceDelay) { // Verifica si ha pasado el tiempo del debounce
      Serial.println("Pulsador 'No' presionado");
      pulsadorNoPresionado = true; // Marca el pulsador "No" como presionado
      puntaje += puntosRespuesta[1]; // Suma los puntos de la respuesta "No" al puntaje
      preguntaActual++;              // Avanza a la siguiente pregunta
      ultimoTiempoNo = tiempoActual; // Actualiza el tiempo de la última lectura del pulsador "No"
    }
  }
}
//------------------------------------------------------------------------------------ Calculo de REsultado Final--------------------
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
    return "/Result_error.mp3"; // Ruta del archivo para errores o puntajes fuera de rango todas respuestas Negativas.
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
