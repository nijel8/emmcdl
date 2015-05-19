
#ifdef _WIN32
#ifdef _WIN32
#include <Windows.h>
#endif
#endif

#define CRC_16_L_SEED         0xFFFF
extern const unsigned short crc_16_l_table[];

#define CRC_16_L_STEP(xx_crc,xx_c) \
  (((xx_crc) >> 8) ^ crc_16_l_table[((xx_crc) ^ (xx_c)) & 0x00ff])

unsigned short CalcCRC16(unsigned char *buf, int length);