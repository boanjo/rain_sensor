//Using the watchdog timer, sleep for the given number of intervals,
//Using an ATmega328P at 16MHz and 5V, draws ~6.3µA while sleeping, which
//is consistent with only the WDT running.
//
//Jack Christensen 19 Nov 2013
//CC BY-SA, see http://creativecommons.org/licenses/by-sa/3.0/

#include <avr/wdt.h>
#include <avr/sleep.h>
#include <avr/eeprom.h>
#include <util/atomic.h>
#include "osv3_pcr800_sensor.h"

#define EE_COUNT_ADDRESS  0   // Select address in EEPROM memory space

#define DEBUG 1
//#define FIRST_TIME 1

const int RAIN_GAUGE_PIN = 2;           
const int WDT_INTERVALS = 6;     //number of WDT timeouts before sending result
const int BATTERY_INTERVALS = 10; //number of reporting periods between every new battery read

// Channel (first argument) is hardcoded to not have any extra processing
// at the time of sending data. A switch could of course be used and only check
// when starting up (reset if channel changed)
Osv3Pcr800Sensor pcr800(2,7,6);  

volatile boolean extInterrupt;    //external interrupt flag (rain gauge tip...)
volatile boolean wdtInterrupt;    //watchdog timer interrupt flag
volatile unsigned long count = 0;
volatile uint32_t prevStoredCount = 0;
volatile unsigned long reportingPeriodCount = BATTERY_INTERVALS;
volatile unsigned long batteryPercent = 0;
int BATTERY_SENSE_PIN = A0;

void wakeOnInterrupt ()
{
  extInterrupt = true;
  
  // don't need the external interrupts any more
  detachInterrupt (0);  
}
  
void setup(void)
{
#ifdef DEBUG
  Serial.begin(115200);
#endif

#ifdef FIRST_TIME
  eeprom_write_dword((uint32_t *)EE_COUNT_ADDRESS, (uint32_t)0);
#endif

  analogReference(INTERNAL);
  pinMode(BATTERY_SENSE_PIN, INPUT);

  count = (unsigned long) eeprom_read_dword((const uint32_t *)EE_COUNT_ADDRESS);
  prevStoredCount = count;
  
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    wdt_reset();
    MCUSR &= ~bit(WDRF);                            //clear WDRF
    WDTCSR |= bit(WDCE) | bit(WDE);                 //enable WDTCSR change
    WDTCSR =  bit(WDIE) | bit(WDP3) | bit(WDP0);    //~8 sec
  }
}

void loop(void)
{
  static int wdtCount;
  
  gotoSleep();
  
  if (wdtInterrupt) {
    if (++wdtCount >= WDT_INTERVALS) {

      // measure battery more infrequent than reporting
      if(reportingPeriodCount >= BATTERY_INTERVALS)
      {
         int sensorValue = analogRead(BATTERY_SENSE_PIN);
         #ifdef DEBUG
         Serial.println(sensorValue);
         #endif
   
         // 2,7M, 820K divider across battery and using internal ADC ref of 1.1V
         // ((2,7e6+820e3)/820e3)*1.1 = Vmax = 4,72 Volts
         // 4,72/1023 = Volts per bit = 0.004615788
         // float batteryV  = sensorValue * 0.004615788;
         batteryPercent = sensorValue / 10;
      }

      // Rainwise 111 counts 1/100" per tip i.e. 0.01" per tip
      // Oregon scientific PCR800 (the protocol) reports in 1/1000" per tip
      // so total rain is just reported as number of tips * 10
      unsigned long totalRain = count * 10;
      unsigned int rainRate = count;
      pcr800.buildAndSendPacket(rainRate, totalRain, batteryPercent);
      wdtCount = 0;


      if(count != prevStoredCount) {
          eeprom_write_dword((uint32_t *)EE_COUNT_ADDRESS, (uint32_t)count);
          prevStoredCount = count; 
      }
    }
  }
  if (extInterrupt) {
    if((digitalRead (RAIN_GAUGE_PIN) == LOW) ) {
      count++;
    }        
  }
}

void gotoSleep(void)
{
  byte adcsra = ADCSRA; // save the ADC Control and Status Register A
  ADCSRA = 0;           // disable the ADC
  noInterrupts ();      // timed sequences follow
  EIFR = bit (INTF0);   // clear flag for interrupt 0
  attachInterrupt (0, wakeOnInterrupt, FALLING);
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  wdtInterrupt = false;
  extInterrupt = false;
  sleep_enable();

  // turn off the brown-out detector while sleeping
  byte mcucr1 = MCUCR | bit(BODS) | bit(BODSE); 
  byte mcucr2 = mcucr1 & ~bit(BODSE);
  MCUCR = mcucr1;    //timed sequence
  // BODS stays active for 3 cycles, sleep instruction must 
  // be executed while it's active
  MCUCR = mcucr2;                
                                 
  interrupts ();     // need interrupts now
  sleep_cpu();       // go to sleep
  sleep_disable();   //wake up here
  ADCSRA = adcsra;   //restore ADCSRA
}

//handles the Watchdog Time-out Interrupt
ISR(WDT_vect)
{
  wdtInterrupt = true;
}
