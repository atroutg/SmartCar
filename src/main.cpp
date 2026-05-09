#include <Arduino.h>

/*
   ==================================================
      ROBOT AUTONOMO AMBIENTAL + REMOTEXY
   ==================================================
*/

//////////////////////////////////////////////
//           REMOTEXY CONFIG                //
//////////////////////////////////////////////

#define REMOTEXY_MODE__ESP32CORE_BLUETOOTH

#include <BluetoothSerial.h>
#define REMOTEXY_BLUETOOTH_NAME "SmartCar UNAD"

#include <RemoteXY.h>

#pragma pack(push, 1)
uint8_t const PROGMEM RemoteXY_CONF_PROGMEM[] =   // 197 bytes V19 
  { 255,2,0,12,0,190,0,19,0,0,0,83,109,97,114,116,67,97,114,32,
  85,78,65,68,0,31,1,106,200,1,1,5,0,130,241,34,129,41,27,17,
  71,3,41,30,30,60,0,2,1,167,135,0,0,0,0,0,0,72,66,0,
  0,160,64,0,0,128,63,0,0,0,0,24,84,101,109,112,101,114,97,116,
  117,114,97,0,71,38,42,30,30,60,0,2,1,167,135,0,0,0,0,0,
  0,200,66,0,0,32,65,0,0,160,64,0,0,0,0,24,72,117,109,101,
  100,97,100,0,71,72,42,30,30,92,3,2,1,167,135,0,0,0,0,0,
  240,127,69,0,0,32,65,0,0,160,64,0,0,128,63,24,65,105,114,101,
  0,135,0,0,0,0,0,0,150,68,78,0,0,150,68,0,64,28,69,36,
  0,64,28,69,0,72,153,69,5,23,98,60,60,1,2,26,31 };

struct {

  // INPUTS
  int8_t joystick_01_x;
  int8_t joystick_01_y;

  // OUTPUTS
  float Temperatura; // from 0 to 50
  float Humedad; // from 0 to 100
  float aire; // from 0 to 4095

  uint8_t connect_flag;

} RemoteXY;

#pragma pack(pop)

//////////////////////////////////////////////
//                LIBRERIAS                 //
//////////////////////////////////////////////

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>
#include <DHT.h>
#include <WiFi.h>
#include <WiFiClient.h>

//////////////////////////////////////////////
//                 LCD                      //
//////////////////////////////////////////////

LiquidCrystal_I2C lcd(0x27, 16, 2);

//////////////////////////////////////////////
//                 DHT22                    //
//////////////////////////////////////////////

#define DHTPIN 15
#define DHTTYPE DHT22

DHT dht(DHTPIN, DHTTYPE);

//////////////////////////////////////////////
//                 MQ2                      //
//////////////////////////////////////////////

const int MQ2_PIN = 34;

//////////////////////////////////////////////
//               MOTORES                    //
//////////////////////////////////////////////

// Motor derecho
const int IN1 = 26;
const int IN2 = 27;
const int ENA = 33;

// Motor izquierdo
const int IN3 = 14;
const int IN4 = 12;
const int ENB = 25;

//////////////////////////////////////////////
//             ULTRASONICO                  //
//////////////////////////////////////////////

const int TRIG = 4;
const int ECHO = 2;

//////////////////////////////////////////////
//                 SERVO                    //
//////////////////////////////////////////////

Servo sonarServo;

const int pinServo = 13;

//////////////////////////////////////////////
//                 BUZZER                    //
//////////////////////////////////////////////

const int buzzerPin = 18;

//////////////////////////////////////////////
//                  PWM                     //
//////////////////////////////////////////////

const int canalENA = 0;
const int canalENB = 1;

const int frecuenciaPWM = 500;
const int resolucionPWM = 8;

//////////////////////////////////////////////
//              VELOCIDADES                 //
//////////////////////////////////////////////

int velocidadIzq = 255;
int velocidadDer = 255;

//////////////////////////////////////////////
//          DISTANCIA MINIMA                //
//////////////////////////////////////////////

const int distanciaMinima = 25; // en centimetros

//////////////////////////////////////////////
//        VARIABLES AMBIENTALES             //
//////////////////////////////////////////////

float temperatura = 0;
float humedad = 0;
int gasValor = 0;

//////////////////////////////////////////////
//            MENSAJE LCD                   //
//////////////////////////////////////////////

String mensaje = "Estacion de variables ambientales movil";


/////////////////////////////////////////////////////
//                 WIFI + API                      //
/////////////////////////////////////////////////////

const char* ssid = "RedmiTrout";
const char* password = "unica123";
// API KEY DE THINGSPEAK
String apiKey = "R2BVK0H1306NGUTJ";

unsigned long ultimoEnvio = 0;
const long intervaloEnvio = 20000; // 20 segundos


/////////////////////////////////////////////////////
//            ENVIAR A THINGSPEAK                  //
/////////////////////////////////////////////////////

void enviarThingSpeak() {

  if (WiFi.status() == WL_CONNECTED) {

    WiFiClient client;

    if (client.connect("api.thingspeak.com", 80)) {

      String url = "/update?api_key=" + apiKey +
                   "&field1=" + String(temperatura) +
                   "&field2=" + String(humedad) +
                   "&field3=" + String(gasValor);

      client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                   "Host: api.thingspeak.com\r\n" +
                   "Connection: close\r\n\r\n");

      Serial.println("Datos enviados a ThingSpeak");

      client.stop();
    }
    else {

      Serial.println("Error conectando ThingSpeak");
    }
  }
}


/////////////////////////////////////////////////////
//               FUNCIONES LCD                     //
/////////////////////////////////////////////////////

void pausaRX(unsigned long tiempo) {

  unsigned long inicio = millis();

  while (millis() - inicio < tiempo) {

    RemoteXY_Handler();
  }
}


void scrollTexto(String texto, int fila, int velocidadMs) {

  for (int i = 0; i < texto.length() - 15; i++) {

    lcd.setCursor(0, fila);
    lcd.print(texto.substring(i, i + 16));

    pausaRX(velocidadMs);
  }
}


/////////////////////////////////////////////////////
//            ALARMA AUDIBLE - BUZZER              //
/////////////////////////////////////////////////////

void beepCorto() {

  digitalWrite(buzzerPin, HIGH);

  pausaRX(120);

  digitalWrite(buzzerPin, LOW);
}

void beepAlarma() {

  for (int i = 0; i < 3; i++) {

    digitalWrite(buzzerPin, HIGH);

    pausaRX(200);

    digitalWrite(buzzerPin, LOW);

    pausaRX(150);
  }
}

/////////////////////////////////////////////////////
//            INICIO DEL SISTEMA                 //
/////////////////////////////////////////////////////

void inicioSistema() {

  lcd.clear();

  scrollTexto(mensaje, 0, 180);

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("Sistema OK");

  lcd.setCursor(0, 1);
  lcd.print("Inicializando");

  pausaRX(2500);

  lcd.clear();
  beepCorto();
}



/////////////////////////////////////////////////////
//             LEER SENSORES                       //
/////////////////////////////////////////////////////

void leerSensores() {

  temperatura = dht.readTemperature();

  humedad = dht.readHumidity();

  gasValor = analogRead(MQ2_PIN);

  // ENVIAR A REMOTEXY
  RemoteXY.Temperatura = temperatura;
  RemoteXY.Humedad = humedad;

  //RemoteXY.aire = map(gasValor, 0, 4095, 0, 100);
  RemoteXY.aire = gasValor;

  Serial.println("===== MONITOREO =====");

  Serial.print("Temperatura: ");
  Serial.println(temperatura);

  Serial.print("Humedad: ");
  Serial.println(humedad);

  Serial.print("MQ2: ");
  Serial.println(gasValor);
}

/////////////////////////////////////////////////////
//             MOSTRAR LCD                         //
/////////////////////////////////////////////////////

void mostrarMonitoreo() {

  lcd.clear();

  lcd.setCursor(0, 0);

  lcd.print("T:");
  lcd.print(temperatura, 1);

  lcd.print(" H:");
  lcd.print(humedad, 0);

  lcd.setCursor(0, 1);

  lcd.print("Gas:");
  lcd.print(gasValor);
}

/////////////////////////////////////////////////////
//           MEDIR DISTANCIA                       //
/////////////////////////////////////////////////////

float medirDistancia() {

  digitalWrite(TRIG, LOW);

  delayMicroseconds(2);

  digitalWrite(TRIG, HIGH);

  delayMicroseconds(10);

  digitalWrite(TRIG, LOW);

  long duracion = pulseIn(ECHO, HIGH, 30000);

  if (duracion == 0) {

    return 999;
  }

  return duracion * 0.034 / 2;
}

/////////////////////////////////////////////////////
//                SERVO SONAR                     //
/////////////////////////////////////////////////////

void centrarServo() {

  sonarServo.write(90);

  pausaRX(300);
}

float mirarIzquierda() {

  sonarServo.write(150);

  pausaRX(500);

  return medirDistancia();
}

float mirarDerecha() {

  sonarServo.write(30);

  pausaRX(500);

  return medirDistancia();
}

/////////////////////////////////////////////////////
//              CONTROL MOTORES                    //
/////////////////////////////////////////////////////

void detenerMotores() {

  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);

  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);

  ledcWrite(canalENA, 0);
  ledcWrite(canalENB, 0);
}

void motorAdelante() {

  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);

  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);

  ledcWrite(canalENA, velocidadIzq);
  ledcWrite(canalENB, velocidadDer);
}

void motorAtras() {

  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);

  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);

  ledcWrite(canalENA, velocidadIzq);
  ledcWrite(canalENB, velocidadDer);
}

void girarIzquierda() {

  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);

  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);

  ledcWrite(canalENA, velocidadIzq);
  ledcWrite(canalENB, velocidadDer);
}

void girarDerecha() {

  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);

  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);

  ledcWrite(canalENA, velocidadIzq);
  ledcWrite(canalENB, velocidadDer);
}

/////////////////////////////////////////////////////
//              MODO MANUAL                        //
/////////////////////////////////////////////////////

void controlManual() {

  int x = RemoteXY.joystick_01_x;
  int y = RemoteXY.joystick_01_y;

  if (abs(x) < 15 && abs(y) < 15) {

    detenerMotores();

    return;
  }

  // ADELANTE
  if (y > 40) {

    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("ADELANTE");

    motorAdelante();

    pausaRX(500);
  }

  // ATRAS
  else if (y < -40) {

    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("ATRAS");

    motorAtras();

    pausaRX(500);
  }

  // IZQUIERDA
  else if (x < -40) {

    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("IZQUIERDA");

    girarIzquierda();

    pausaRX(500);
  }

  // DERECHA
  else if (x > 40) {

    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("DERECHA");

    girarDerecha();

    pausaRX(500);
  }

  mostrarMonitoreo();
}

/////////////////////////////////////////////////////
//             MODO AUTONOMO                       //
/////////////////////////////////////////////////////

void modoAutonomo() {

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("MODO");

  lcd.setCursor(0, 1);
  lcd.print("AUTONOMO");

  pausaRX(700);

  mostrarMonitoreo();

  centrarServo();

  float distanciaFrente = medirDistancia();
  
  

  Serial.print("Distancia: ");
  Serial.println(distanciaFrente);

  // ALERTA GAS
  if (gasValor > 2500) {

    detenerMotores();

    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("ALERTA GAS");

    lcd.setCursor(0, 1);
    lcd.print("Nivel Alto");

    beepAlarma();

    pausaRX(1500);

    mostrarMonitoreo();

    return;
  }

  // CAMINO LIBRE
  if (distanciaFrente > distanciaMinima) {

    motorAdelante();
  }

  // OBSTACULO
  else {

    detenerMotores();

    beepCorto();

    motorAtras();

    pausaRX(700);

    detenerMotores();

    float izquierda = mirarIzquierda();

    centrarServo();

    float derecha = mirarDerecha();

    centrarServo();

    if (izquierda > derecha) {

      lcd.clear();

      lcd.setCursor(0, 0);
      lcd.print("GIRO");

      lcd.setCursor(0, 1);
      lcd.print("IZQUIERDA");

      girarIzquierda();

      pausaRX(850);
    }
    else {

      lcd.clear();

      lcd.setCursor(0, 0);
      lcd.print("GIRO");

      lcd.setCursor(0, 1);
      lcd.print("DERECHA");

      girarDerecha();

      pausaRX(850);
    }

    detenerMotores();

    mostrarMonitoreo();
  }
}

/////////////////////////////////////////////////////
//                  SETUP                          //
/////////////////////////////////////////////////////

void setup() {

  Serial.begin(115200);

  //////////////////////////////////////////////////
  // WIFI
  //////////////////////////////////////////////////

  WiFi.begin(ssid, password);

  Serial.print("Conectando WiFi");

  while (WiFi.status() != WL_CONNECTED) {

    delay(500);

    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi conectado");
  Serial.println(WiFi.localIP());

  // REMOTEXY
  RemoteXY_Init();

  // LCD
  lcd.init();
  lcd.backlight();

  // DHT
  dht.begin();

  // MOTORES
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);

  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  // ULTRASONICO
  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);

  // MQ2
  pinMode(MQ2_PIN, INPUT);
  
  // BUZZER
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, LOW);

  // PWM
  ledcSetup(canalENA, frecuenciaPWM, resolucionPWM);
  ledcAttachPin(ENA, canalENA);

  ledcSetup(canalENB, frecuenciaPWM, resolucionPWM);
  ledcAttachPin(ENB, canalENB);

  // SERVO
  sonarServo.setPeriodHertz(50);

  sonarServo.attach(pinServo, 500, 2400);

  centrarServo();

  // INICIO
  inicioSistema();
}

/////////////////////////////////////////////////////
//                    LOOP                         //
/////////////////////////////////////////////////////

void loop() {

  // ACTUALIZAR REMOTEXY
  RemoteXY_Handler();

  // LEER SENSORES
  leerSensores();


  //////////////////////////////////////////////////
  // ENVIAR A THINGSPEAK
  //////////////////////////////////////////////////

  if (millis() - ultimoEnvio > intervaloEnvio) {

    enviarThingSpeak();

    ultimoEnvio = millis();
  }

  // MOSTRAR DATOS
  mostrarMonitoreo();

  //================================================
  // JOYSTICK CENTRADO = MODO AUTONOMO
  //================================================

  if (abs(RemoteXY.joystick_01_x) < 15 &&
      abs(RemoteXY.joystick_01_y) < 15) {

    modoAutonomo();
  }

  //================================================
  // CONTROL MANUAL
  //================================================
  else {

    controlManual();
  }

  pausaRX(100);
}

