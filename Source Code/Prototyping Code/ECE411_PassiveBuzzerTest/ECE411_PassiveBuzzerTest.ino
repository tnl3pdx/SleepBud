//Ameer Melli
//11/2/24
//Code for passive buzzer

int buzzerPin = 21;

void setup() {
  pinMode(buzzerPin, OUTPUT);

  tone(buzzerPin, 1000, 2000);
}

void loop() {
  tone(buzzerPin, 440); // A4
  delay(1000);

  tone(buzzerPin, 494); // B4
  delay(1000);

  tone(buzzerPin, 523); // C4
  delay(1000);

  tone(buzzerPin, 587); // D4
  delay(1000);

  tone(buzzerPin, 659); // E4
  delay(1000);

  tone(buzzerPin, 698); // F4
  delay(1000);

  tone(buzzerPin, 784); // G4
  delay(1000);

  noTone(buzzerPin);
  delay(1000);
}





/*
//Code for passive buzzer (should work untested because we dont have the materials yet)
//Does not work with Passive buzzer

int buzzerPin = 21;

void setup() {
  pinMode(buzzerPin, OUTPUT);
}

void loop() {

  digitalWrite(buzzerPin, HIGH);

  delay(1000);
  digitalWrite(buzzerPin, LOW);
}
*/

