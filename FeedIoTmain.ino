#include <ArduinoJson.h>
#include <ESP32Servo.h>
#include <UniversalTelegramBot.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

// Credenciales del cliente wifi
#define SSID "Redmi_note88"
#define PASSWORD "1234Nothe8"

// Inicializar el BOT de Telegram
#define FeedIoTBot "6781969640:AAHhHr9ah3bfmXNUFX_rgnd6v0vhQELuSWI"
#define CHAT_ID "1143709720"

// Objetos para el cliente wifi y el BOT de Telegram
WiFiClientSecure client;
UniversalTelegramBot bot(FeedIoTBot, client);
Servo servo1;

static const int ledPin = 32;
static const int foodPin = 33;
static const int servoPin = 13;
static const int tankPin = 25;
static const int nofoodPin = 26;
int bot_delay = 1000;
time_t now;
unsigned long horautotime = 0;
unsigned long minautotime = 1;
struct tm timeinfo;
unsigned long lastTimeBotRan;
unsigned long lastTimeTankRan;
time_t lastAutoTime;
unsigned long lastTimeReport;
unsigned char Mode;
bool PetBtn = true;
volatile bool PetBtnPressed = false;
bool restState = 0;
bool ledState = LOW;
bool tankState;
bool noFood;
String interruptTimes[5];
int idx = 0;
int cnt = 0;
int maxcnt = 20;

// Maneja lo que sucede cuando se reciben nuevos mensajes
void manejarNuevosMensajes(int numNewMessages) {
  // Serial.println("Manejando nuevo mensaje");
  // Serial.println(String(numNewMessages));

  for (int i = 0; i < numNewMessages; i++) {
    // ID de chat del solicitante
    String chat_id = bot.messages[i].chat_id;
    if (chat_id != CHAT_ID && chat_id != "6263349131") {
      bot.sendMessage(chat_id, "Usuario no autorizado", "");
      continue;
    }

    // Imprime el mensaje en telegram
    String user_prompt = bot.messages[i].text;
    Serial.println(user_prompt);

    String nombre_usuario = bot.messages[i].from_name;
    // Commando inicio
    if (user_prompt == "/inicio") {
      String bienvenida = "Bienvenido, " + nombre_usuario + ".\n";
      bienvenida += "Comandos disponibles\n\n";
      bienvenida += "Envía /conf para ver la configuración \n";
      bienvenida += "Envía /disp para dispensar comida \n";
      bienvenida += "Envía /auto para activar el modo automático \n";
      bienvenida += "Envía /manual para activar el modo manual \n";
      bienvenida +=
          "Envía /btn para activar o desactivar el botón de mascota \n";
      bienvenida += "Envía /por para ajustar las porciones \n";
      bot.sendMessage(chat_id, bienvenida, "");
    }

    // Menu for the configuration
    if (user_prompt == "/conf") {
      String configuracion = "Configuración actual\n\n";
      configuracion += "Modo de Operación:\t" +
                       String((Mode == 2) ? "Auto" : "Manual") + "\n";
      configuracion +=
          "Botón de mascota:\t" + String(PetBtn ? "On" : "Off") + "\n";
      configuracion +=
          "Alimento:\t" + String(tankState ? "Disponible" : "Vacio") + "\n";
      configuracion += "Plato:\t" + String(noFood ? "Vacio" : "Lleno") + "\n";
      configuracion += "Se dispensa cada:\t" + String(horautotime) + " horas " +
                       String(minautotime) + " minutos\n";
      configuracion += "Limite de porciones:\t" + String(maxcnt) + "\n";
      bot.sendMessage(chat_id, configuracion, "");
    }

    // instant manual dispense
    if (user_prompt == "/disp") {
      if (tankState) {
        if (noFood) {
          bot.sendMessage(chat_id, "Activado directo", "");
          servoMove();
        } else {
          bot.sendMessage(chat_id, "Plato lleno!!", "");
          bot.sendMessage(chat_id, "Entrando en descanzo", "");
          restState = 1;
        }
      } else {
        bot.sendMessage(chat_id, "Comida no disponible!!", "");
        bot.sendMessage(chat_id, "Entrando en descanzo", "");
        restState = 1;
      }
    }

    // Auto mode handler
    if (user_prompt == "/auto") {
      modoAutomatico();
      bot.sendMessage(chat_id, "Modo automático activado", "");
    }
    if (user_prompt == "/manual") {
      modoManual();
      bot.sendMessage(chat_id, "Modo manual activado", "");
    }
    // Activación y desactivación del Botón
    if (user_prompt == "/btn") {
      PetBtn = !PetBtn;
      bot.sendMessage(chat_id,
                      "Botón de Mascota " + String(PetBtn ? "On" : "Off"), "");
    }

    // manual timer promt handler
    if (user_prompt == "/tim") {
      bot.sendMessage(chat_id, "Escribe cuando quieres que se sirva la comida",
                      "");
      int newMessages = bot.getUpdates(bot.last_message_received + 1);
      while (newMessages == 0 || bot.messages[newMessages - 1].text == "") {
        delay(1000);
        newMessages = bot.getUpdates(bot.last_message_received + 1);
      }

      int tdelay = bot.messages[newMessages - 1].text.toInt();
      bot.sendMessage(chat_id, "Activando en " + String(tdelay) + " segundos",
                      "");
      vTaskDelay(tdelay * 1000);

      if (tankState) {
        if (noFood) {
          bot.sendMessage(chat_id, "Activación temporizada", "");
          servoMove();
        } else {
          bot.sendMessage(chat_id, "Plato lleno", "");
          bot.sendMessage(chat_id, "Entrando en descanzo", "");
          restState = 1;
        }
      } else {
        bot.sendMessage(chat_id, "Comida no disponible!!", "");
        bot.sendMessage(chat_id, "Entrando en descanzo", "");
        restState = 1;
      }
    }

    // comando para ajustar el modo automático
    if (user_prompt == "/autoctl") {
      bot.sendMessage(
          chat_id, "Escribe el intervalo para dispensar (hora:minutos): ", "");

      int newMessages = bot.getUpdates(bot.last_message_received + 1);
      while (newMessages == 0 || bot.messages[newMessages - 1].text == "") {
        delay(1000);
        newMessages = bot.getUpdates(bot.last_message_received + 1);
      }

      String timeString = bot.messages[newMessages - 1].text;
      String hourString = timeString.substring(0, timeString.indexOf(":"));
      String minuteString = timeString.substring(timeString.indexOf(":") + 1);
      horautotime = hourString.toInt();
      minautotime = minuteString.toInt();

      bot.sendMessage(chat_id,
                      "Dispensando cada " + hourString + " horas y " +
                          minuteString + " minutos",
                      "");
    }

    // Ajuste de la cantida de porciones
    if (user_prompt == "/por") {
      bot.sendMessage(chat_id, "Escribe el limite de porciones: ", "");

      int newMessages = bot.getUpdates(bot.last_message_received + 1);
      while (newMessages == 0 || bot.messages[newMessages - 1].text == "") {
        delay(1000);
        newMessages = bot.getUpdates(bot.last_message_received + 1);
      }

      String porString = bot.messages[newMessages - 1].text;
      maxcnt = porString.toInt();

      bot.sendMessage(chat_id,
                      "Limite de porciones: " + porString + " establecido", "");
    }
  }
}

void modoAutomatico() {
  Mode = 2;
  lastAutoTime = time(&now);
  Serial.println("Modo automático activado");
  bot.sendMessage(CHAT_ID, "usa /autoctl para ajustar el intervalo", "");
}

void modoManual() {
  Mode = 1;
  Serial.println("Modo manual activado");
  bot.sendMessage(CHAT_ID, "usa /tim para preogramar la comida", "");
}

void servoMove() {
  // servo1.attach(servoPin); // Attach servo to GPIO pin 13
  servo1.write(0); // Turn Servo Left to 0 degrees
  vTaskDelay(200);
  servo1.write(90); // Turn Servo Right to 180 degrees
  vTaskDelay(500);
  servo1.write(0);
  vTaskDelay(300);
  // servo1.detach(); // Detach the servo
}

//  keep the function code in the Internal RAM
void IRAM_ATTR handleInterrupt() {
  if (PetBtn) {
    PetBtnPressed = true;
  }
}

void setup() {

  // Variables de tiempo
  unsigned long startTime = millis();
  lastTimeBotRan = startTime;
  lastTimeTankRan = startTime;
  lastTimeReport = startTime;

  pinMode(ledPin, OUTPUT);
  pinMode(foodPin, INPUT_PULLUP);
  pinMode(tankPin, INPUT_PULLUP);
  pinMode(nofoodPin, INPUT);

  Serial.begin(115200);

  // Setup process for peripherals
  tankState = digitalRead(tankPin);
  noFood = digitalRead(nofoodPin);

  if (tankState) {
    digitalWrite(ledPin, HIGH);
  } else {
    digitalWrite(ledPin, LOW);
  }

  // Allow allocation of all timers
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  servo1.setPeriodHertz(50);          // Standard 50hz servo
  servo1.attach(servoPin, 500, 2400); // Attach servo to GPIO pin 13

  // Enable the interrupt pin
  attachInterrupt(digitalPinToInterrupt(foodPin), handleInterrupt, FALLING);

  // Conectarse a la red Wi-Fi
  WiFi.mode(WIFI_STA);
  Serial.print("Conectando a ");
  Serial.println(SSID);
  WiFi.begin(SSID,PASSWORD);

  while (WiFi.status() != WL_CONNECTED) { //&& elapsedTime < 40000) {
    delay(500);
    Serial.print(".");
    digitalWrite(ledPin, ledState);
    ledState = !ledState;
  }

  if (WiFi.status() == WL_CONNECTED) {
    for (int i = 0; i < 3; i++) {
      digitalWrite(ledPin, HIGH);
      delay(100);
      digitalWrite(ledPin, LOW);
      delay(100);
    }
    Serial.println("\nConectado al WiFi!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }

  // Telegram
  client.setCACert(
      TELEGRAM_CERTIFICATE_ROOT); // Add root certificate for api.telegram.org

  // CLock from NTP
  Serial.print("Retrieving time: ");
  configTime(-5 * 3600, 0, "time.google.com");
  while (now < 24 * 3600) {
    Serial.print(".");
    delay(100);
    now = time(nullptr);
  }

  char formattedDate[20];
  char formattedTime[20];
  strftime(formattedDate, sizeof(formattedDate), "%d-%m-%Y", localtime(&now));
  strftime(formattedTime, sizeof(formattedTime), "%H:%M:%S", localtime(&now));

  Serial.println("\nCurrent datetime: " + String(formattedDate) + String(" ") +
                 String(formattedTime));

  // mensaje de finalización del setup
  Serial.println("Working....");
  bot.sendMessage(CHAT_ID,
                  "\nAlimentador Encendido\nActivado el: " +
                      String(formattedDate) + " " + String(formattedTime) +
                      "\nEnvía /inicio para ver los comandos disponibles",
                  "");
}

void loop() {

  // comprueba el estado del tanque y el plato cada 1 segundo
  if (millis() > lastTimeTankRan + 1000) {
    tankState = digitalRead(tankPin);
    noFood = digitalRead(nofoodPin);

    if (tankState) {
      digitalWrite(ledPin, HIGH);
      restState = 0;
    } else {
      digitalWrite(ledPin, LOW);
    }
    lastTimeTankRan = millis();
  }

  // ------------------------------------------//
  // Rutina más importante timepo real --------//

  // comprueba el estado del botón de mascota
  if (PetBtnPressed) {
    PetBtnPressed = false;
    Serial.println("Activated by pet");
    if (tankState) {
      if (noFood) {
        servoMove();
        getLocalTime(&timeinfo);
        char buffer[10];
        strftime(buffer, sizeof(buffer), "%H:%M:%S", &timeinfo);
        interruptTimes[idx] = String(buffer);
        idx = (idx + 1) % 5;
        lastTimeReport = millis();
      } else {
        bot.sendMessage(CHAT_ID, "Plato lleno", "");
        bot.sendMessage(CHAT_ID, "Entrando en descanzo", "");
        restState = 1;
      }
    } else {
      bot.sendMessage(CHAT_ID, "Comida no disponible!!", "");
      bot.sendMessage(CHAT_ID, "Entrando en descanzo", "");
      restState = 1;
    }
  }

  // ----------------------------------------//
  // Rutinas  de mayor duración -------------//
  // Reporte de interrupciones cada 60 segundos
  if ((millis() > lastTimeReport + 60000) && (!PetBtnPressed)) {
    String reportmsgs = "";

    // Test the interruptTimes array is empty
    if (!(interruptTimes[0] == "")) {
      // Send the latest interrupt times in a single message
      for (int i = 0; i < 5; i++) {
        if (interruptTimes[i] == "") {
          continue;
        }

        String idxTime = interruptTimes[i];
        reportmsgs += "Activación física a: " + idxTime + "\n";
        interruptTimes[i] = "";
      }

      if (!reportmsgs.isEmpty()) {
        bot.sendMessage(CHAT_ID, reportmsgs, "");
        // lastTimeReport = millis();
        Serial.println("Reporte enviado");
        idx = 0;
      }
    }
    lastTimeReport = millis();
  }

  // ------------------------------------------------------//
  // Modo automático

  if ((Mode == 2) && (restState == 0) && (!PetBtnPressed) && (cnt < maxcnt)) {
    // Serial.println("auto mode checking");
    //  check the current time and compare whit horautotime and minautotime
    if (time(&now) > lastAutoTime + (horautotime * 3600 + minautotime * 60)) {
      Serial.println("auto triggered");
      // Serial.println(now);
      // Serial.print(String(lastAutoTime + (horautotime * 3600 + minautotime
      // * 60)));
      if (tankState) {
        if (noFood) {
          servoMove();
          bot.sendMessage(CHAT_ID,
                          "Se ha dispensado comida " + String(++cnt) + " veces",
                          "");
          lastAutoTime = time(&now);
          if (cnt == maxcnt) {
            bot.sendMessage(CHAT_ID, "Limite de porciones alcanzado", "");
            bot.sendMessage(CHAT_ID, "Entrando en descanzo", "");
            restState = 1;
          }
        } else {
          bot.sendMessage(CHAT_ID, "Plato lleno", "");
          bot.sendMessage(CHAT_ID, "Entrando en descanzo", "");
          restState = 1;
        }
      } else {
        bot.sendMessage(CHAT_ID, "Comida no disponible!!", "");
        bot.sendMessage(CHAT_ID, "Entrando en descanzo", "");
        restState = 1;
      }
    }
  }

  // This takes five seconds to run
  // comprueba si hay nuevos mensajes cada segundo y envía una respuesta
  if ((millis() > lastTimeBotRan + bot_delay) && (!PetBtnPressed)) {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while (numNewMessages) {
      Serial.println("got response");
      manejarNuevosMensajes(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    // I want to print the actual current time from here
    lastTimeBotRan = millis();
  }

  // unsigned long start = millis();
  // unsigned long stop;
  // time(&now);
  // Serial.println(ctime(&now));
  // stop = millis();
  // mesure the execution of this functions and print it
  // Serial.println("Tiempo de ejecución: " + String(stop - start) + " ms");
}
