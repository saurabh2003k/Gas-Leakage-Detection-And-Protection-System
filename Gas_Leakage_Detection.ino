#define BLYNK_TEMPLATE_ID "YOUR_TEMPLATE_ID"
#define BLYNK_TEMPLATE_NAME "LPG Safety System"
#define BLYNK_AUTH_TOKEN "YOUR_BLYNK_AUTH_TOKEN"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>

// ---------------- WIFI ----------------
char ssid[] = "YOUR_WIFI_NAME";
char pass[] = "YOUR_WIFI_PASSWORD";

// ---------------- PINS ----------------
#define MQ2_PIN 34
#define BUZZER_PIN 25
#define RELAY_PIN 26
#define SERVO_PIN 27

// ---------------- SERVO (ESP32 CORE 3.x) ----------------
// Convert angle (0-180) to PWM duty
uint32_t angleToDuty(int angle)
{
    // 50 Hz -> 20 ms period
    // Servo pulse: 0.5 ms to 2.5 ms
    int minDuty = 1638;
    int maxDuty = 8192;

    return map(angle, 0, 180, minDuty, maxDuty);
}

// Set servo angle
void setServoAngle(int angle)
{
    ledcWrite(SERVO_PIN, angleToDuty(angle));
    Blynk.virtualWrite(V2, angle);
}

// ---------------- VARIABLES ----------------
int gasThreshold = 100;
int gasValue = 0;

bool gasDetected = false;
bool alertSent = false;

bool fanState = LOW;
bool buzzerState = LOW;

// ---------------- NON-BLOCKING TIMER ----------------
unsigned long previousMillis = 0;
int systemStep = 0;

// Step sequence:
// 0 = Idle
// 1 = Servo OFF
// 2 = Wait 1 sec -> Fan ON
// 3 = Wait 1 sec -> Buzzer ON + Notification
// 4 = Done

// ---------------- SETUP ----------------
void setup()
{
    Serial.begin(115200);

    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(RELAY_PIN, OUTPUT);

    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(RELAY_PIN, LOW);

    // ESP32 Core 3.x PWM setup
    // Pin, frequency, resolution
    ledcAttach(SERVO_PIN, 50, 16);

    // Gas ON initially
    setServoAngle(0);

    Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
}

// ---------------- MAIN LOOP ----------------
void loop()
{
    Blynk.run();

    // Read MQ-2 sensor
    gasValue = analogRead(MQ2_PIN);

    // Send gas value to Blynk
    Blynk.virtualWrite(V0, gasValue);

    // ---------------- GAS DETECTION LOGIC ----------------
    if (gasValue > gasThreshold)
    {
        Blynk.virtualWrite(V1, "DANGER");

        if (!gasDetected)
        {
            gasDetected = true;
            systemStep = 1;
            previousMillis = millis();
        }
    }
    else
    {
        Blynk.virtualWrite(V1, "SAFE");

        gasDetected = false;
        alertSent = false;
        systemStep = 0;
    }

    // ---------------- NON-BLOCKING SEQUENCE ----------------
    unsigned long currentMillis = millis();

    switch (systemStep)
    {
        case 0:
            // Idle
            break;

        case 1:
            // STEP 1: Turn OFF gas (servo 90 degrees)
            setServoAngle(90);

            previousMillis = currentMillis;
            systemStep = 2;
            break;

        case 2:
            // STEP 2: After 1 sec -> Fan ON
            if (currentMillis - previousMillis >= 1000)
            {
                digitalWrite(RELAY_PIN, HIGH);
                fanState = HIGH;
                Blynk.virtualWrite(V3, fanState);

                previousMillis = currentMillis;
                systemStep = 3;
            }
            break;

        case 3:
            // STEP 3: After 1 sec -> Buzzer ON + Notification
            if (currentMillis - previousMillis >= 1000)
            {
                digitalWrite(BUZZER_PIN, HIGH);
                buzzerState = HIGH;
                Blynk.virtualWrite(V4, buzzerState);

                if (!alertSent)
                {
                    Blynk.logEvent("gas_alert", "Gas Leak Detected!");
                    alertSent = true;
                }

                systemStep = 4;
            }
            break;

        case 4:
            // Final state
            break;
    }

    delay(200); // Small stability delay
}

// ---------------- BLYNK CONTROLS ----------------

// Servo control (manual)
BLYNK_WRITE(V2)
{
    int angle = param.asInt();
    setServoAngle(angle);
}

// Fan control
BLYNK_WRITE(V3)
{
    fanState = param.asInt();
    digitalWrite(RELAY_PIN, fanState);
}

// Buzzer control
BLYNK_WRITE(V4)
{
    buzzerState = param.asInt();
    digitalWrite(BUZZER_PIN, buzzerState);
}
