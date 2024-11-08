//Code to test PIR sensor

int sensor = 21;              // the pin that the sensor is attached to
int state = LOW;              // by default, no motion detected
int val = 0;                  // variable to store the sensor status (value)

void setup() {
  pinMode(sensor, INPUT);     // initialize sensor as an input
  Serial.begin(115200);         // initialize serial
}

void loop(){
  val = digitalRead(sensor);  // read sensor value
  if (val == HIGH) {          // check if the sensor is HIGH
    //delay(100);               // delay 100 milliseconds 
    
    if (state == LOW) {
      Serial.println("Motion detected!"); 
      state = HIGH;           // update variable state to HIGH
    }
  } 
  else {
    //delay(200);               // delay 200 milliseconds 
    
    if (state == HIGH) {
      Serial.println("Motion stopped!");
      state = LOW;            // update variable state to LOW
    }
  }
}

