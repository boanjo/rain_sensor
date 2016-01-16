//Using the watchdog timer, sleep for the given number of intervals,
//Using an ATmega328P at 16MHz and 5V, draws ~6.3µA while sleeping, which
//is consistent with only the WDT running.
//
//Jack Christensen 19 Nov 2013
//CC BY-SA, see http://creativecommons.org/licenses/by-sa/3.0/

#include <avr/wdt.h>
#include <avr/sleep.h>
#include <util/atomic.h>
#include "osv3_pcr800_sensor.h"

// Using a Brage SIP lite board (a arduino uno like board)

const int RAIN_GAUGE_PIN = 2;           
const int WDT_INTERVALS = 6;     //number of WDT timeouts before sending result
const int TX_GND_PIN = 5;        //Connection to gnd to brage

// Channel (first argument) is hardcoded to not have any extra processing
// at the time of sending data. A switch could of course be used and only check
// when starting up (reset if channel changed)
Osv3Pcr800Sensor pcr800(2,7,6);  

volatile boolean extInterrupt;    //external interrupt flag (rain gauge tip...)
volatile boolean wdtInterrupt;    //watchdog timer interrupt flag
volatile unsigned long count = 0;

void wakeOnInterrupt ()
{
  extInterrupt = true;
  
  // don't need the external interrupts any more
  detachInterrupt (0);  
}
  
void setup(void)
{
  pinMode(TX_GND_PIN, OUTPUT);
  digitalWrite(TX_GND_PIN, LOW);
  
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
      
      // Rainwise 111 counts 1/100" per tip i.e. 0.01" per tip
      // Oregon scientific PCR800 (the protocol) reports in 1/1000" per tip
      // so total rain is just reported as number of tips * 10
      unsigned long totalRain = count * 10;
      unsigned int rainRate = count;
      pcr800.buildAndSendPacket(rainRate, totalRain);
      wdtCount = 0;
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
