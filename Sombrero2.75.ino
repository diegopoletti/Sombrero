//Version 2.75
#include "AudioFileSourceSD.h" // Fuente de audio desde la tarjeta SD
#include "AudioGeneratorMP3.h" // Generador de audio MP3
#include "AudioOutputI2SNoDAC.h" // Salida de audio sin DAC
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include <WiFi.h>
#include <ArduinoOTA.h>

// Configuración de la red WiFi
const char* ssid = "DPP"; // Nombre de la red WiFi
const char* password = "monigote"; // Contraseña de la red WiFi

// Pines del Bus SPI para la conexión de la Tarjeta SD.
#define SCK 18 // Clock desde Micro a SD.
#define MISO 19 // Data Out de la SD al Micro (DO).
#define MOSI 23 // Data input de la SD desde el Micro (DI).
#define CS 5 // Chip Select Desde el Micro selecciona la Tarjeta.

// Pines touch para respuestas se debe hacer un PAD dado que el hardware Touch ya lo tiene el ESP32
#define TOUCH_PIN_SI T0 // GPIO 4 - Configura el pin touch para "Sí"
#define TOUCH_PIN_NO T3 // GPIO 15 - Configura el pin touch para "No"
bool touchPresionado = false; // Bandera para verificar si se presionó algún botón Touch

#define PWM_PIN 12 // GPIO 12 - Pin para la salida PWM

// Variables para el manejo del audio
AudioGeneratorMP3 *mp3; // Generador de audio MP3
AudioFileSourceSD *fuente; // Fuente de archivo de audio desde la tarjeta SD
AudioOutputI2SNoDAC *salida; // Salida de audio sin Conversor DAC
bool yaReprodujo = false; // Bandera para verificar si ya se reprodujo un audio
bool aleatoreaReproducida= false; //bandera para verificar si corresponde reproducción aleatorea.
// Variables para el manejo de la lógica del cuestionario
int preguntaActual = 0; // Pregunta actual, comienza por la primera pregunta
int puntaje = 0; // Puntaje acumulado
const int totalPreguntas = 7; // Número total de preguntas
const int puntosRespuesta[2] = {10, 0}; // Puntaje para respuestas Si/No
const char *archivosPreguntas[totalPreguntas] = {"q1.mp3", "q2.mp3", "q3.mp3", "q4.mp3", "q5.mp3", "q6.mp3", "q7.mp3"}; // Archivos de audio para las preguntas
unsigned long ultimoUso = 0; // Variable para rastrear el último uso
//--------------------------------------------------------------------------SETUP--------------------------------------------------------------------------
void setup() {
  Serial.begin(115200); // Inicializa la comunicación serial a 115200 baudios

  if (!SD.begin(CS)) { // Inicializa la tarjeta SD conectada al pin de Selección CS
    Serial.println("Tarjeta SD no encontrada");
    return; // Si no se encuentra la tarjeta SD, termina la configuración
  }

  mp3 = new AudioGeneratorMP3(); // Crea una instancia del generador de audio MP3
  salida = new AudioOutputI2SNoDAC(); // Crea una instancia de la salida de audio sin DAC
    fuente = new AudioFileSourceSD();
  // Configuración explícita de los pines I2S
  //salida->SetPinout(26, 25, 22); // Ejemplo de configuración de pines: BCLK, LRC, DOUT
   /*
  Descripción: AudioOutputI2SNoDAC se utiliza para generar una señal “analógica” 
  en los datos I2S mediante una técnica de delta-sigma (emulación de DAC) en lugar de utilizar un DAC físico. 
  Esto significa que no hay un convertidor digital-analógico (DAC) dedicado en el chip,
   sino que se utiliza una técnica de modulación para generar una señal de audio.
  Funcionamiento: En lugar de enviar directamente una señal analógica a un DAC, 
  AudioOutputI2SNoDAC genera una señal digital de alta frecuencia (delta-sigma) 
  que se envía a través del bus I2S. Luego, un filtro pasa-bajas en el amplificador de audio 
  del dispositivo convierte esta señal en una forma de onda analógica.
  se debe colocar un filtro pasabajos R/C (fc=1/2*pi*R*C) 
  donde fc es la frecuencia de corte en el digrama de bode de para 1 polo -3dB si se hace a 4kHz puede ser 3k3 10pF
  se observa que para mejorar el sonido se coloca 1,5K y 100nF
  */
   salida->SetOutputModeMono(true); // Especifica que el audio, de ser estereo reroducirá monoaural
  

  // Conexión a la red WiFi
  WiFi.begin(ssid, password); // Comienza la conexión WiFi con el SSID y contraseña proporcionados
  while (WiFi.status() != WL_CONNECTED) { // Espera hasta que el ESP32 esté conectado a la red WiFi
    delay(1000); // Retraso de 1 segundo entre intentos de conexión
    yield(); //pasa el control para otras tareas de wifi y/o Bluetooth
    Serial.println("Conectando a WiFi...");
  }
  Serial.println("Conectado a la WiFi"); // Mensaje de confirmación de conexión
   initOTA(); //Inicia configuración OTA para programación en el aire.
  reproducirIntroduccion(); // Reproduce el audio de introducción
 }
//----------------------------------------------------------------------------------- LOOP---------------------------------------------------------------------
void loop() {
  
  unsigned long tiempoActual = millis(); // Obtiene el tiempo actual
 if (mp3->isRunning()) { // Verifica si el audio está en reproducción
  if (!mp3->loop() && yaReprodujo) { // Si el audio ha terminado de reproducirse
      yaReprodujo = false; // Reinicia la bandera de reproducción
      mp3->stop(); // Detén la reproducción 
      fuente->close(); // Se cierra el archivo previamente reproducido.
      Serial.println("Audio Stop");
      Serial.println("Archivo Cerrado"); 
      yield(); //pasa el control para otras tareas de wifi y/o Bluetooth
    }
    } else {  // Verifica si el audio no está en reproducción
          ArduinoOTA.handle(); // Maneja la actualización OTA
         verificarRespuestaTouch(); // Verifica la respuesta del usuario mediante los pines touch
          //delay(1);
       if (touchPresionado) { // Si fue presionado algún botón
              Serial.println("Algún touch fue presionado");
              touchPresionado = false; // Reinicia la bandera de touch presionado
              if (preguntaActual < totalPreguntas) 
                   {
                    //Serial.print("Llamado a  respuesta aleatorea ");
                      //reproducirRespuestaAleatoria();                        
                      Serial.print("Llamado a pregunta: ");
                      Serial.println(preguntaActual);
                      reproducirPregunta(preguntaActual);// Reproduce la pregunta
                    
                   } else {
                      Serial.print("Llamado a reproducir Resultado Final:");
                      reproducirResultadoFinal(); // Reproduce el resultado final basado en el puntaje
                      }
           }
  }
}

void reproducirIntroduccion() {
  const char* archivoIntroduccion = "/intro.mp3"; // Ruta del archivo de introducción
  if (!SD.exists(archivoIntroduccion)) { // Verifica si el archivo de introducción existe
    Serial.println("Archivo de introducción no encontrado");
    return; // Si no se encuentra el archivo, termina la función
  }
  
  if (!fuente->open(archivoIntroduccion)) { // Abre el archivo de introducción
    Serial.println("Error al abrir el archivo de introducción");
    return; // Si no se pudo abrir el archivo, termina la función
  }
  yield(); //pasa el control para otras tareas de wifi y/o Bluetooth
  mp3->begin(fuente, salida); // Inicia la reproducción del audio de introducción
  yaReprodujo = true; // Marca que ya se está reproduciendo un audio
}

void reproducirPregunta(int numeroPregunta) {
  Serial.print("Pregunta a reproducir:");
  Serial.println(numeroPregunta);
  char ruta[15];
  snprintf(ruta, sizeof(ruta), "/%s", archivosPreguntas[numeroPregunta]); // Formatea la ruta del archivo de audio de la pregunta
  Serial.print("Ruta y archivo a reproducir:");
  Serial.println(ruta);
  
  if (!SD.exists(ruta)) { // Verifica si el archivo de la pregunta existe
    Serial.println("Archivo de pregunta no encontrado");
    return; // Si no se encuentra el archivo, termina la función
  }
  
  if (!fuente->open(ruta)) { // Abre el archivo de la pregunta
    Serial.println("Error al abrir el archivo de pregunta");
    return; // Si no se pudo abrir el archivo, termina la función
  }
  //Serial.println("Fuente cargada:");
  yield(); //pasa el control para otras tareas de wifi y/o Bluetooth
  mp3->begin(fuente, salida); // Inicia la reproducción del audio de la pregunta
  yaReprodujo = true; // Marca que ya se está reproduciendo un audio
  aleatoreaReproducida= true; //levanta la bandera de que la respuesta Aleatorea fue reproducida
 // Serial.println("Se llamó a mp3 begin y regresó");
}

void reproducirRespuestaAleatoria() {
  char ruta[15];
  int numeroRespuesta = random(1, 4); // Genera un número aleatorio entre 1 y 4
  snprintf(ruta, sizeof(ruta), "/resp%d.mp3", numeroRespuesta); // Formatea la ruta del archivo de audio de la respuesta aleatoria
  yield(); //pasa el control para otras tareas de wifi y/o Bluetooth
  Serial.print("Aleatorea: ");
  Serial.println(ruta);
  if (!SD.exists(ruta)) { // Verifica si el archivo de respuesta aleatoria existe
    Serial.println("Archivo de respuesta aleatoria no encontrado");
    return; // Si no se encuentra el archivo, termina la función
  }
  
  if (!fuente->open(ruta)) { // Abre el archivo de respuesta
    Serial.println("Error al abrir el archivo de respuesta aleatoria");
    return; // Si no se pudo abrir el archivo, termina la función
  }
  yield(); //pasa el control para otras tareas de wifi y/o Bluetooth
  mp3->begin(fuente, salida); // Inicia la reproducción del audio de la respuesta
  aleatoreaReproducida= true; //levanta la bandera de que la respuesta Aleatorea fue reproducida
  yaReprodujo = false; // Marca que ya se está reproduciendo un audio
}

void verificarRespuestaTouch() {
  if (touchRead(TOUCH_PIN_SI) < 20) { // Verifica si se tocó el pin "Sí"
    Serial.println("Sí presionado");
    puntaje += puntosRespuesta[0]; // Suma puntos al puntaje
    Serial.print("Puntaje Acumulado:");
    Serial.println(puntaje);
    preguntaActual++; // Pasa a la siguiente pregunta
    Serial.print("Pregunta a la que se pasará:");
    Serial.println(preguntaActual);
    ultimoUso = millis(); // Actualiza el tiempo del último uso
    Serial.print("Actualización de último uso:");
    Serial.println(ultimoUso);
    touchPresionado = true; // Marca que se presionó un touch
  } else if (touchRead(TOUCH_PIN_NO) < 20) { // Verifica si se tocó el pin "No"
    Serial.println("No presionado");
    puntaje += puntosRespuesta[1]; // Suma puntos al puntaje
    Serial.print("Puntaje Acumulado:");
    Serial.println(puntaje);
    preguntaActual++; // Pasa a la siguiente pregunta
    Serial.print("Pregunta a la que se pasará:");
    Serial.println(preguntaActual);
    ultimoUso = millis(); // Actualiza el tiempo del último uso
    Serial.print("Actualización de último uso:");
    Serial.println(ultimoUso);
    touchPresionado = true; // Marca que se presionó un touch
  }
//  Serial.print("Resultado de la bandera touchPresionado:");
//  Serial.println(touchPresionado);
}

void reproducirResultadoFinal() {
  char ruta[15];
  if (puntaje >= 70) {
    snprintf(ruta, sizeof(ruta), "/result4.mp3"); // Formatea la ruta del archivo de audio del resultado
  } else if (puntaje >= 50) {
    snprintf(ruta, sizeof(ruta), "/result3.mp3");
  } else if (puntaje >= 30) {
    snprintf(ruta, sizeof(ruta), "/result2.mp3");
  } else {
    snprintf(ruta, sizeof(ruta), "/result1.mp3");
  }
  puntaje = 0; // Resetea el puntaje
  preguntaActual = 0; // Resetea la pregunta actual
  
  if (!SD.exists(ruta)) { // Verifica si el archivo de resultado final existe
    Serial.println("Archivo de resultado final no encontrado");
    return; // Si no se encuentra el archivo, termina la función
  }
  yield(); //pasa el control para otras tareas de wifi y/o Bluetooth
  if (!fuente->open(ruta)) { // Abre el archivo de resultado
    Serial.println("Error al abrir el archivo de resultado final");
    return; // Si no se pudo abrir el archivo, termina la función
  }
  yield(); //pasa el control para otras tareas de wifi y/o Bluetooth;
  mp3->begin(fuente, salida); // Inicia la reproducción del audio de resultado
  yaReprodujo = true; // Marca que ya se está reproduciendo un audio
}

void initOTA() {
    // Configuración de OTA
  ArduinoOTA.setHostname("Sombrero");
  ArduinoOTA.onStart([]() { // Configura la función de inicio de la actualización OTA
    String tipo;
    if (ArduinoOTA.getCommand() == U_FLASH) { // Verifica si la actualización es del sketch
      tipo = "sketch";
    } else { // O de SPIFFS
      tipo = "filesystem";
    }
    Serial.println("Inicio de actualización: " + tipo);
  });
  ArduinoOTA.onEnd([]() { // Configura la función de finalización de la actualización OTA
    Serial.println("\nFin de la actualización");
  });
  ArduinoOTA.onProgress([](unsigned int progreso, unsigned int total) { // Configura la función de progreso de la actualización OTA
    Serial.printf("Progreso: %u%%\r", (progreso / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) { // Configura la función de manejo de errores de la actualización OTA
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Error de autenticación");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Error al comenzar");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Error de conexión");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Error al recibir");
    } else if (error == OTA_END_ERROR) {
      Serial.println("Error al finalizar");
    }
  });
  yield(); //pasa el control para otras tareas de wifi y/o Bluetooth
  ArduinoOTA.begin(); // Inicia la configuración de OTA
  
  
  }
