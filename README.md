
 # Sombrero Seleccionador ESP32

## Descripción del Proyecto

El Sombrero Seleccionador ESP32 es una recreación interactiva del famoso Sombrero Seleccionador de Harry Potter, utilizando un ESP32 como cerebro del proyecto. Este dispositivo hace preguntas a los usuarios y, basándose en sus respuestas, determina a qué casa de Hogwarts pertenecen.

## Características Principales

- Reproduce preguntas y respuestas mediante archivos de audio MP3.
- Utiliza pulsadores para que los usuarios respondan "Sí" o "No".
- Incluye movimientos de servomotores para simular la "boca" y la "cúspide" del sombrero.
- Calcula un puntaje basado en las respuestas y determina el resultado final.
- Soporte para actualizaciones Over-The-Air (OTA).

## Explicación del Código

El código está escrito en C++ para la plataforma ESP32 y utiliza varias bibliotecas para manejar audio, servomotores y funcionalidades del ESP32.

### Funciones Principales

1. `setup()`: Inicializa todos los componentes y reproduce la introducción.
2. `loop()`: Maneja la lógica principal del programa, incluyendo la reproducción de audio y la verificación de pulsadores.
3. `reproducirIntroduccion()`: Reproduce el audio de introducción.
4. `reproducirPregunta(int numeroPregunta)`: Reproduce una pregunta específica.
5. `reproducirRespuestaAleatoria()`: Selecciona y reproduce una respuesta aleatoria.
6. `reproducirResultadoFinal()`: Reproduce el resultado final basado en el puntaje.
7. `verificarRespuestaPulsadores()`: Maneja la lógica de los pulsadores y actualiza el puntaje.
8. `moverServoCuspide()` y `moverServoBoca()`: Controlan el movimiento de los servomotores.

### Componentes del Código

- Manejo de audio: Utiliza las bibliotecas AudioFileSourceSD, AudioGeneratorMP3 y AudioOutputI2SNoDAC.
- Control de servomotores: Usa la biblioteca ESP32Servo.
- Almacenamiento: Los archivos de audio se almacenan en una tarjeta SD.
- Entradas: Utiliza pulsadores para las respuestas "Sí" y "No".
- OTA: Incluye funcionalidad para actualizaciones Over-The-Air.

## Hardware Necesario

- ESP32 DevKit
- Módulo lector de tarjeta SD
- 2 Servomotores
- 2 Pulsadores
- Altavoz
- Tarjeta SD con archivos de audio

## Conexionado de Hardware

1. **Tarjeta SD:**
   - CS -> GPIO 5
   - MOSI -> GPIO 23
   - MISO -> GPIO 19
   - SCK -> GPIO 18

2. **Pulsadores:**
   - Pulsador "Sí" -> GPIO 4
   - Pulsador "No" -> GPIO 15

3. **Servomotores:**
   - Servo de la cúspide -> GPIO 13
   - Servo de la boca -> GPIO 14

4. **Audio:**
   - Salida de audio -> GPIO 12

## Diagrama de Conexiones
![image](https://github.com/user-attachments/assets/9d7e7700-d046-4db9-811e-4e0fd3a52e68)


## Configuración y Uso

1. Carga los archivos de audio en la tarjeta SD.
2. Configura las credenciales WiFi en el código si deseas utilizar OTA.
3. Compila y carga el código en tu ESP32.
4. Enciende el dispositivo y sigue las instrucciones de voz.

Las contribuciones a este proyecto son bienvenidas. Por favor, abre un issue para discutir los cambios propuestos antes de hacer un pull request.
## Versiones
# Sombrero2.78
 Sombrero seleccionador
 Este proyecto se realizo como base para el proyecto sombrero seleccionador de Harry Potter
 versión EEST1 propuesto pòr el Prof. Matias Aldana.
 # Sombrero3.00 
//se realiza cambio a verificación por pulsadores estandard.
 # Sombrero 4.00
 // Se realizo agregado de servomotores para cuspide y boca del sombrero.
 // se cambio logica de pulsadores para funcionar con nivel activo en bajo.
