#ifndef __OSV3_PCR800_SENSOR_H_
#define __OSV3_PCR800_SENSOR_H_

// base class for simulating a THGR810 sensor
// sensors derive from this and use it to build and send the data packets
// class also contains the timing for each THGR810 channel
#define PREAMBLE_NIBBLES                6
#define SYNCH_NIBBLES                   1
#define PAYLOAD_STARTING_NIBBLE         PREAMBLE_NIBBLES + SYNCH_NIBBLES
#define PAYLOAD_NIBBLES                20  // Including CHECKSUM NIBBLES
#define CHECKSUM_NIBBLES                2
#define OSV3_PCR800_PACKET_LEN         15

// crc constants
#define GP  0x107   /* x^8 + x^2 + x + 1 */
#define DI  0x07


class Osv3Pcr800Sensor
{
  private:
    // channel effects packet tx frequency
    unsigned char m_packet[OSV3_PCR800_PACKET_LEN];
    unsigned int m_rollingCode;

    int m_transmitterPin;

    // having a CRC table in each sensor isn't the most efficient...
    unsigned char m_crc8Table[256];     /* 8-bit table */
    int m_madeTable;
    unsigned char calcChecksum(void);
    unsigned char calcCrc(void);
    void sendData(void);
    void manchesterEncode (unsigned char encodeByte, bool lastByte);
    void initCrc8(void);
    void crc8(unsigned char *crc, unsigned char m);


  public:
    Osv3Pcr800Sensor(int transmitterPin);
    ~Osv3Pcr800Sensor();

    void buildAndSendPacket(const unsigned int channel, const unsigned int countPerHour, const unsigned long totalCount, const bool batteryLow, const unsigned long rollingCode);

};

#endif
