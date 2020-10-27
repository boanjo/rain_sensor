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
#define EE_ROLLING_ADDRESS  1   // Select address in EEPROM for Rolling code
#define EE_CHANNEL_ADDRESS  2   // Select address in EEPROM for last 

// Enabling DEBUG will change the timing
//#define DEBUG 1


const int DIP_1_PIN = 10;           
const int DIP_2_PIN = 11;           
const int DIP_3_PIN = 12;           
const int RAIN_GAUGE_PIN = 2;           
const int WDT_INTERVALS = 6;     //number of WDT timeouts before sending result
const int BATTERY_INTERVALS = 30; //number of reporting periods between every new battery read

//Data pin 7
Osv3Pcr800Sensor pcr800(7);  

volatile boolean extInterrupt;    //external interrupt flag (rain gauge tip...)
volatile boolean wdtInterrupt;    //watchdog timer interrupt flag
volatile unsigned long count = 0;
volatile uint32_t prevStoredCount = 0;
volatile unsigned long reportingPeriodCount = BATTERY_INTERVALS;
volatile bool batteryLow = false;
unsigned long rolling_code = 0; 
unsigned int channel = 0xFF;
int batteryValue = 0;
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
  pinMode(DIP_1_PIN, INPUT_PULLUP);
  pinMode(DIP_2_PIN, INPUT_PULLUP);
  pinMode(DIP_3_PIN, INPUT_PULLUP);
  unsigned int prevChannel = (unsigned long) eeprom_read_dword((const uint32_t *)EE_CHANNEL_ADDRESS);

  channel = 0;
  if(digitalRead(DIP_1_PIN) == LOW) channel |= 0x4;
  if(digitalRead(DIP_2_PIN) == LOW) channel |= 0x2;
  if(digitalRead(DIP_3_PIN) == LOW) channel |= 0x1;

  // We have switched channel -> reset all data
  if(channel != prevChannel)
  {
    eeprom_write_dword((uint32_t *)EE_CHANNEL_ADDRESS, (uint32_t)channel);
    eeprom_write_dword((uint32_t *)EE_COUNT_ADDRESS, (uint32_t)0);
    eeprom_write_dword((uint32_t *)EE_ROLLING_ADDRESS, (uint32_t)0);
  }

#ifdef DEBUG
  Serial.print("channel=");
  Serial.println(channel);
#endif
 

  // Number of Bucket tips
  count = (unsigned long) eeprom_read_dword((const uint32_t *)EE_COUNT_ADDRESS);
  prevStoredCount = count;
  
  // Rolling code is really a restart counter
  rolling_code = (unsigned long) eeprom_read_dword((const uint32_t *)EE_ROLLING_ADDRESS);
  rolling_code++;

  if(rolling_code >= 100) 
  {
    rolling_code = 1;
  }
  eeprom_write_dword((uint32_t *)EE_ROLLING_ADDRESS, (uint32_t)rolling_code);

  
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
  if (extInterrupt) {
    if((digitalRead (RAIN_GAUGE_PIN) == LOW) ) {
      count++;
    }
  }

  if (wdtInterrupt) {
    if (++wdtCount >= WDT_INTERVALS) {

      // measure battery more infrequent than reporting
      if(reportingPeriodCount >= BATTERY_INTERVALS)
      {

        batteryValue = readVcc();
        #ifdef DEBUG
        Serial.print("batteryValue=");
        Serial.println(batteryValue);
        #endif

          // I consider 3.8V to be minimum
         if(batteryValue > 3800) 
           batteryLow = false;
         else 
           batteryLow = true;
      }

      // Rainwise 111 counts 1/100" per tip i.e. 0.01" per tip
      // Oregon scientific PCR800 (the protocol) reports in 1/1000" per tip
      // so total rain is just reported as number of tips * 10
      unsigned long totalRain = count * 10;

      // The rain_rate is * 0.254 on the receiver side but we use the rain rate to 
      // send the actual voltage in millivolt / 10 (*4 instead need of double /0.254)
      // max value that can be sent i 9999 (4 digit for rate)
      unsigned int rainRate = batteryValue / 10;
      rainRate = rainRate*4;
      pcr800.buildAndSendPacket(channel, rainRate, totalRain, batteryLow, rolling_code);
      wdtCount = 0;

      #ifdef DEBUG
       Serial.print("Total Rain=");
       Serial.println(totalRain);
       Serial.flush();
       #endif

      if(count != prevStoredCount) {
          eeprom_write_dword((uint32_t *)EE_COUNT_ADDRESS, (uint32_t)count);
          prevStoredCount = count; 
      }
    }
  }
}

long readVcc() {
  // Read 1.1V reference against AVcc
  // set the reference to Vcc and the measurement to the internal 1.1V reference
  #if defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
    ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  #elif defined (__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__)
    ADMUX = _BV(MUX5) | _BV(MUX0);
  #elif defined (__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__)
    ADMUX = _BV(MUX3) | _BV(MUX2);
  #else
    ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  #endif  
  
  delay(2); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Start conversion
  while (bit_is_set(ADCSRA,ADSC)); // measuring
 
  uint8_t low  = ADCL; // must read ADCL first - it then locks ADCH  
  uint8_t high = ADCH; // unlocks both
 
  long result = (high<<8) | low;
 
  result = 1125300L / result; // Calculate Vcc (in mV); 1125300 = 1.1*1023*1000
  return result; // Vcc in millivolts
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
