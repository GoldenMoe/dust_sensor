/*
 * @file dust_sensor.ino
 * @author James Moessis
 * @date April 2018
 * 
 * See Program notes at bottom.
 * See README.md for more info.
 */

#include "Arduino.h"
#include <hpma115S0.h>   // https://github.com/felixgalindo/HPMA115S0
#include <TimeLib.h>     // https://github.com/PaulStoffregen/Time
#include <MegunoLink.h>  // https://github.com/Megunolink/MLP

#define BAUDRATE 9600
#define FET 2 // Trigger threshold. MOSFET. Actives red LED
#define GREEN_LED 3 // High when threshold note exceeded
#define POWER_DUST 4 // pnp bjt powering HPMA
// scale the threshold with this value. 
#define SCALE 1.0 // to specify float, must have decimal notation

void reset_sensor(void);
int read_adc(void);

// GLOBALS
  time_t t;

  //Controlled by Potentiometer
  const int analogInPin = A0;  // Potentiometer Analog ping
  const int analogOutPin = 9; 
  short int sensorValue = 0;        // value read from the pot
  short int outputValue = 0;        // value output to the PWM
  float threshold;
  float average; 
  
  // Controlled by Dust Sensor
  double sum = 0;
  float circle[300]; // Dust measurements store
  size_t circle_size = sizeof(circle)/sizeof(circle[0]);
  unsigned int i; // index for circle


//Create an instance of the HPMA115S0 class
HPMA115S0 honeywell(Serial1);

//Create an instance of MegunoLink TimePlot class
TimePlot dust_plot;

void setup() {
  
  // initialize pins
  pinMode(POWER_DUST, OUTPUT);
  pinMode(FET, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);

  digitalWrite(FET, LOW);
  digitalWrite(GREEN_LED, HIGH);
  
  //power cycle dust sensor
  reset_sensor();
  
  Serial.begin(BAUDRATE);
  Serial.println("Hello Computer.");
  
  // initialize all measurements as 0
  for( i = 0; i < circle_size; i++){
    circle[i] = 0;
  }
  i = 0; // i is global, reset to 0

  dust_plot.SetTitle("Dust vs Threshold Over Time");
  dust_plot.SetXlabel("Time");
  dust_plot.SetYlabel("Dust Level (%)");
  
  Serial1.begin(BAUDRATE); //begin honeywell comms
  do {
    Serial.println("Starting Serial1...");
    delay(5000); // wait 5s for honeywell to connect
  } while (!Serial1);

  //power control will be on pin 3, active low
  
  honeywell.Init();
  honeywell.StartParticleMeasurement();
  Serial.println("Setup func complete!");
}


void loop() {
    
    static int failure_count;
    // Read Honeywell dust sensor
    unsigned int pm2_5, pm10;
    if (honeywell.ReadParticleMeasurement(&pm2_5, &pm10)) {
      failure_count = 0;
      Serial.println("PM 2.5: " + String(pm2_5) + " ug/m3" );
      Serial.println("PM 10: " + String(pm10) + " ug/m3" );
      
      // convert dust measurement to value out of 100
      circle[i] = pm10 * ( (float)100 / (float)1000 ); 
      Serial.println("circle[i] = " + String(circle[i]));
      Serial.println("i = " + String(i));
      delay(10);
    }
    else {
      failure_count++;
    }
    
    // Increment iterator
    // If reached end of array, go back to beginning
    if(i >= circle_size - 1) {
      i = 0;
      Serial.println("Resetting i!");
      time_t lap_time = now() - t;
      Serial.print("lap_time = ");
      Serial.println(lap_time);
      t = now(); // time in sec since program started

      // Send data to meguno for plotting
      // https://www.megunolink.com/documentation/plotting/time-plot-reference/
      dust_plot.SendData("Dust Level", average);
      dust_plot.SendData("Threshold Level", SCALE*threshold);

      // Print message to meguno for logging
      // https://www.megunolink.com/documentation/logging-data/
      Serial.print("{MESSAGE:|data|");
      Serial.print(",");
      Serial.print(SCALE*threshold);
      Serial.print(",");
      Serial.print(average);
      Serial.println("}");
    }
    else {
      i++;
    }      

    //read analog
    //threshold dust level out of 100
    threshold = read_adc() * ( (float)100 / (float)1023 );
    Serial.println("Threshold = " + String(threshold) + "%");

  if (failure_count < 1000){
    // Sum all recent measurements
    int k = 0;
    sum = 0;
    for(k = 0; k < circle_size; k++) {
      sum = sum + circle[k];
      //sum = sum + (float)99.9; //test maxed values
    }

    // Average all recent measurements
    average = ( (float)sum ) / ( (float)circle_size ); 
    //Serial.println("Sum = " + String(sum));
    Serial.println("Avg = " + String(average));     
    
    
    // Red Light on
    if (average >= SCALE * threshold) {
      digitalWrite(GREEN_LED, LOW);
      digitalWrite(FET, HIGH);
      delay(1);
      Serial.println("Threshold Exceeded");
    }
    // Green light on
    else {
      digitalWrite(GREEN_LED, HIGH);
      digitalWrite(FET, LOW);
      delay(1);
    }
  }
  else {
    failure_count = 0;
    reset_sensor();
  }
}



//adapted from example Arduino sketch
int read_adc(){
  // read the analog in value:
  sensorValue = analogRead(analogInPin);
  // map it to the range of the analog out:
  outputValue = map(sensorValue, 0, 1023, 0, 255);
  // change the analog out value:
  analogWrite(analogOutPin, outputValue);

  // wait 2 milliseconds before the next loop for the analog-to-digital
  // converter to settle after the last reading:
  delay(2);
  return sensorValue;
}

void reset_sensor(){
  digitalWrite(POWER_DUST, HIGH);
  Serial.println("Resetting Honeywell Sensor..."); 
  delay(3000);
  digitalWrite(POWER_DUST, LOW);
  return;
}

/***************************************************
 * Notes
 * Currently the use of memory is quite inefficient.
 * Floats are not necessarily needed, but were ok
 * in the current case.
 * 
 * To scale this program to do more, delays should
 * be altered, and memory efficiency should be tuned.
 * The method in which circle[] is summed can be made
 * more efficient through additional logic.
 * Additionally, the variables could be static local.
*****************************************************/


