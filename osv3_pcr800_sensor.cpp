// base class for simulating a Pcr800 sensor
// sensors derive from this and use it to build and send the data packets

// the packet is built with the required Pcr800 fields
// checksums and CRCs are calculated
// the data is manchester encoded and sent to the transmitter

#include "Arduino.h"
#include "osv3_pcr800_sensor.h"
#include "math.h"
Osv3Pcr800Sensor::Osv3Pcr800Sensor(int channel, int transmitterPin, int transmitterPowerPin)
{
  m_channel = channel;
  m_madeTable = false;
  m_transmitterPin = transmitterPin;
  m_transmitterPowerPin = transmitterPowerPin;
  
  // new rolling code with every reset
  randomSeed(analogRead(1)); // analog pin one used as source of noise for random seed
  m_rollingCode = random(0x01, 0xFE);
  
  pinMode(m_transmitterPin, OUTPUT);
  pinMode(m_transmitterPowerPin, OUTPUT);

  digitalWrite(m_transmitterPowerPin, LOW);
  //digitalWrite(m_transmitterPowerPin, HIGH);
  
  initCrc8();
}

Osv3Pcr800Sensor::~Osv3Pcr800Sensor()
{
}

void Osv3Pcr800Sensor::buildAndSendPacket(const unsigned int rainRate, const unsigned long totalRain)
{  
  // Nibbles are sent LSB first

  // --- preamble ---
  // The preamble consists of twenty four '1' bits (6 nibbles) for v3.0 sensors
  m_packet[0] = 0xFF;
  m_packet[1] = 0xFF;
  m_packet[2] = 0xFF;

  // A sync nibble (4-bits) which is '0101'
  m_packet[3] = 0xA0;

  // --- payload ---
  // sending Pcr800 packet
  // nibbles 0..3 Sensor ID This 16-bit value is unique to each sensor, or sometimes a group of sensors.
  m_packet[3] |= 0x02;
  m_packet[4] = 0x91;
  m_packet[5] = 0x40;

  // nibble 4 Channel 1 thru 15
  m_packet[5] |= m_channel;

  // nibbles 5..6 Rolling Code Value changes randomly every time the sensor is reset
  m_packet[6] = m_rollingCode;

  // nibble 7 Flags - battery status
  // my sensor is plugged in, so power is always good
  m_packet[7] = 0x80;

  // nibble 8..[n-5] Sensor-specific Data
  // Rain Rate (4 nibbles)
  // nibbles 11..8 in 0.01 inches per hour

  m_packet[7] |= (rainRate) % 10;
  m_packet[8]  = ((rainRate / 10) % 10) << 4;
  m_packet[8] |= (rainRate / 100) % 10;
  m_packet[9]  = ((rainRate / 1000) % 10) << 4;
  
  m_packet[9]  |= (totalRain) % 10;
  m_packet[10]  = ((totalRain / 10) % 10) << 4;
  m_packet[10] |= (totalRain / 100) % 10;
  m_packet[11]  = ((totalRain / 1000) % 10) << 4;
  m_packet[11] |= (totalRain / 10000) % 10;
  m_packet[12]  = ((totalRain / 100000) % 10) << 4;


  // Total Rain  (6 nibbles)
  // nibbles 18..13 in 0.001 inches 
  
  // nibbles [n-3]..[n-4] Checksum The 8-bit sum of nibbles 0..[n-5]
    
  unsigned char checksum = calcChecksum();
    
  // Checksum
  m_packet[12]  |= checksum & 0x0F;
  m_packet[13] = checksum & 0xF0;

  // CRC-8
 // m_packet[14] = calcCrc();

  // send high value
  sendData();
}

unsigned char Osv3Pcr800Sensor::calcChecksum(void)
{
  unsigned char checksum = 0;

  checksum += m_packet[3] & 0x0F;
  for (int i=4; i<12; i++)
  {
    checksum += (m_packet[i] >> 4) & 0x0F;
    checksum += (m_packet[i] & 0x0F);
  }
  checksum += (m_packet[12] >> 4) & 0x0F;

  // nibble swap
//  return ((checksum & 0x0F) << 4) | ((checksum & 0xF0) >> 4);
return checksum;
}

unsigned char Osv3Pcr800Sensor::calcCrc()
{
  unsigned char crc = 0;

  // does not include all of byte at index 3 (skip the sync pattern 0xA)
  crc8(&crc, m_packet[3] & 0x0F);

  // includes bytes at indicies 4 through 10, does not include checksum in index 11   
  for (int i=4; i<=10; i++)
  {
     crc8(&crc, m_packet[i]);
  }

  // nibble swap
  return ((crc & 0x0F) << 4) | ((crc & 0xF0) >> 4);;  
}

void Osv3Pcr800Sensor::sendData(void)
{
   int i;
  
  // power on transmitter
  digitalWrite(m_transmitterPowerPin, HIGH);
  delay(60); // wait 60ms for transmitter to get ready
  
  digitalWrite(m_transmitterPin, LOW);
  delayMicroseconds(2000); 
    
  for (i=0; i < OSV3_PCR800_PACKET_LEN; i++)
  {
    manchesterEncode(m_packet[i], (i+1 == OSV3_PCR800_PACKET_LEN));
  }
  
  // power off transmitter
  digitalWrite(m_transmitterPowerPin, LOW);
  digitalWrite(m_transmitterPin, LOW);
}


void Osv3Pcr800Sensor::manchesterEncode (unsigned char encodeByte, bool lastByte)
{
  unsigned char mask = 0x10;
  int loopCount;
  static int lastbit = 0;
  static unsigned long baseMicros = micros();

  //  488us timing would be 1024 data rate
  // the data rate actually appears to be 1020 or 490us
  // if the timing is off, then the base station won't receive packets reliably
  const unsigned int desiredDelay = 490;

  // due to processing time, the delay shouldn't be a full desired delay time
  const unsigned int shorten = 32;
  
  // bits are transmitted in the order 4 thru 7, then 0 thru 3
  
  for (loopCount = 0; loopCount < 8; loopCount++)
  {  
    baseMicros += desiredDelay;
    unsigned long delayMicros = baseMicros - micros();
    
    if (delayMicros > 2*delayMicros)
    {
      // big delay indicates break between packet transmits, reset timing base
      baseMicros = micros();
    }
    else if (delayMicros > 0) 
    {
      delayMicroseconds(delayMicros); 
    }
    
    if ((encodeByte & mask) == 0)
    {
      // a zero bit is represented by an off-to-on transition in the RF signal
      digitalWrite(m_transmitterPin, LOW);
      delayMicroseconds(desiredDelay - shorten); 
      digitalWrite(m_transmitterPin, HIGH);
      
      // no long delay after last low to high transistion as no more data follows
      if (lastByte) delayMicroseconds(desiredDelay); 

      lastbit = 0;
    }
    else
    {
      digitalWrite(m_transmitterPin, HIGH);
      delayMicroseconds(desiredDelay - shorten); 
      digitalWrite(m_transmitterPin, LOW);
      lastbit = 1;
    }

    if (mask == 0x80)
    {
      mask = 0x01;
    }
    else
    {
      mask <<= 1;
    }

    baseMicros += desiredDelay;
  }
}  



// this CRC code is from crc8.c published by Rajiv Chakravorty
void Osv3Pcr800Sensor::initCrc8(void)
     /*
      * Should be called before any other crc function.  
      */
{
  int i,j;
  unsigned char crc;
  
  if (!m_madeTable) 
  {
    for (i=0; i<256; i++) 
    {
      crc = i;
      for (j=0; j<8; j++)
      {
        crc = (crc << 1) ^ ((crc & 0x80) ? DI : 0);
      }
      
      m_crc8Table[i] = crc & 0xFF;
    }
    
    m_madeTable = true;
  }
}

void Osv3Pcr800Sensor::crc8(unsigned char *crc, unsigned char m)
     /*
      * For a byte array whose accumulated crc value is stored in *crc, computes
      * resultant crc obtained by appending m to the byte array
      */
{
  if (!m_madeTable)
    initCrc8();

  *crc = m_crc8Table[(*crc) ^ m];
  *crc &= 0xFF;
}


