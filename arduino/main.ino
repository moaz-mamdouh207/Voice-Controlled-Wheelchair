
#include <SoftwareSerial.h>

SoftwareSerial mySerial(10, 11); // RX, TX

// Right Motor (Driver 1)
#define PWM_R_R    5   // PWM Right (forward)
#define PWM_L_R    6   // PWM Left (backward)

// Left Motor (Driver 2)
#define PWM_R_L    9   // PWM Right (forward)
#define PWM_L_L    3   // PWM Left (backward)

// ===== Ultrasonic Sensor Pins =====
#define TRIG_PIN 12
#define ECHO_PIN 13

// ===== Speed (0-255) =====
#define SPEED 70

// ===== Ultrasonic Function =====
long getDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH);
  // Calculate distance in cm
  long distance = duration * 0.034 / 2;
  return distance;
}

void setup() {
  mySerial.begin(9600);    // UART from ESP32

  // Motor pins
  pinMode(PWM_R_R,  OUTPUT);
  pinMode(PWM_L_R,  OUTPUT);
  pinMode(PWM_R_L,  OUTPUT);
  pinMode(PWM_L_L,  OUTPUT);

  // Ultrasonic pins
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
}

// ===== Motor Functions =====
void moveForward() {
  analogWrite(PWM_R_R, SPEED);  analogWrite(PWM_L_R, 0);
  analogWrite(PWM_R_L, SPEED);  analogWrite(PWM_L_L, 0);
}

void moveBackward() {
  analogWrite(PWM_R_R, 0);  analogWrite(PWM_L_R, SPEED);
  analogWrite(PWM_R_L, 0);  analogWrite(PWM_L_L, SPEED);
}


void moveRight() {
  // Right wheel backward, Left wheel forward
  analogWrite(PWM_R_R, 0);     analogWrite(PWM_L_R, SPEED);  // Right wheel ← backward
  analogWrite(PWM_R_L, SPEED); analogWrite(PWM_L_L, 0);      // Left wheel → forward
}

void moveLeft() {
  // Left wheel backward, Right wheel forward
  analogWrite(PWM_R_R, SPEED); analogWrite(PWM_L_R, 0);      // Right wheel → forward
  analogWrite(PWM_R_L, 0);     analogWrite(PWM_L_L, SPEED);  // Left wheel ← backward
}

void stopMotors() {
  analogWrite(PWM_R_R, 0);  analogWrite(PWM_L_R, 0);
  analogWrite(PWM_R_L, 0);  analogWrite(PWM_L_L, 0);
}

void loop() {
  // 1. Check Distance
  long currentDistance = getDistance();

  // 2. Safety Logic: Override if distance < 30cm
  if (currentDistance > 0 && currentDistance < 30) {
    stopMotors();
    // Optional: You could add a 'delay' or ignore 'f' commands here
  } 
  else {
    // 3. Normal Control Logic
    if (mySerial.available()) {
      char cmd = mySerial.read();

      switch (cmd) {
        case 'f':
          moveForward();
          break;
        case 'b':
          moveBackward();
          break;
        case 'l':
          moveLeft();
          break;
        case 'r':
          moveRight();
          break;
        case 's':
          stopMotors();
          break;
      }
    }
  }
  
  delay(50); // Small delay to stabilize sensor readings
}