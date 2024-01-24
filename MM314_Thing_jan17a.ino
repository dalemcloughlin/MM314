#include "arduino_secrets.h"
/*
  MM314 Early Conceptualisation
  17/01/24

  Rev 0
  Aim: Investigate the viability of using the new arduino cloud IOT features for use in the test rig.

  Uses a rolling mean data smoothing (5 point window) for voltage input

  Triggers Alarm Sequence in the event that input voltage is below 110% of the critical voltage
  for 5 data points in a row (this limit of consecutive exceedances may change)

*/

//Development: User messages:
// TODO: This should be a text file
const String RES_dropoff_warning_message = "WARNING: RES is not providing sufficient power.";
const String RES_dropoff_alert_message = "WARNING: RES is below critical threshold.";
const String switchover_RES_to_BES_message = "Switching Over to BES.";
const String switchover_BES_to_RES_message = "Switching Over to RES.";
const String all_systems_ok_message = "All Systems Operational";
const String startup_message = "Successful Startup!";
const String alerts_on_message = "Alerts switched on.";
const String alerts_off_message = "Alerts switched off.";



#include "thingProperties.h"

// Define analog input
#define ANALOG_IN_PIN A0
#define ALARM_PIN 7
#define WARNING_PIN 12
#define OK_PIN 8

// Floats for ADC voltage & Input voltage
float adc_voltage = 0.0;

// Floats for resistor values in divider (in ohms)
float R1 = 30000.0;
float R2 = 7500.0;

// Float for Reference Voltage
float ref_voltage = 5;

// Integer for ADC value
int adc_value = 0;

//moving average
float voltage_value = 0;           // Variable to store the raw potentiometer value
const int windowSize = 5;    // Size of the moving average window
float voltage_values[windowSize] = {0.0};   // Array to store the last 'windowSize' potentiometer values
int voltage_valuesIndex = 0;      // Index for updating the array
float sum = 0.0;
float smoothed_value_raw = 0.0;

//Counting variable for alarm function
int i;
int exceed = 0;
int exceed_threshold = 5;

//alarm configuration
int alarm_rings = 3;
int alarm_on_time = 50;
int alarm_off_time = 100;

//safety factor (1.1 = 10% safety factor)
float safety_factor = 1.1;

void setup() {
  // Initialize serial and wait for port to open:
  Serial.begin(9600);
  // This delay gives the chance to wait for a Serial Monitor without blocking if none is found
  delay(1500);

  Serial.println("PV Monitoring System");

  // Defined in thingProperties.h
  initProperties();

  // Connect to Arduino IoT Cloud
  ArduinoCloud.begin(ArduinoIoTPreferredConnection);

  /*
     The following function allows you to obtain more information
     related to the state of network and IoT Cloud connection and errors
     the higher number the more granular information you’ll get.
     The default is 0 (only errors).
     Maximum is 4
  */
  setDebugMessageLevel(2);
  ArduinoCloud.printDebugInfo();

  /* Define inputs and outputs */
  pinMode(ANALOG_IN_PIN, INPUT);
  pinMode(ALARM_PIN, OUTPUT);
  pinMode(WARNING_PIN, OUTPUT);
  pinMode(OK_PIN, OUTPUT);

  //initialise critical_voltage
  critical_voltage = 4.0;

  //initialise smoothed voltage
  smoothed_value = 0.0;   // Variable to store the smoothed potentiometer value (moving average)

  user_message = startup_message;
}

void loop() {

  ArduinoCloud.update();

  // Read the Analog Input
  adc_value = analogRead(ANALOG_IN_PIN);

  // Determine voltage at ADC input
  adc_voltage  = (adc_value * ref_voltage) / 1024.0;

  // Calculate voltage at divider input
  in_voltage = adc_voltage / (R2 / (R1 + R2));

  // Update the moving average array
  voltage_values[voltage_valuesIndex] = in_voltage;
  voltage_valuesIndex = (voltage_valuesIndex + 1) % windowSize;

  //Reset push nofication variable
  send_push_notification = LOW;

  // Calculate the moving average
  sum = 0;
  for (int i = 0; i < windowSize; i++) {
    sum += voltage_values[i];
  }
  smoothed_value_raw = 10 * (sum / windowSize);
  smoothed_value = round(smoothed_value_raw) / 10;

  // Print results to Serial Monitor to 2 decimal places
  Serial.print("Input Voltage = ");
  Serial.println(in_voltage, 2);

  Serial.print("Smoothed Voltage = ");
  Serial.println(smoothed_value, 2);

  Serial.println();
  Serial.print("Critical Voltage = ");
  Serial.println(critical_voltage, 2);

  if (in_voltage > safety_factor * critical_voltage) {
    operational_state = HIGH;
    Serial.println("Systems Operational");
    digitalWrite(OK_PIN, HIGH);
    exceed = 0;
  }
  else {
    operational_state = LOW;
    exceed++;
    digitalWrite(OK_PIN, LOW);
    digitalWrite(WARNING_PIN, HIGH);
    delay(alarm_on_time);
    digitalWrite(WARNING_PIN, LOW);
    delay(alarm_off_time);
    if (exceed > exceed_threshold) {
      //alarm procedure
      Serial.println("System Disruption!");
      user_message = switchover_BES_to_RES_message;
      send_push_notification = HIGH;
      for (i = 1; i <= alarm_rings; i++) {
        digitalWrite(ALARM_PIN, HIGH);
        delay(alarm_on_time);
        digitalWrite(ALARM_PIN, LOW);
        delay(alarm_off_time);
      }
    }
    Serial.println();
    Serial.println();

  }
}

/*
  Since CriticalVoltage is READ_WRITE variable, onCriticalVoltageChange() is
  executed every time a new value is received from IoT Cloud.
*/
void onCriticalVoltageChange()  {
  // Add your code here to act upon CriticalVoltage change
}

/*
  Since SmoothedValue is READ_WRITE variable, onSmoothedValueChange() is
  executed every time a new value is received from IoT Cloud.
*/
void onSmoothedValueChange()  {
  // Add your code here to act upon SmoothedValue change
}
/*
  Since OperationalState is READ_WRITE variable, onOperationalStateChange() is
  executed every time a new value is received from IoT Cloud.
*/
void onOperationalStateChange()  {
  // Add your code here to act upon OperationalState change
  if (alerts_on==1){
    if(operational_state == 1){
    user_message = all_systems_ok_message;
  }
  else {
    user_message = RES_dropoff_warning_message;
  }
}
}
/*
  Since AlertsOn is READ_WRITE variable, onAlertsOnChange() is
  executed every time a new value is received from IoT Cloud.
*/
void onAlertsOnChange()  {
  // Add your code here to act upon AlertsOn change
  if (alerts_on==HIGH){
    user_message = alerts_on_message;
  }
  if (alerts_on==LOW){
    user_message = alerts_off_message;
  }
}

void onUserMessageChange()  {
  // Add your code here to act upon CriticalVoltage change
}


/*
  Since SendPushNotification is READ_WRITE variable, onSendPushNotificationChange() is
  executed every time a new value is received from IoT Cloud.
*/
void onSendPushNotificationChange()  {
  // Add your code here to act upon SendPushNotification change
}