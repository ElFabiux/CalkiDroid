// Plotclock ESP32 Port - DRAW MODE INTEGRATED
// cc - by Johannes Heberlein 2014
// units: mm; microseconds; radians

// Activa SOLO UNO de estos modos a la vez:
//#define CALIBRATION       // Calibración mecánica
//#define TIME              // Reloj automático (19:38)
#define DRAW              // Modo Dibujo: lienzo HTML -> PC -> ESP32
//#define CALCULATOR        // Modo Calculador: voz -> Python -> solo resultado dibujado por ESP32

#if (defined(CALIBRATION) + defined(TIME) + defined(DRAW) + defined(CALCULATOR)) != 1
  #error "Activa SOLO UNO de estos modos: CALIBRATION, TIME, DRAW o CALCULATOR"
#endif

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

// Prototipos de los nuevos manejadores para separar los modos
void ensureServosAttached();
void eraseBoard();
void parkAndDetach();
void handleSerialDrawMode();
void handleSerialCalculatorMode();
void drawCalculatorResult(String text);
void drawResultChar(float bx, float by, char ch, float scale);
void drawLineChar(float x1, float y1, float x2, float y2);
bool isResultSupportedChar(char ch);

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
  // Modo Dibujo: conserva el comportamiento actual del lienzo HTML.
  handleSerialDrawMode();

#elif defined(CALCULATOR)
  // Modo Calculador: recibe únicamente el resultado numérico ya calculado por Python.
  handleSerialCalculatorMode();
#endif
} 

// --- Manejadores de comunicación por Serial ---

void ensureServosAttached() {
  if (!servo1.attached()) servo1.attach(SERVOPINLIFT, 500, 2400);
  if (!servo2.attached()) servo2.attach(SERVOPINLEFT, 500, 2550); //Cambiar si la esquina inferior izquierda no llega bien el trazo
  if (!servo3.attached()) servo3.attach(SERVOPINRIGHT, 500, 2400);
}

void eraseBoard() {
  number(3, 3, 111, 1);
}

void parkAndDetach() {
  lift(2);
  drawTo(73.5, 44.5);
  lift(1);
  servo1.detach();
  servo2.detach();
  servo3.detach();
}

void handleSerialDrawMode() {
  // Modo interactivo: escucha comandos desde el script de Python por Serial.
  if (Serial.available() <= 0) return;

  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  if (cmd.length() == 0) return;

  ensureServosAttached();
  char action = cmd.charAt(0);

  if (action == 'E') {          // Comando: E (Erase / Borrar)
    eraseBoard();
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
    parkAndDetach();
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
  else {
    Serial.println("ERR");
  }
}

void handleSerialCalculatorMode() {
  // Comandos aceptados en modo calculadora:
  // E             -> borra la pizarra
  // C,resultado   -> dibuja solo el resultado numérico: 0123456789-.
  // P             -> parking y apaga servos
  if (Serial.available() <= 0) return;

  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  if (cmd.length() == 0) return;

  ensureServosAttached();

  if (cmd == "E") {
    eraseBoard();
    Serial.println("OK");
  }
  else if (cmd == "P") {
    parkAndDetach();
    Serial.println("OK");
  }
  else if (cmd.startsWith("C,")) {
    String text = cmd.substring(2);
    drawCalculatorResult(text);
    Serial.println("OK");
  }
  else {
    Serial.println("ERR");
  }
}

// --- Dibujo de resultado numérico para CALCULATOR ---

bool isResultSupportedChar(char ch) {
  // El resultado que envía Python solo debería traer números,
  // punto decimal o signo negativo. Se ignora cualquier otro símbolo.
  return (ch >= '0' && ch <= '9') || ch == '.' || ch == '-';
}

void drawLineChar(float x1, float y1, float x2, float y2) {
  lift(1);
  drawTo(x1, y1);
  lift(0);
  drawTo(x2, y2);
  lift(1);
}

void drawResultChar(float bx, float by, char ch, float scale) {
  if (ch >= '0' && ch <= '9') {
    number(bx, by, ch - '0', scale);
    return;
  }

  // Signo negativo para resultados como -4 o -2.5
  if (ch == '-') {
    drawLineChar(bx + 1 * scale, by + 10 * scale, bx + 11 * scale, by + 10 * scale);
    return;
  }

  // Punto decimal para resultados como 3.5
  if (ch == '.') {
    drawLineChar(bx + 5 * scale, by + 1 * scale, bx + 7 * scale, by + 1 * scale);
    drawLineChar(bx + 6 * scale, by + 0 * scale, bx + 6 * scale, by + 2 * scale);
    return;
  }
}

void drawCalculatorResult(String text) {
  // Limpia el texto recibido por seguridad. Aunque Python ya filtra el resultado,
  // aquí se vuelve a filtrar para que el ESP32 solo dibuje caracteres numéricos.
  String result = "";
  for (int i = 0; i < text.length(); i++) {
    char ch = text.charAt(i);
    if (isResultSupportedChar(ch)) {
      result += ch;
    }
  }

  if (result.length() == 0) return;

  // Área útil aproximada: 64 x 50 mm.
  // Como ahora se dibuja solo el resultado, se puede usar una escala más grande.
  const float scale = 0.72;
  const float startX = 4.0;
  const float line1Y = 25.0;
  const float line2Y = 6.0;
  const float stepX = 10.5;
  const int maxCols = 6;
  const int maxLines = 2;

  int col = 0;
  int line = 0;

  lift(1);
  for (int i = 0; i < result.length(); i++) {
    char ch = result.charAt(i);

    if (col >= maxCols) {
      col = 0;
      line++;
    }
    if (line >= maxLines) break;

    float bx = startX + (col * stepX);
    float by = (line == 0) ? line1Y : line2Y;
    drawResultChar(bx, by, ch, scale);
    col++;
  }
  lift(1);
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