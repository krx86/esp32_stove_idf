#pragma once
#include <esp_log.h>
#include <string>

void damperControlInit();
void damperControlLoop();
int average(const int* arr, int start, int count);
bool WoodFilled(int CurrentTemp);
void moveServoToDamper();
void startDamperControlTask();

void checkLowTempDeepSleep();
void setDamperTarget(int newTarget);

// Buzzer funkcijas
void initBuzzer();
void playWarningSound();
void startBuzzerSound();
void stopBuzzerSound();
void buzzerTask(void *pvParameters);

// Manual mode stub for VVC
bool is_manual_damper_mode();

extern int damper;
extern int minDamper;
extern int maxDamper;
extern int zeroDamper;
extern int kP;  // PID regulatora koeficients
extern float tauI;  // PID integrāla komponentes laika konstante
extern float tauD;  // PID diferenciālā komponentes laika konstante
extern float kI;  // PID integrāla koeficients
extern float kD;  // PID diferenciālā koeficients
extern float endTrigger;
extern float refillTrigger;
extern std::string messageDamp;
extern bool servoMoving;
extern float errP;
extern float errI;
extern float errD;
extern float errOld;

// Servo parametri
extern int servoAngle;  // Servo motora maksimālais leņķis
extern int servoOffset;  // Servo pozīcijas nobīde
extern int servoStepInterval;  // Servo kustības solis milisekundēs

// Buzzer pins un statuss
extern int buzzer;
extern bool isWarningActive;

// Zemas temperatūras pārbaudes statuss
extern bool lowTempCheckActive;
extern unsigned long LOW_TEMP_TIMEOUT;
