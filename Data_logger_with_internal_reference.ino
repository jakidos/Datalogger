#include <SPI.h>
#include <LCD.h>
#include <LiquidCrystal_I2C.h>
#include "SD.h"
#include <Wire.h>
#include "RTClib.h"

// A simple data logger for the Arduino analog pins
unsigned long LOG_INTERVAL = 1000; // mills between entries

// how many milliseconds before writing the logged data permanently to disk
// set it to the LOG_INTERVAL to write each time (safest)
// set it to 10*LOG_INTERVAL to write all data every 10 datareads, you could lose up to
// the last 10 reads if power is lost but it uses less power and is much faster!
unsigned long SYNC_INTERVAL = 1000; // mills between calls to flush() - to write data to the card
uint32_t syncTime = 0; // time of last sync()

bool FirstMeasure = false;
bool Calibration1 = false;
bool Calibration2 = false;

unsigned long last_time = 0;

#define ECHO_TO_SERIAL   1 // echo data to serial port

// The analog pins that connect to the sensors
#define MFC1Pin 0
#define MFC2Pin 1
#define MFC3Pin 2
#define MFC4Pin 3

// the digital pins that connect to the LEDs
const int redLEDpin = 5;
const int greenLEDpin = 6;

// Variables will change:
int redledState = LOW;             // ledState used to set the LED
int greenledState = LOW;

// Generally, you should use "unsigned long" for variables that hold time
// The value will quickly become too large for an int to store
unsigned long previousMillis = 0;        // will store last time LED was updated

// constants won't change:
const long interval = 1000;

int Contrast = 20;


const int buttonPin = 7;
int buttonState = 0;
boolean ShowTimer = false;

//switch on/off
int lastButtonState = 1;   // the previous reading from the input pin
int reading;
long lastDebounceTime = 0;  // the last time the output pin was toggled
long debounceDelay = 50;    // the debounce time; increase if the output flickers

//switch analog reference internal/DEFAULT
const int Switch2Pin = 3;
bool Switch2State = 0;
int lastSwitch2State = 1;   // the previous reading from the input pin
int Reading;
long lastDebounceSwitch2Time = 0;  // the last time the output pin was toggled
long debounceSwitch2Delay = 50;    // the debounce time; increase if the output flickers

//time measure increment button
uint8_t IncButtonPin = 4;
int IncButtonPinState = 0;
int IncButtonPinDebounce;
int LastIncButtonPinState = LOW;
int LastIncButtonPinDebounce = LOW;
unsigned long LastDebounceTime = 0;
unsigned long DebounceDelay = 50;
int buttonPushCounter = 0;

float R1 = 30000.0;
float R2 = 7500.0;
float MFC1_reading = 0.0;
float MFC2_reading = 0.0;
float MFC3_reading = 0.0;
float MFC4_reading = 0.0;
int offset = -27;

RTC_DS1307 RTC; // define the Real Time Clock object


// for the data logging shield, we use digital pin 10 for the SD cs line
const int chipSelect = 10;

// the logging file
File logfile;

void error(char *str)
{
  Serial.print("error: ");
  Serial.println(str);

  // red LED indicates error
  digitalWrite(redLEDpin, HIGH);

  while (1);
}

#define I2C_ADDR 0x27
LiquidCrystal_I2C lcd(I2C_ADDR, 2, 1, 0, 4, 5, 6, 7);

void setup()
{

  Serial.begin(9600);

  pinMode(buttonPin, INPUT);

  pinMode(Switch2Pin, INPUT);

  pinMode(IncButtonPin, INPUT);
  digitalWrite(IncButtonPin, HIGH);

  pinMode(redLEDpin, OUTPUT);
  pinMode(greenLEDpin, OUTPUT);

  digitalWrite(greenLEDpin, HIGH);

  //Defining analog pins
  analogReference(DEFAULT);

  Serial.println();


  //LCD
  analogWrite(6, Contrast);
  lcd.begin(20, 4);
  lcd.setBacklightPin(3, POSITIVE);
  lcd.setBacklight(HIGH);

  //Fake initializer
  lcd.begin(20, 4);
  lcd.print("||||||||||||||||||||");
  lcd.print("           ");
  lcd.setCursor(2, 1);
  lcd.print("   DATA LOGGER   ");
  lcd.setCursor(0, 2);
  lcd.print("||||||||||||||||||||");
  lcd.setCursor(0, 3);
  lcd.print("||||||||||||||||||||");
  delay(2000);

  // initialize the SD card
  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.print(" Connecting SD card ");
  delay(3000);
  Serial.print("Connecting SD card...");
  // make sure that the default chip select pin is set to
  // output, even if you don't use it:
  pinMode(10, OUTPUT);

  // see if the card is present and can be initialized:
  if (!SD.begin(chipSelect)) {

    Serial.println("Card failed or not present");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Card failed or not  present");
    delay(2000);// don't do anything more:

    return;
  }

  Serial.println("card initialized.");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("  Card inizialized");
  delay(2000);

  // create a new file
  char filename[] = "MFCLOG00.CSV";
  for (uint8_t i = 0; i < 100; i++) {
    filename[6] = i / 10 + '0';
    filename[7] = i % 10 + '0';
    if (! SD.exists(filename)) {
      // only open a new file if it doesn't exist
      logfile = SD.open(filename, FILE_WRITE);
      break;  // leave the loop!
    }
  }

  if (! logfile) {
    error("couldnt create file");
    lcd.clear();
    lcd.setCursor(0, 1);
    lcd.print("couldnt create file");
  }

  Serial.print("Logging to: ");
  Serial.println(filename);
  lcd.setCursor(0, 2);
  lcd.println("  ");
  lcd.println(filename);
  delay(2000);

  // connect to RTC
  Wire.begin();
  if (!RTC.begin()) {
    logfile.println("RTC failed");
#if ECHO_TO_SERIAL
    Serial.println("RTC failed");
#endif  //ECHO_TO_SERIAL
  }


  logfile.println("datetime \t ; CELL1 ; CELL2 ; CELL3 ; CELL4");
#if ECHO_TO_SERIAL
  Serial.println("datetime \t \t \t CELL1 \t CELL2 \t CELL3 \t CELL4");
#endif


}


void loop()
{
  reading = digitalRead(buttonPin);
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
    lastButtonState = reading;
  }
  buttonState = digitalRead(buttonPin);
  if ((millis() - lastDebounceTime) > debounceDelay) {

    if (buttonState == 1) { // switch ON

      if (!FirstMeasure) {

        schermo_1 ();

        FirstMeasure = true;
      }

      schermo_2 ();

    }
    if (buttonState == 0) {  //switch OFF

      FirstMeasure = false;

      unsigned long currentMillis = millis();

      if (currentMillis - previousMillis > interval) {

        previousMillis = currentMillis;

        schermo_3 ();
      }
      digitalWrite(redLEDpin, HIGH);

      int CurrentIncButtonPin = digitalRead(IncButtonPin);

      if (CurrentIncButtonPin != LastIncButtonPinDebounce)
      {
        LastDebounceTime = millis();
      }
      if ((millis() - LastDebounceTime) > DebounceDelay)
      {
        if (CurrentIncButtonPin != LastIncButtonPinState)
        {
          if (CurrentIncButtonPin == LOW )
          {
            buttonPushCounter++;

            //seconds
            if (buttonPushCounter == 1)
            {
              LOG_INTERVAL = 20000;
              SYNC_INTERVAL = LOG_INTERVAL;
            }
            else if (buttonPushCounter == 2)
            {
              LOG_INTERVAL = 40000;
              SYNC_INTERVAL = LOG_INTERVAL;
            }
            else if (buttonPushCounter == 3)
            {
              LOG_INTERVAL = 60000;
              SYNC_INTERVAL = LOG_INTERVAL;
            }

            //minutes
            else if (buttonPushCounter == 4)
            {
              LOG_INTERVAL = 300000;
              SYNC_INTERVAL = LOG_INTERVAL;
            }
            else if (buttonPushCounter == 5)
            {
              LOG_INTERVAL = 900000;
              SYNC_INTERVAL = LOG_INTERVAL;
            }
            else if (buttonPushCounter == 6)
            {
              LOG_INTERVAL = 1800000;
              SYNC_INTERVAL = LOG_INTERVAL;
            }
            else if (buttonPushCounter == 7)
            {
              LOG_INTERVAL = 3600000;
              SYNC_INTERVAL = LOG_INTERVAL;
            }

            //hours
            else if (buttonPushCounter == 8)
            {
              LOG_INTERVAL = 7200000;
              SYNC_INTERVAL = LOG_INTERVAL;
            }
            else if (buttonPushCounter == 9)
            {
              LOG_INTERVAL = 21600000;
              SYNC_INTERVAL = LOG_INTERVAL;
            }
            else if (buttonPushCounter == 10)
            {
              LOG_INTERVAL = 43200000;
              SYNC_INTERVAL = LOG_INTERVAL;
            }
            else if (buttonPushCounter == 11)
            {
              LOG_INTERVAL = 86400000;
              SYNC_INTERVAL = LOG_INTERVAL;
            }
            else if (buttonPushCounter == 12)
            {
              LOG_INTERVAL = 1000;
              SYNC_INTERVAL = LOG_INTERVAL;
            }
            else
              buttonPushCounter = 0;

          }
        }
        LastIncButtonPinState = CurrentIncButtonPin;
      }
      LastIncButtonPinDebounce = CurrentIncButtonPin;
    }
  }
}

void schermo_1 () {

  RTC_DS3231 rtc;

  DateTime now = rtc.now();

  // log milliseconds since starting
  uint32_t m = millis();
  //        logfile.print(m);           // milliseconds since start
  //        logfile.print(", ");
  //#if ECHO_TO_SERIAL
  //        Serial.print(m);         // milliseconds since start
  //        Serial.print("\t");
  //#endif

  // fetch the time
  now = RTC.now();
  // log time
  //    logfile.print(now.unixtime()); // seconds since 2000

  logfile.print(now.year(), DEC);
  logfile.print("/");
  logfile.print(now.month(), DEC);
  logfile.print("/");
  logfile.print(now.day(), DEC);
  logfile.print(" ");
  logfile.print(now.hour(), DEC);
  logfile.print(":");
  logfile.print(now.minute(), DEC);
  logfile.print(":");
  logfile.print(now.second(), DEC);
  logfile.print(";");
#if ECHO_TO_SERIAL
  // Serial.print(now.unixtime()); // seconds since 2000

  Serial.print(now.year(), DEC);
  Serial.print("/");
  Serial.print(now.month(), DEC);
  Serial.print("/");
  Serial.print(now.day(), DEC);
  Serial.print(" ");
  Serial.print(now.hour(), DEC);
  Serial.print(":");
  Serial.print(now.minute(), DEC);
  Serial.print(":");
  Serial.print(now.second(), DEC);
  Serial.print("\t");
#endif //ECHO_TO_SERIAL

  if (Switch2State == 1)
  {
    //CELL1
    analogRead(MFC1Pin);
    delay(10);
    MFC1_reading = analogRead(MFC1Pin);
    delay(10);
    MFC1_reading = analogRead(MFC1Pin);
    delay(10);
    MFC1_reading = (((MFC1_reading + 0.5) * 5.0 / 1024.0) / (R2 / (R1 + R2))-0.01);

    //CELL2
    analogRead(MFC2Pin);
    delay(10);
    MFC2_reading = analogRead(MFC2Pin);
    delay(10);
    MFC2_reading = analogRead(MFC2Pin);
    delay(10);
    MFC2_reading = (((MFC2_reading + 0.5) * 5.0 / 1024.0) / (R2 / (R1 + R2))-0.01);


    //CELL3
    analogRead(MFC3Pin);
    delay(10);
    MFC3_reading = analogRead(MFC3Pin);
    delay(10);
    MFC3_reading = analogRead(MFC3Pin);
    delay(10);
    MFC3_reading = (((MFC3_reading + 0.5) * 5.0 / 1024.0) / (R2 / (R1 + R2))-0.01);

    //CELL4
    analogRead(MFC4Pin);
    delay(10);
    MFC4_reading = analogRead(MFC4Pin);
    delay(10);
    MFC4_reading = analogRead(MFC4Pin);
    delay(10);
    MFC4_reading = (((MFC4_reading + 0.5) * 5.0 / 1024.0) / (R2 / (R1 + R2))-0.01);
  }

  if (Switch2State == 0)
  {
    //CELL1
    analogRead(MFC1Pin);
    delay(10);
    MFC1_reading = analogRead(MFC1Pin);
    delay(10);
    MFC1_reading = analogRead(MFC1Pin);
    delay(10);
    MFC1_reading = (((MFC1_reading + 0.5) * 1.068 / 1024.0) / (R2 / (R1 + R2)));

    //CELL2
    analogRead(MFC2Pin);
    delay(10);
    MFC2_reading = analogRead(MFC2Pin);
    delay(10);
    MFC2_reading = analogRead(MFC2Pin);
    delay(10);
    MFC2_reading = (((MFC2_reading + 0.5) * 1.068 / 1024.0) / (R2 / (R1 + R2)));


    //CELL3
    analogRead(MFC3Pin);
    delay(10);
    MFC3_reading = analogRead(MFC3Pin);
    delay(10);
    MFC3_reading = analogRead(MFC3Pin);
    delay(10);
    MFC3_reading = (((MFC3_reading + 0.5) * 1.068 / 1024.0) / (R2 / (R1 + R2)));

    //CELL4
    analogRead(MFC4Pin);
    delay(10);
    MFC4_reading = analogRead(MFC4Pin);
    delay(10);
    MFC4_reading = analogRead(MFC4Pin);
    delay(10);
    MFC4_reading = (((MFC4_reading + 0.5) * 1.068 / 1024.0) / (R2 / (R1 + R2)));
  }
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("CELL 1 = ");
  lcd.print(MFC1_reading);
  lcd.print("V");

  lcd.setCursor(0, 1);
  lcd.print("CELL 2 = ");
  lcd.print(MFC2_reading);
  lcd.print("V");

  lcd.setCursor(0, 2);
  lcd.print("CELL 3 = ");
  lcd.print(MFC3_reading);
  lcd.print("V");

  lcd.setCursor(0, 3);
  lcd.print("CELL 4 = ");
  lcd.print(MFC4_reading);
  lcd.print("V");


  logfile.print(MFC1_reading);
  logfile.print(";");
  logfile.print(MFC2_reading);
  logfile.print(";");
  logfile.print(MFC3_reading);
  logfile.print(";");
  logfile.print(MFC4_reading);
  logfile.println();
#if ECHO_TO_SERIAL

  Serial.print("\t");
  Serial.print(MFC1_reading);
  Serial.print("\t");
  Serial.print(MFC2_reading);
  Serial.print("\t");
  Serial.print(MFC3_reading);
  Serial.print("\t");
  Serial.print(MFC4_reading);
  Serial.println();
#endif //ECHO_TO_SERIAL

  // Now we write data to disk! Don't sync too often - requires 2048 bytes of I/O to SD card
  // which uses a bunch of power and takes time
  if ((millis() - syncTime) < SYNC_INTERVAL) return;
  syncTime = millis();

  // blink LED to show we are syncing data to the card & updating FAT!
  digitalWrite(redLEDpin, LOW);
  logfile.flush();
}


void schermo_2 () {

  RTC_DS3231 rtc;

  DateTime now = rtc.now();

  // delay for the amount of time we want between readings
  //delay((LOG_INTERVAL - 1) - (millis() % LOG_INTERVAL));

  for (int j = 0; j <= LOG_INTERVAL; j++)
  {
    buttonState = digitalRead(buttonPin);

    if (buttonState == 1) {

      delay(1);
    }

    if (buttonState == 0) {

      break;
    }
  }
  // if (millis() - last_time >= LOG_INTERVAL)
  //{
  //  last_time += LOG_INTERVAL;


  digitalWrite(greenLEDpin, HIGH);

  // log milliseconds since starting
  uint32_t m = millis();

  //        logfile.print(m);           // milliseconds since start
  //        logfile.print(", ");
  //#if ECHO_TO_SERIAL
  //        Serial.print(m);         // milliseconds since start
  //        Serial.print("\t");
  //#endif

  // fetch the time
  now = RTC.now();
  // log time
  //    logfile.print(now.unixtime()); // seconds since 2000

  logfile.print(now.year(), DEC);
  logfile.print("/");
  logfile.print(now.month(), DEC);
  logfile.print("/");
  logfile.print(now.day(), DEC);
  logfile.print(" ");
  logfile.print(now.hour(), DEC);
  logfile.print(":");
  logfile.print(now.minute(), DEC);
  logfile.print(":");
  logfile.print(now.second(), DEC);
  logfile.print(";");
#if ECHO_TO_SERIAL
  // Serial.print(now.unixtime()); // seconds since 2000

  Serial.print(now.year(), DEC);
  Serial.print("/");
  Serial.print(now.month(), DEC);
  Serial.print("/");
  Serial.print(now.day(), DEC);
  Serial.print(" ");
  Serial.print(now.hour(), DEC);
  Serial.print(":");
  Serial.print(now.minute(), DEC);
  Serial.print(":");
  Serial.print(now.second(), DEC);
  Serial.print("\t");
#endif //ECHO_TO_SERIAL

  if (Switch2State == 1)
  {
    //CELL1
    analogRead(MFC1Pin);
    delay(10);
    MFC1_reading = analogRead(MFC1Pin);
    delay(10);
    MFC1_reading = analogRead(MFC1Pin);
    delay(10);
    MFC1_reading = (((MFC1_reading + 0.5) * 5.0 / 1024.0) / (R2 / (R1 + R2))-0.01);

    //CELL2
    analogRead(MFC2Pin);
    delay(10);
    MFC2_reading = analogRead(MFC2Pin);
    delay(10);
    MFC2_reading = analogRead(MFC2Pin);
    delay(10);
    MFC2_reading = (((MFC2_reading + 0.5) * 5.0 / 1024.0) / (R2 / (R1 + R2))-0.01);


    //CELL3
    analogRead(MFC3Pin);
    delay(10);
    MFC3_reading = analogRead(MFC3Pin);
    delay(10);
    MFC3_reading = analogRead(MFC3Pin);
    delay(10);
    MFC3_reading = (((MFC3_reading + 0.5) * 5.0 / 1024.0) / (R2 / (R1 + R2))-0.01);

    //CELL4
    analogRead(MFC4Pin);
    delay(10);
    MFC4_reading = analogRead(MFC4Pin);
    delay(10);
    MFC4_reading = analogRead(MFC4Pin);
    delay(10);
    MFC4_reading = (((MFC4_reading + 0.5) * 5.0 / 1024.0) / (R2 / (R1 + R2))-0.01);
  }

  if (Switch2State == 0)
  {
    //CELL1
    analogRead(MFC1Pin);
    delay(10);
    MFC1_reading = analogRead(MFC1Pin);
    delay(10);
    MFC1_reading = analogRead(MFC1Pin);
    delay(10);
    MFC1_reading = (((MFC1_reading + 0.5) * 1.068 / 1024.0) / (R2 / (R1 + R2)));

    //CELL2
    analogRead(MFC2Pin);
    delay(10);
    MFC2_reading = analogRead(MFC2Pin);
    delay(10);
    MFC2_reading = analogRead(MFC2Pin);
    delay(10);
    MFC2_reading = (((MFC2_reading + 0.5) * 1.068 / 1024.0) / (R2 / (R1 + R2)));


    //CELL3
    analogRead(MFC3Pin);
    delay(10);
    MFC3_reading = analogRead(MFC3Pin);
    delay(10);
    MFC3_reading = analogRead(MFC3Pin);
    delay(10);
    MFC3_reading = (((MFC3_reading + 0.5) * 1.068 / 1024.0) / (R2 / (R1 + R2)));

    //CELL4
    analogRead(MFC4Pin);
    delay(10);
    MFC4_reading = analogRead(MFC4Pin);
    delay(10);
    MFC4_reading = analogRead(MFC4Pin);
    delay(10);
    MFC4_reading = (((MFC4_reading + 0.5) * 1.068 / 1024.0) / (R2 / (R1 + R2)));
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("CELL 1 = ");
  lcd.print(MFC1_reading);
  lcd.print("V");

  lcd.setCursor(0, 1);
  lcd.print("CELL 2 = ");
  lcd.print(MFC2_reading);
  lcd.print("V");

  lcd.setCursor(0, 2);
  lcd.print("CELL 3 = ");
  lcd.print(MFC3_reading);
  lcd.print("V");

  lcd.setCursor(0, 3);
  lcd.print("CELL 4 = ");
  lcd.print(MFC4_reading);
  lcd.print("V");


  logfile.print(MFC1_reading);
  logfile.print(";");
  logfile.print(MFC2_reading);
  logfile.print(";");
  logfile.print(MFC3_reading);
  logfile.print(";");
  logfile.print(MFC4_reading);
  logfile.println();
#if ECHO_TO_SERIAL

  Serial.print("\t");
  Serial.print(MFC1_reading);
  Serial.print("\t");
  Serial.print(MFC2_reading);
  Serial.print("\t");
  Serial.print(MFC3_reading);
  Serial.print("\t");
  Serial.print(MFC4_reading);
  Serial.println();
#endif //ECHO_TO_SERIAL



  // Now we write data to disk! Don't sync too often - requires 2048 bytes of I/O to SD card
  // which uses a bunch of power and takes time
  if ((millis() - syncTime) < SYNC_INTERVAL) return;
  syncTime = millis();

  // blink LED to show we are syncing data to the card & updating FAT!
  digitalWrite(redLEDpin, HIGH);
  logfile.flush();
  digitalWrite(redLEDpin, LOW);

}


void schermo_3 () {

  if (LOG_INTERVAL < 60000) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("      STANDBY   ");
    lcd.setCursor(0, 1);
    lcd.print("   READY TO START   ");
    lcd.setCursor(0, 3);
    lcd.print("Interval = ");

    lcd.print(LOG_INTERVAL / 1000);
    lcd.print(" s");
  }
  if (LOG_INTERVAL >= 60000) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("      STANDBY   ");
    lcd.setCursor(0, 1);
    lcd.print("   READY TO START   ");
    lcd.setCursor(0, 3);
    lcd.print("Interval = ");
    lcd.print(LOG_INTERVAL / 60000);
    lcd.print(" min");
  }
  if (LOG_INTERVAL >= 3600000) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("      STANDBY   ");
    lcd.setCursor(0, 1);
    lcd.print("   READY TO START   ");
    lcd.setCursor(0, 3);
    lcd.print("Interval = ");
    lcd.print(LOG_INTERVAL / 3600000);
    lcd.print(" hour");
  }

  Reading = digitalRead(Switch2Pin);
  if (Reading != lastSwitch2State) {
    lastDebounceSwitch2Time = millis();
    lastSwitch2State = Reading;
  }

  if ((millis() - lastDebounceSwitch2Time) > debounceSwitch2Delay) {
    Switch2State = digitalRead(Switch2Pin);// read the state of the pushbutton value:

    if (Switch2State == 0)
    { Calibration2 = false;

      if (!Calibration1) {
        analogReference(INTERNAL);
        delay(500);
        for (int i = 0; i < 200; i++)
        {
          analogRead(MFC1Pin);
          delay(10);
          MFC1_reading = analogRead(MFC1Pin);
        }
        Calibration1 = true;
      }
      lcd.setCursor(0, 2);
      lcd.print("Voltage Ref = 1.1 V");

    }

    if (Switch2State == 1)
    { Calibration1 = false;

      if (!Calibration2) {
        analogReference(DEFAULT);
        delay(500);

        for (int i = 0; i < 200; i++)
        {
          analogRead(MFC1Pin);
          delay(10);
          MFC1_reading = analogRead(MFC1Pin);
        }
        Calibration2 = true;
      }
      lcd.setCursor(0, 2);
      lcd.print("Voltage Ref = 5 V");
    }
  }
}
