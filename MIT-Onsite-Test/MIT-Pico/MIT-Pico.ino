#include "cnt.h"
#include "i2c_init.h"
#include "tiny4fsk.h"

CNTData currents_test;

void setup() {
    Serial.begin(115200);
    cnt_begin();
    i2c_slave_begin();
}

void loop() {
    //cnt_loop();
    //delayMicroseconds(100);
    cnt_scan_once();
    cnt_data_load(currents_test);
    Serial.print("CNT1 Values: ");
    Serial.print(currents_test.cnt1.val1);
    Serial.print(',');
    Serial.print(currents_test.cnt1.val2);
    Serial.print(',');
    Serial.print(currents_test.cnt1.val3);
    Serial.print(',');
    Serial.print(currents_test.cnt1.val4);
    Serial.print(", CNT2 Values: ");
    Serial.print(currents_test.cnt2.val1);
    Serial.print(',');
    Serial.print(currents_test.cnt2.val2);
    Serial.print(',');
    Serial.print(currents_test.cnt2.val3);
    Serial.print(',');
    Serial.print(currents_test.cnt2.val4);
    Serial.print(", CNT3 Values: ");
    Serial.print(currents_test.cnt3.val1);
    Serial.print(',');
    Serial.print(currents_test.cnt3.val2);
    Serial.print(',');
    Serial.print(currents_test.cnt3.val3);
    Serial.print(',');
    Serial.print(currents_test.cnt3.val4);
    Serial.println();
}