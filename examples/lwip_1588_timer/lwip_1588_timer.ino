#include "lwip_t41.h"

elapsedMillis msec;

// initialize the ethernet hardware
void setup()
{
  while (!Serial) ; // wait
  Serial.println("Ethernet 1588 Timer Test");
  Serial.println("------------------------\n");
  
  enet_init(NULL, NULL, NULL);

  msec = 0;
}

void sample() {
    Serial.print(read_1588_timer());
    Serial.print(" at ");
    Serial.println(msec);
}

// watch for data to arrive
void loop()
{
  if (msec >= 1000) {
    sample();
    msec = 0;
  }
  if (msec == 500) {
    sample();
    delay(1);
  }
}
