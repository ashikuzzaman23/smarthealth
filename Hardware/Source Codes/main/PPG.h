/*
   Arduino Sketch to detect pulses from two PulseSensors.

   Here is a link to the tutorial
   https://pulsesensor.com/pages/two-or-more-pulse-sensors

   Copyright World Famous Electronics LLC - see LICENSE
   Contributors:
     Joel Murphy, https://pulsesensor.com
     Yury Gitman, https://pulsesensor.com
     Bradford Needham, @bneedhamia, https://bluepapertech.com

   Licensed under the MIT License, a copy of which
   should have been included with this software.

   This software is not intended for medical use.
*/

/*
   Every Sketch that uses the PulseSensor Playground must
   define USE_ARDUINO_INTERRUPTS before including PulseSensorPlayground.h.
   Here, #define USE_ARDUINO_INTERRUPTS false tells the library to
   not use interrupts to read data from the PulseSensor.

   If you want to use interrupts, simply change the line below
   to read:
     #define USE_ARDUINO_INTERRUPTS true

   Set US_PS_INTERRUPTS to false if either
   1) Your Arduino platform's interrupts aren't yet supported
   by PulseSensor Playground, or
   2) You don't wish to use interrupts because of the side effects.

   NOTE: if US_PS_INTERRUPTS is false, your Sketch must
   call pulse.sawNewSample() at least once every 2 milliseconds
   to accurately read the PulseSensor signal.
*/
#define USE_ARDUINO_INTERRUPTS false
#include <PulseSensorPlayground.h>


/*
   The format of our output.

   Set this to PROCESSING_VISUALIZER if you're going to run
    the multi-sensor Processing Visualizer Sketch.
    See https://github.com/WorldFamousElectronics/PulseSensorAmped_2_Sensors

   Set this to SERIAL_PLOTTER if you're going to run
    the Arduino IDE's Serial Plotter.
*/
const int OUTPUT_TYPE = SERIAL_PLOTTER;

/*
   Number of PulseSensor devices we're reading from.
*/
const int PULSE_SENSOR_COUNT = 2;

/*
     PIN_POWERx = the output pin that the red (power) pin of
      the first PulseSensor will be connected to. PulseSensor only
      draws about 4mA, so almost any micro can power it from a GPIO.
      If you don't want to use pins to power the PulseSensors, you can remove
      the code dealing with PIN_POWER0 and PIN_POWER1.
     PIN_INPUTx = Analog Input. Connected to the pulse sensor
      purple (signal) wire.
     PIN_BLINKx = digital Output. Connected to an LED (must have at least
      470 ohm resistor) that will flash on each detected pulse.
     PIN_FADEx = digital Output. PWM pin onnected to an LED (must have
      at least 470 ohm resistor) that will smoothly fade with each pulse.

     NOTE: PIN_FADEx must be pins that support PWM.
       If USE_INTERRUPTS is true, Do not use pin 9 or 10 for PIN_FADEx
       because those pins' PWM interferes with the sample timer.
*/
const int PIN_INPUT0 = A3;
const int PIN_BLINK0 = 40;    // Pin 13 is the on-board LED
const int PIN_FADE0 = 2;

const int PIN_INPUT1 = A2;
const int PIN_BLINK1 = 41;
const int PIN_FADE1 = 3;

const int THRESHOLD = 550;   // Adjust this number to avoid noise when idle

/*
   samplesUntilReport = the number of samples remaining to read
   until we want to report a sample over the serial connection.

   We want to report a sample value over the serial port
   only once every 20 milliseconds (10 samples) to avoid
   doing Serial output faster than the Arduino can send.
*/
byte samplesUntilReport;
const byte SAMPLES_PER_SERIAL_SAMPLE = 10;

/*
   All the PulseSensor Playground functions.
   We tell it how many PulseSensors we're using.
*/
PulseSensorPlayground pulseSensor(PULSE_SENSOR_COUNT);
unsigned long lastBeatSampleNumber[PULSE_SENSOR_COUNT];
const int minPTT = 50;
const int maxPTT = 300;
int last_ten_ptt[10];
int last_ten_BPM[10];
bool firstPTT = false;
bool newPTT = false;
bool firstBPM = false;
int PTT;
int BPM;
int loopcount;
double newPTTValue, newBPMValue;
double alpha;
int SBP;
int DBP;

void bloodPressure()
{
  SBP = (int)(215 - 0.5282 * PTT);
  DBP = (int)(123 - 0.2218 * PTT);
  pulseSensor.outputToSerial('S', SBP);
  pulseSensor.outputToSerial('D', DBP);

}

void ppg_setup()
{
  //Serial.begin(250000);

  /*
     Configure the PulseSensor manager,
     telling it which PulseSensor (0 or 1)
     we're configuring.
  */

  pulseSensor.analogInput(PIN_INPUT0, 0);
  pulseSensor.blinkOnPulse(PIN_BLINK0, 0);
  pulseSensor.fadeOnPulse(PIN_FADE0, 0);

  pulseSensor.analogInput(PIN_INPUT1, 1);
  pulseSensor.blinkOnPulse(PIN_BLINK1, 1);
  pulseSensor.fadeOnPulse(PIN_FADE1, 1);

  pulseSensor.setSerial(Serial);
  pulseSensor.setOutputType(OUTPUT_TYPE);
  pulseSensor.setThreshold(THRESHOLD);

  // Skip the first SAMPLES_PER_SERIAL_SAMPLE in the loop().
  samplesUntilReport = SAMPLES_PER_SERIAL_SAMPLE;

  // Now that everything is ready, start reading the PulseSensor signal.
  if (!pulseSensor.begin()) {
    /*
       PulseSensor initialization failed,
       likely because our Arduino platform interrupts
       aren't supported yet.

       If your Sketch hangs here, try changing USE_ARDUINO_INTERRUPTS to false.
    */
    for (;;) {
      // Flash the led to show things didn't work.
      digitalWrite(PIN_BLINK0, LOW);
      delay(50);
      digitalWrite(PIN_BLINK0, HIGH);
      delay(50);
    }
  }
}


void ppg_operation()
{
  if (pulseSensor.sawNewSample()) {

    if (--samplesUntilReport == (byte) 0) {
      samplesUntilReport = SAMPLES_PER_SERIAL_SAMPLE;

      // pulseSensor.outputSample();

      /*
         If a beat has happened on a given PulseSensor
         since we last checked, write the per-beat information
         about that PulseSensor to Serial.
      */

      for (int i = 0; i < PULSE_SENSOR_COUNT; ++i) {
        if (pulseSensor.sawStartOfBeat(i)) {
          //pulseSensor.outputBeat(i);
          //pulseSensor.outputSample();

          lastBeatSampleNumber[i] = pulseSensor.getLastBeatTime(i);
          if (i == 1) {
            PTT = lastBeatSampleNumber[1] - lastBeatSampleNumber[0];
            newPTT = true;
          }
        }
      }
    }

    if (newPTT)
    {
      newPTT = false;
      if (PTT >= minPTT && PTT <= maxPTT)
      {
        if (!firstPTT)
        {
          newPTTValue = PTT;
          firstPTT = true;
        }
        else
        {
          int difference = abs(PTT - newPTTValue);
          if (difference > 20)
          {
            alpha = 0.1;
          }
          else if (difference <= 20 && difference > 10)
          {
            alpha = 0.175;
          }
          else if (difference <= 10 && difference > 5)
          {
            alpha = 0.25;
          }
          else
          {
            alpha = 0.5;
          }

          newPTTValue = alpha * PTT + (1 - alpha) * newPTTValue;
        }
      }

      BPM = pulseSensor.getBeatsPerMinute(1);
      if (BPM >= 60 && BPM <= 100)
      {
        if (!firstBPM)
        {
          newBPMValue = BPM;
          firstBPM = true;
        }
        else
        {
          int difference = abs(BPM - newBPMValue);
          if (difference > 20)
          {
            alpha = 0.5;
          }
          else if (difference <= 20 && difference > 10)
          {
            alpha = 0.333;
          }
          else if (difference <= 10 && difference > 5)
          {
            alpha = 0.25;
          }
          else
          {
            alpha = 0.1;
          }

          newBPMValue = alpha * BPM + (1 - alpha) * newBPMValue;
        }
      }

      pulseSensor.outputToSerial('|', newPTTValue);
      pulseSensor.outputToSerial('+', BPM);
      pulseSensor.outputToSerial('-', newBPMValue);
      SBP = (int)(215 - 0.5282 * newPTTValue);
      DBP = (int)(123 - 0.2218 * newPTTValue);
      pulseSensor.outputToSerial('S', SBP);
      pulseSensor.outputToSerial('D', DBP);
      //bloodPressure();
    }
  }
}


