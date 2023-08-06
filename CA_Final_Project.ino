/* Control and Automation for the Efficient Use of Energy
Final Project
@authors: Sara Kiprijanova, Alice Scalamandrè, Laura Amaro
*/

///// G E N E R A L   V A L U E S
#define ADC_SOLUTION (4095.0) // ADC accuracy of ESP32 is 12bit
#define ESP_VOLTAGE (3.3) // 3.3V output of ESP32

///// D I S T A N C E  V A R I A B L E S
#define MAX_RANGE (500) // The max measurement vaule of the module is 500cm
const int signalPin = A0; // Input analog pin 

///// C U R R E N T  V A R I A B L E S
// Measure the consumed current by a load with the SCT013-30A CT
// Pins
const int sensorPin = A3;
const int refPin = A2;
const int Rshunt = 33.3;
// Time variables
unsigned long time_now = 0;
unsigned long time1_ant = 0, time2_ant = 0;
unsigned long count = 0;
float sum1 = 0, sum2 = 0;
double Ifilt = 0.0;   
// Auxiliary variables
unsigned long time_ant = 0, difTime = 0, act_time = 0;
const int sampleDuration = 20;
int count_integral = 0;
double rawSquaredSum = 0;
double Iant = 0;
// Constant grid frequency (50 Hz)
double freq = 50;
// Transformer reduction relationship
double n_trafo = 1000;
// Measured current variable
double Irms = 0;

///// T E L E G R A M  V A R I A B L E S
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>

const char* ssid = "MIWIFI_fXEC"; // Needs to be changed for different WiFi
const char* password = "YUFJYFhf";  // Needs to be changed for different WiFi

#define BOTtoken "5894687503:AAG-kLyVIJFXcaGAJPe864e-8p8cHSzA6xA" // Telegram Bot
#define CHAT_ID "1529642773"  // Sara's Telegram ID

WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

int botRequestDelay = 5000;
unsigned long lastTimeBotRan;

// Handle what happens new messages from user are received
void handleNewMessages(int numNewMessages) {
  for (int i=0; i<numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    if (chat_id != CHAT_ID){
      bot.sendMessage(chat_id, "Unauthorized user", ""); // If other user tries to message the bot
      continue;
    }    
    // Print the received message
    String text = bot.messages[i].text;
    Serial.println(text);
    String from_name = bot.messages[i].from_name;
  }
}

///// R E L A Y  V A R I A B L E S 
#define RELAY_PIN D2
bool carDetectedOld = false;
bool carDetectedNew = false;
bool stopPrint = false;
char relayControl; 

///// S E T U P
void setup() {
Serial.begin(9600);

///// M O T I O N  S E T U P
float distCar, sensity_t;

///// C U R R E N T  S E T U P 

///// T E L E G R A M  S E T U P
// Establishing WiFi connection
Serial.print("Connecting Wifi: ");
Serial.println(ssid);

WiFi.mode(WIFI_STA);
WiFi.begin(ssid, password);
client.setCACert(TELEGRAM_CERTIFICATE_ROOT);
 
while (WiFi.status() != WL_CONNECTED) {
  Serial.print(".");
  delay(500);
}

Serial.println("");
Serial.println("WiFi connected");
Serial.print("IP address: ");
Serial.println(WiFi.localIP());

bot.sendMessage(CHAT_ID, "Welcome home!", "");

///// R E L A Y  S E T U P 
// Initialize digital pin as an output
pinMode(RELAY_PIN, OUTPUT);
}

///// L O O P
void loop() {

///// M O T I O N  L O O P
// Read the value from the sensor:
float sensity_t = analogRead(signalPin);
float distCar = sensity_t * MAX_RANGE / ADC_SOLUTION;
//Serial.print(distCar);
//Serial.println(""); // Value sent to the computer

if(stopPrint == false){
  //Serial.print("Distance: ");
  //Serial.print(distCar,0);
  //Serial.println(";");
  delay(2500); 
  if(distCar==0) {
    stopPrint = true;
  }
}

///// T E L E G R A M  L O O P
if(distCar==0){
    carDetectedNew = true;
}

if(carDetectedNew != carDetectedOld){
    bot.sendMessage(CHAT_ID, "Your car is now parked. How many hours will you stay at home?", "");
    carDetectedOld = true;
}    

// Sending a message from the User to the bot
if (millis() > lastTimeBotRan + botRequestDelay)  {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while(numNewMessages) {
      bot.sendMessage(CHAT_ID, "Thanks, enjoy your stay at home!");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    lastTimeBotRan = millis();
  }

///// R E L A Y  L O O P
if (Serial.available() > 0) {
  relayControl = Serial.read();
  if (relayControl == 'H') {
  digitalWrite(RELAY_PIN, HIGH);
  }
    else if (relayControl == 'L') {
    digitalWrite(RELAY_PIN, LOW);
  }  
}

///// C U R R E N T  L O O P
act_time = micros();
  difTime = act_time - time_ant;
  int RawValue = 0;
  if (difTime >= 1000) {
    time_ant = act_time + (difTime - 1000);

    // Read the ADC input from the sensor and the voltage reference point
    int ADC_sensor = analogRead(sensorPin);
    int ADC_ref = analogRead(refPin);

    // Convert the ADC input measured to voltage values
    double V_sens = ADC_sensor * ESP_VOLTAGE / ADC_SOLUTION;
    double V_ref = ADC_ref * ESP_VOLTAGE / ADC_SOLUTION;

    // Calculate the instantaneous current using the voltage diference and the burder resistor value
    double Iinst =  n_trafo * (V_sens - V_ref) / Rshunt;

    // Calculate the integral
     rawSquaredSum += Iinst * Iinst * 0.001;

    // Count 20 ms
    count_integral++;
  }

  // Each 20 ms, calculte the RMS
  if (count_integral >= sampleDuration)
  {
    // Calculate the RMS
    Irms = sqrt(freq * rawSquaredSum);
    // Counter and integral reset
    count_integral = 0;
    rawSquaredSum = 0;

    // Low-pass filter
    Ifilt = 0.95 * Iant + 0.05 * Irms;
    Iant = Ifilt;

    // Calculate the average power
    double Pavg = Ifilt * 230.0;
  }  
    // Read time in ms
    time_now = millis();
    // Each 1 second, measure the A2 and A3 ports
    if (time_now - time1_ant > 1000) {
      // Increment the time counter
      count++;
      // Acumulate the ADC measurements each second, to calculate latter and average value each 5 seconds
      sum1 += Ifilt;
      sum2 += (Ifilt * 230.0);
      // Udpate the "1 second" time flag
      time1_ant = time_now;
    }

    /* Each 5 seconds, calculate the average value of the A2 and A3 measurments, and the state of the relay output pin
    and write the values with the serial port using semicolons to separate them */
    if (time_now - time2_ant > 5000) {
      //Serial.print("Current: ");
      //Serial.print(sum1/count);
      //Serial.print("; Power: ");
      Serial.print(sum2/count); // Value sent to the computer
      //Serial.println(";");
      // Reset the variables to calculate the avarage results
      sum1 = 0;
      sum2 = 0;
      // Reset the time counter and update the "5 second" time flag
      count = 0;
      time2_ant = time_now;
    }  
}