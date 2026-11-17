// ============================================================
// HYBRIDFLOW / ZAQUAFLOW
// AUTOMATIC SOIL-BASED PUMP CONTROL
// RULE-BASED PUMP FAULT DETECTION & AUTO SHUTDOWN
// + SERIAL TELEMETRY FOR LAPTOP XGBOOST TESTING
//
// ESP32 pins:
// Soil moisture : GPIO 34
// Flow sensor   : GPIO 18
// ACS712        : GPIO 35
// Relay         : GPIO 19
//
// Serial commands (115200 baud):
// S = Manual Start Pump
// P = Manual Stop Pump
// R = Reset Fault Lock
//
// Fault classes:
// NORMAL, DRY_RUN, BLOCKAGE, CAVITATION
//
// Python telemetry format:
// ML,soil_moisture_pct,flow_lpm,current_a,pump_state,firmware_status
//
// IMPORTANT:
// - ML prediction runs on your laptop, not on this ESP32.
// - The ESP32 rule-based protection remains responsible for shutdown.
// - Keep the pump's water supply and outlet safely configured.
// ============================================================

#include <Arduino.h>
#include <math.h>

// ============================================================
// PIN DEFINITIONS
// ============================================================

#define SOIL_PIN        34
#define FLOW_PIN        18
#define CURRENT_PIN     35
#define RELAY_PIN       19

// ============================================================
// SENSOR CALIBRATION
// ============================================================

#define AIR_VALUE          3271
#define WATER_VALUE        1080
#define SENSITIVITY        0.100
#define PULSES_PER_LITRE   517.0

float zeroOffset = 0.0;

// ============================================================
// RELAY CONFIGURATION
// Change these if your relay module is active-LOW.
// ============================================================

#define PUMP_ON         HIGH
#define PUMP_OFF        LOW

// ============================================================
// AUTOMATIC SOIL CONTROL
// ============================================================

#define SOIL_PUMP_ON_THRESHOLD    40.0
#define SOIL_PUMP_OFF_THRESHOLD   70.0

// ============================================================
// RULE-BASED FAULT THRESHOLDS
// ============================================================

#define NORMAL_FLOW_MIN           1.80
#define LOW_FLOW_THRESHOLD        0.50
#define DRY_RUN_CURRENT_MAX       0.72
#define BLOCKAGE_CURRENT_MIN      0.73
#define CAVITATION_CURRENT_MIN    0.73

// ============================================================
// TIMING
// ============================================================

#define STARTUP_DELAY             5000UL
#define FAULT_CONFIRM_TIME        5000UL
#define SAMPLE_INTERVAL           1000UL

// ============================================================
// GLOBAL VARIABLES
// ============================================================

volatile unsigned long flowPulses = 0;

unsigned long lastFlowCalculation = 0;
unsigned long lastSampleTime = 0;

unsigned long pumpStartTime = 0;
unsigned long faultStartTime = 0;

float flowRate = 0.0;
float totalLitres = 0.0;
float currentA = 0.0;
float soilMoisture = 0.0;

bool pumpRunning = false;
bool faultLocked = false;

String currentFault = "NONE";

// ============================================================
// FLOW SENSOR INTERRUPT
// ============================================================

void IRAM_ATTR flowPulse()
{
  flowPulses++;
}

// ============================================================
// SOIL MOISTURE READING
// ============================================================

float readSoilMoisture()
{
  int raw = analogRead(SOIL_PIN);

  float moisture =
      ((float)(AIR_VALUE - raw) /
       (float)(AIR_VALUE - WATER_VALUE)) * 100.0;

  moisture = constrain(moisture, 0.0, 100.0);

  return moisture;
}

// ============================================================
// ACS712 ZERO-CURRENT CALIBRATION
// Keep the pump OFF during startup calibration.
// ============================================================

void calibrateCurrentSensor()
{
  const int samples = 1000;
  long total = 0;

  Serial.println();
  Serial.println("================================");
  Serial.println(" CURRENT SENSOR CALIBRATION");
  Serial.println("================================");
  Serial.println("Make sure pump is OFF.");
  Serial.println("Calibrating...");

  delay(2000);

  for (int i = 0; i < samples; i++)
  {
    total += analogRead(CURRENT_PIN);
    delay(2);
  }

  zeroOffset = total / (float)samples;

  Serial.print("Auto Zero Offset: ");
  Serial.println(zeroOffset, 2);

  Serial.println("Calibration complete.");
  Serial.println("================================");
  Serial.println();
}

// ============================================================
// ACS712 CURRENT READING
// ============================================================

float readCurrent()
{
  const int samples = 100;
  long total = 0;

  for (int i = 0; i < samples; i++)
  {
    total += analogRead(CURRENT_PIN);
    delayMicroseconds(200);
  }

  float rawAverage = total / (float)samples;

  float voltageDifference =
      ((rawAverage - zeroOffset) * 3.3) / 4095.0;

  float current = voltageDifference / SENSITIVITY;

  if (fabs(current) < 0.05)
  {
    current = 0.0;
  }

  return fabs(current);
}

// ============================================================
// FLOW RATE CALCULATION
// ============================================================

void calculateFlow()
{
  unsigned long now = millis();
  unsigned long elapsed = now - lastFlowCalculation;

  if (elapsed >= 1000UL)
  {
    noInterrupts();
    unsigned long pulses = flowPulses;
    flowPulses = 0;
    interrupts();

    float litres = pulses / PULSES_PER_LITRE;

    totalLitres += litres;

    flowRate = litres * (60000.0 / elapsed);

    lastFlowCalculation = now;
  }
}

// ============================================================
// START PUMP
// ============================================================

void startPump()
{
  if (faultLocked)
  {
    Serial.println();
    Serial.println("!!! PUMP START BLOCKED !!!");
    Serial.println("Fault lock is active.");
    Serial.println("Press R to reset the fault.");
    Serial.println();
    return;
  }

  digitalWrite(RELAY_PIN, PUMP_ON);

  pumpRunning = true;
  pumpStartTime = millis();
  faultStartTime = 0;
  currentFault = "STARTING";

  Serial.println();
  Serial.println("================================");
  Serial.println("       PUMP STARTED");
  Serial.println("================================");
  Serial.println("Fault monitoring starts after 5 seconds.");
}

// ============================================================
// STOP PUMP
// ============================================================

void stopPump()
{
  digitalWrite(RELAY_PIN, PUMP_OFF);

  pumpRunning = false;
  faultStartTime = 0;
  currentFault = "NONE";

  Serial.println();
  Serial.println(">>> PUMP OFF <<<");
}

// ============================================================
// AUTOMATIC FAULT SHUTDOWN
// ============================================================

void faultShutdown(String fault)
{
  digitalWrite(RELAY_PIN, PUMP_OFF);

  pumpRunning = false;
  currentFault = fault;
  faultLocked = true;
  faultStartTime = 0;

  Serial.println();
  Serial.println("========================================");
  Serial.println("        !!! FAULT DETECTED !!!");
  Serial.println("========================================");

  Serial.print("FAULT: ");
  Serial.println(fault);

  Serial.println("Pump automatically turned OFF.");
  Serial.println("FAULT LOCK ACTIVE.");
  Serial.println("Press R to reset fault.");
  Serial.println("========================================");
  Serial.println();
}

// ============================================================
// RESET FAULT LOCK
// ============================================================

void resetFault()
{
  if (!faultLocked)
  {
    Serial.println();
    Serial.println("No fault lock is active.");
    return;
  }

  faultLocked = false;
  faultStartTime = 0;
  currentFault = "NONE";

  Serial.println();
  Serial.println("========================================");
  Serial.println("          FAULT RESET");
  Serial.println("========================================");
  Serial.println("Fault lock cleared.");
  Serial.println("Automatic soil control is active.");
  Serial.println("========================================");
  Serial.println();
}

// ============================================================
// RULE-BASED FAULT DETECTION
// ============================================================

void detectFault()
{
  if (!pumpRunning)
  {
    return;
  }

  unsigned long now = millis();

  // Startup protection
  if (now - pumpStartTime < STARTUP_DELAY)
  {
    currentFault = "STARTING";
    faultStartTime = 0;
    return;
  }

  // NORMAL
  if (flowRate >= NORMAL_FLOW_MIN)
  {
    currentFault = "NORMAL";
    faultStartTime = 0;
    return;
  }

  // CAVITATION
  if (flowRate >= LOW_FLOW_THRESHOLD &&
      flowRate < NORMAL_FLOW_MIN &&
      currentA >= CAVITATION_CURRENT_MIN)
  {
    if (currentFault != "CAVITATION")
    {
      currentFault = "CAVITATION";
      faultStartTime = now;
      Serial.println("WARNING: CAVITATION CONDITION");
    }

    if (now - faultStartTime >= FAULT_CONFIRM_TIME)
    {
      faultShutdown("CAVITATION");
    }

    return;
  }

  // VERY LOW FLOW: DRY RUN or BLOCKAGE
  if (flowRate < LOW_FLOW_THRESHOLD)
  {
    // DRY RUN
    if (currentA <= DRY_RUN_CURRENT_MAX)
    {
      if (currentFault != "DRY_RUN")
      {
        currentFault = "DRY_RUN";
        faultStartTime = now;
        Serial.println("WARNING: DRY RUN CONDITION");
      }

      if (now - faultStartTime >= FAULT_CONFIRM_TIME)
      {
        faultShutdown("DRY_RUN");
      }

      return;
    }

    // BLOCKAGE
    if (currentA >= BLOCKAGE_CURRENT_MIN)
    {
      if (currentFault != "BLOCKAGE")
      {
        currentFault = "BLOCKAGE";
        faultStartTime = now;
        Serial.println("WARNING: BLOCKAGE CONDITION");
      }

      if (now - faultStartTime >= FAULT_CONFIRM_TIME)
      {
        faultShutdown("BLOCKAGE");
      }

      return;
    }
  }

  // UNKNOWN
  currentFault = "UNKNOWN";
  faultStartTime = 0;
}

// ============================================================
// AUTOMATIC SOIL-BASED PUMP CONTROL
// ============================================================

void automaticSoilControl()
{
  // Never automatically restart while fault lock is active.
  if (faultLocked)
  {
    return;
  }

  // Soil too dry: start pump.
  if (!pumpRunning &&
      soilMoisture < SOIL_PUMP_ON_THRESHOLD)
  {
    Serial.println();
    Serial.println("================================");
    Serial.println(" AUTOMATIC PUMP START");
    Serial.println("================================");

    Serial.print("Soil Moisture: ");
    Serial.print(soilMoisture, 2);
    Serial.println("%");

    Serial.println("Soil below 40%.");
    Serial.println("Starting pump automatically.");

    startPump();
    return;
  }

  // Soil sufficiently wet: stop pump.
  if (pumpRunning &&
      soilMoisture >= SOIL_PUMP_OFF_THRESHOLD)
  {
    Serial.println();
    Serial.println("================================");
    Serial.println(" AUTOMATIC PUMP STOP");
    Serial.println("================================");

    Serial.print("Soil Moisture: ");
    Serial.print(soilMoisture, 2);
    Serial.println("%");

    Serial.println("Soil reached 70%.");
    Serial.println("Stopping pump automatically.");

    stopPump();
    return;
  }
}

// ============================================================
// MACHINE-READABLE TELEMETRY FOR PYTHON XGBOOST
// ============================================================

void sendMLTelemetry()
{
  // Exact format expected by live_zaquaflow_serial.py:
  // ML,soil_moisture_pct,flow_lpm,current_a,pump_state,firmware_status

  Serial.print("ML,");
  Serial.print(soilMoisture, 2);
  Serial.print(",");
  Serial.print(flowRate, 3);
  Serial.print(",");
  Serial.print(currentA, 3);
  Serial.print(",");
  Serial.print(pumpRunning ? "ON" : "OFF");
  Serial.print(",");
  Serial.println(currentFault);
}

// ============================================================
// SETUP
// ============================================================

void setup()
{
  Serial.begin(115200);
  delay(1000);

  pinMode(SOIL_PIN, INPUT);
  pinMode(CURRENT_PIN, INPUT);
  pinMode(FLOW_PIN, INPUT_PULLUP);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, PUMP_OFF);

  // ESP32 ADC configuration
  analogReadResolution(12);

  // Current sensor calibration: pump must be OFF.
  calibrateCurrentSensor();

  attachInterrupt(
      digitalPinToInterrupt(FLOW_PIN),
      flowPulse,
      RISING
  );

  lastFlowCalculation = millis();

  Serial.println();
  Serial.println("==========================================");
  Serial.println("       HYBRIDFLOW MONITORING SYSTEM");
  Serial.println("==========================================");

  Serial.println();
  Serial.println("COMMANDS:");
  Serial.println("S -> MANUAL START");
  Serial.println("P -> MANUAL STOP");
  Serial.println("R -> RESET FAULT LOCK");

  Serial.println();
  Serial.println("AUTOMATIC SOIL CONTROL:");

  Serial.print("Pump ON  when Soil < ");
  Serial.print(SOIL_PUMP_ON_THRESHOLD);
  Serial.println("%");

  Serial.print("Pump OFF when Soil >= ");
  Serial.print(SOIL_PUMP_OFF_THRESHOLD);
  Serial.println("%");

  Serial.println();
  Serial.println("FAULT DETECTION:");
  Serial.println("NORMAL");
  Serial.println("DRY_RUN");
  Serial.println("BLOCKAGE");
  Serial.println("CAVITATION");

  Serial.println();
  Serial.println("ML SERIAL TELEMETRY: ENABLED");
  Serial.println("Baud rate: 115200");
  Serial.println("SYSTEM READY");
  Serial.println();
}

// ============================================================
// MAIN LOOP
// ============================================================

void loop()
{
  // ----------------------------------------------------------
  // SERIAL COMMANDS
  // ----------------------------------------------------------

  if (Serial.available() > 0)
  {
    char command = (char)Serial.read();

    if (command >= 'a' && command <= 'z')
    {
      command = command - 'a' + 'A';
    }

    if (command == 'S')
    {
      if (!pumpRunning)
      {
        startPump();
      }
      else
      {
        Serial.println("Pump is already ON.");
      }
    }
    else if (command == 'P')
    {
      if (pumpRunning)
      {
        stopPump();
      }
      else
      {
        Serial.println("Pump is already OFF.");
      }
    }
    else if (command == 'R')
    {
      resetFault();
    }
  }

  // ----------------------------------------------------------
  // FLOW CALCULATION
  // ----------------------------------------------------------

  calculateFlow();

  // ----------------------------------------------------------
  // SENSOR SAMPLING
  // ----------------------------------------------------------

  unsigned long now = millis();

  if (now - lastSampleTime >= SAMPLE_INTERVAL)
  {
    lastSampleTime = now;

    soilMoisture = readSoilMoisture();
    currentA = readCurrent();

    // Existing local control and safety logic.
    automaticSoilControl();
    detectFault();

    // Send one machine-readable line for laptop inference.
    sendMLTelemetry();

    // Human-readable serial monitor output.
    Serial.print("Time: ");
    Serial.print(now);

    Serial.print(" | Soil: ");
    Serial.print(soilMoisture, 2);
    Serial.print("%");

    Serial.print(" | Current: ");
    Serial.print(currentA, 3);
    Serial.print(" A");

    Serial.print(" | Flow: ");
    Serial.print(flowRate, 3);
    Serial.print(" L/min");

    Serial.print(" | Pump: ");
    Serial.print(pumpRunning ? "ON" : "OFF");

    Serial.print(" | Status: ");
    Serial.print(currentFault);

    Serial.print(" | Lock: ");
    Serial.println(faultLocked ? "YES" : "NO");
  }
}
