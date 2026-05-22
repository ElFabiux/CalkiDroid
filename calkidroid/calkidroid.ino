// Plotclock ESP32 Port - DRAW MODE INTEGRATED
// cc - by Johannes Heberlein 2014
// units: mm; microseconds; radians

// Activa SOLO UNO de estos modos a la vez:
//#define CALIBRATION       // Calibración mecánica
//#define TIME              // Reloj automático (19:38)
#define DRAW              // Modo interactivo (Teléfono -> PC -> ESP32)

// Ajustes mecánicos ya calibrados
#define SERVOFAKTORLEFT 650
#define SERVOFAKTORRIGHT 605
#define SERVOLEFTNULL 2310
#define SERVORIGHTNULL 850

// Pines ESP32
#define SERVOPINLIFT  25 
#define SERVOPINLEFT  26
#define SERVOPINRIGHT 27

// Alturas del lápiz
#define LIFT0 1080 // Tocando la pizarra
#define LIFT1 925  // Levantado para moverse
#define LIFT2 725  // Levantado hacia Parking

// Dimensiones
#define L1 35
#define L2 55.1
#define L3 13.2
#define O1X 22
#define O1Y -25
#define O2X 47
#define O2Y -25

#include <TimeLib.h> 
#include <ESP32Servo.h> 

Servo servo1;  
Servo servo2;  
Servo servo3;  

volatile double lastX = 75;
volatile double lastY = 47.5;
int last_min = 0;

void setup() 
{ 
  Serial.begin(115200); // Esencial para la comunicación con Python
  
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  
  servo1.setPeriodHertz(50); 
  servo2.setPeriodHertz(50);
  servo3.setPeriodHertz(50);

  setTime(19, 38, 0, 0, 0, 0);

  drawTo(73.5, 44.5);
  lift(0);
  
  servo1.attach(SERVOPINLIFT, 500, 2400);  
  servo2.attach(SERVOPINLEFT, 500, 2550); //Cambiar si la esquina inferior izquierda no llega bien el trazo
  servo3.attach(SERVOPINRIGHT, 500, 2400);  
  delay(1000);
}

void loop() 
{ 
#ifdef CALIBRATION
  drawTo(-3, 29.2);
  delay(500);
  drawTo(74.1, 28);
  delay(500);

#elif defined(TIME)
  int i = 0;
  if (last_min != minute()) {
    if (!servo1.attached()) servo1.attach(SERVOPINLIFT, 500, 2400);
    if (!servo2.attached()) servo2.attach(SERVOPINLEFT, 500, 2400); //Cambiar si la esquina inferior izquierda no llega bien el trazo
    if (!servo3.attached()) servo3.attach(SERVOPINRIGHT, 500, 2400);

    lift(0);
    hour();
    while ((i+1)*10 <= hour()) i++;
    
    number(3, 3, 111, 1); 
    number(5, 25, i, 0.9);
    number(19, 25, (hour()-i*10), 0.9);
    number(28, 25, 11, 0.9); 

    i=0;
    while ((i+1)*10 <= minute()) i++;
    
    number(34, 25, i, 0.9);
    number(48, 25, (minute()-i*10), 0.9);
    
    lift(2);
    drawTo(74.2, 47.5); 
    lift(1);
    last_min = minute();

    servo1.detach();
    servo2.detach();
    servo3.detach();
  }

#elif defined(DRAW)
  // Modo interactivo: Escucha comandos desde el script de Python por Serial
  if (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.length() > 0) {
      
      // Asegurar que los servos estén encendidos
      if (!servo1.attached()) servo1.attach(SERVOPINLIFT, 500, 2400);
      if (!servo2.attached()) servo2.attach(SERVOPINLEFT, 500, 2550); //Cambiar si la esquina inferior izquierda no llega bien el trazo
      if (!servo3.attached()) servo3.attach(SERVOPINRIGHT, 500, 2400);

      char action = cmd.charAt(0);
      
      if (action == 'E') {          // Comando: E (Erase / Borrar)
        number(3, 3, 111, 1);
        Serial.println("OK");
      } 
      else if (action == 'U') {     // Comando: U (Up / Levantar lápiz)
        lift(1);
        Serial.println("OK");
      }
      else if (action == 'D') {     // Comando: D (Down / Bajar lápiz)
        lift(0);
        Serial.println("OK");
      }
      else if (action == 'P') {     // Comando: P (Park / Regresar y apagar)
        lift(2);
        drawTo(73.5, 44.5);
        lift(1);
        servo1.detach();            // Apaga motores en reposo
        servo2.detach();
        servo3.detach();
        Serial.println("OK");
      }
      else if (action == 'M') {     // Comando: M,x,y (Mover a coordenadas)
        int comma1 = cmd.indexOf(',');
        int comma2 = cmd.lastIndexOf(',');
        if (comma1 > 0 && comma2 > comma1) {
          float x = cmd.substring(comma1 + 1, comma2).toFloat();
          float y = cmd.substring(comma2 + 1).toFloat();
          drawTo(x, y);
        }
        Serial.println("OK");
      }
    }
  }
#endif
} 

// --- Mantén todas tus funciones (number, lift, bogenUZS, bogenGZS, drawTo, return_angle, set_XY) exactamente iguales abajo ---

void number(float bx, float by, int num, float scale) {
  switch (num) {
  case 0: drawTo(bx + 12 * scale, by + 6 * scale); lift(0); bogenGZS(bx + 7 * scale, by + 10 * scale, 10 * scale, -0.8, 6.7, 0.5); lift(1); break;
  case 1: drawTo(bx + 3 * scale, by + 15 * scale); lift(0); drawTo(bx + 10 * scale, by + 20 * scale); drawTo(bx + 10 * scale, by + 0 * scale); lift(1); break;
  case 2: drawTo(bx + 2 * scale, by + 12 * scale); lift(0); bogenUZS(bx + 8 * scale, by + 14 * scale, 6 * scale, 3, -0.8, 1); drawTo(bx + 1 * scale, by + 0 * scale); drawTo(bx + 12 * scale, by + 0 * scale); lift(1); break;
  case 3: drawTo(bx + 2 * scale, by + 17 * scale); lift(0); bogenUZS(bx + 5 * scale, by + 15 * scale, 5 * scale, 3, -2, 1); bogenUZS(bx + 5 * scale, by + 5 * scale, 5 * scale, 1.57, -3, 1); lift(1); break;
  case 4: drawTo(bx + 10 * scale, by + 0 * scale); lift(0); drawTo(bx + 10 * scale, by + 20 * scale); drawTo(bx + 2 * scale, by + 6 * scale); drawTo(bx + 12 * scale, by + 6 * scale); lift(1); break;
  case 5: drawTo(bx + 2 * scale, by + 5 * scale); lift(0); bogenGZS(bx + 5 * scale, by + 6 * scale, 6 * scale, -2.5, 2, 1); drawTo(bx + 5 * scale, by + 20 * scale); drawTo(bx + 12 * scale, by + 20 * scale); lift(1); break;
  case 6: drawTo(bx + 2 * scale, by + 10 * scale); lift(0); bogenUZS(bx + 7 * scale, by + 6 * scale, 6 * scale, 2, -4.4, 1); drawTo(bx + 11 * scale, by + 20 * scale); lift(1); break;
  case 7: drawTo(bx + 2 * scale, by + 20 * scale); lift(0); drawTo(bx + 12 * scale, by + 20 * scale); drawTo(bx + 2 * scale, by + 0); lift(1); break;
  case 8: drawTo(bx + 5 * scale, by + 10 * scale); lift(0); bogenUZS(bx + 5 * scale, by + 15 * scale, 5 * scale, 4.7, -1.6, 1); bogenGZS(bx + 5 * scale, by + 5 * scale, 5 * scale, -4.7, 2, 1); lift(1); break;
  case 9: drawTo(bx + 9 * scale, by + 11 * scale); lift(0); bogenUZS(bx + 7 * scale, by + 15 * scale, 5 * scale, 4, -0.5, 1); drawTo(bx + 5 * scale, by + 0); lift(1); break;
  case 111: lift(0); drawTo(73.5, 44.5); drawTo(65, 40); drawTo(65, 49); drawTo(5, 49); drawTo(5, 45); drawTo(65, 45); drawTo(65, 40); drawTo(5, 40); drawTo(5, 35); drawTo(65, 35); drawTo(65, 30); drawTo(5, 30); drawTo(5, 25); drawTo(65, 25); drawTo(65, 20); drawTo(5, 20); drawTo(60, 44); drawTo(73.5, 44.5);; lift(2); break;
  case 11: drawTo(bx + 5 * scale, by + 15 * scale); lift(0); bogenGZS(bx + 5 * scale, by + 15 * scale, 0.1 * scale, 1, -1, 1); lift(1); drawTo(bx + 5 * scale, by + 5 * scale); lift(0); bogenGZS(bx + 5 * scale, by + 5 * scale, 0.1 * scale, 1, -1, 1); lift(1); break;
  }
  //case 111: lift(0); drawTo(73.5, 44.5); drawTo(65, 40); HAY QUE CALIBRAR ESTO PARA QUE SALGA MAS FINO DEL PARK DEL BORRADOR
}

void lift(char lift) {
  int targetLift = LIFT0;
  switch (lift) {
    case 0: targetLift = LIFT0; break;
    case 1: targetLift = LIFT1; break;
    case 2: targetLift = LIFT2; break;
  }
  servo1.writeMicroseconds(targetLift);
  delay(80); 
}

void bogenUZS(float bx, float by, float radius, int start, int ende, float sqee) {
  float inkr = -0.15; float count = 0;
  do { drawTo(sqee * radius * cos(start + count) + bx, radius * sin(start + count) + by); count += inkr; } while ((start + count) > ende);
}

void bogenGZS(float bx, float by, float radius, int start, int ende, float sqee) {
  float inkr = 0.15; float count = 0;
  do { drawTo(sqee * radius * cos(start + count) + bx, radius * sin(start + count) + by); count += inkr; } while ((start + count) <= ende);
}

void drawTo(double pX, double pY) {
  double dx, dy, c; int i;
  dx = pX - lastX; dy = pY - lastY;
  c = floor(sqrt(dx * dx + dy * dy) / 1.3);
  if (c < 1) c = 1;
  for (i = 0; i <= c; i++) {
    set_XY(lastX + (i * dx / c), lastY + (i * dy / c));
    delay(4); 
  }
  lastX = pX; lastY = pY;
}

double return_angle(double a, double b, double c) { return acos((a * a + c * c - b * b) / (2 * a * c)); }

void set_XY(double Tx, double Ty) {
  double dx, dy, c, a1, a2, Hx, Hy;
  dx = Tx - O1X; dy = Ty - O1Y;
  c = sqrt(dx * dx + dy * dy);  
  a1 = atan2(dy, dx); 
  a2 = return_angle(L1, L2, c);
  servo2.writeMicroseconds(floor(((a2 + a1 - PI) * SERVOFAKTORLEFT) + SERVOLEFTNULL));
  a2 = return_angle(L2, L1, c);
  Hx = Tx + L3 * cos((a1 - a2 + 0.621) + PI); 
  Hy = Ty + L3 * sin((a1 - a2 + 0.621) + PI);
  dx = Hx - O2X; dy = Hy - O2Y;
  c = sqrt(dx * dx + dy * dy);
  a1 = atan2(dy, dx);
  a2 = return_angle(L1, (L2 - L3), c);
  servo3.writeMicroseconds(floor(((a1 - a2) * SERVOFAKTORRIGHT) + SERVORIGHTNULL));
}