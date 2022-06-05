#include <hardware/pio.h>
#include "rt.pio.h"
#include "I2CCom/I2CCom.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include <LiquidCrystal.h>

char hex[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
  
byte macaddress[6] = {0b10000101, 0b10010000, 0b00000000, 0b00000000, 0b00000000, 0b00000001};

LiquidCrystal lcd(12, 11, 10, 9, 8, 7);


struct L7Data {
  byte data[5];
};
struct L6Data {
  byte protocolid;
  //0 REGISTER
  //1 ICMP
  //2 SEND
  L7Data L7;
};

/*
//------------------------------------------
//----------TCP PROTOCOL STRUCTS------------
//------------------------------------------
struct L4SegmentTCP {
  bool istcp = true;
  byte destinationport;
  byte sourceport;
  L6Data L6;
};
struct L3PacketTCP {
  uint32_t destinationip;
  uint32_t sourceip;
  L4SegmentTCP L4;
};
struct L2FrameTCP {
  uint8_t framestart = 0b11111111;
  uint8_t destinationmac;
  uint8_t sourcemac;
  L3PacketTCP L3;
  uint32_t crc;
};

//------------------------------------------
//---------TCP PROTOCOL FUNCTIONS-----------
//------------------------------------------

uint32_t calculatecrc32(L2FrameTCP frame) {
    byte* bytes = (byte*)&frame;
    const uint32_t polynomial = 0x04C11DB7;
    uint32_t crc = 0xFFFFFFFF;
    for(int i = 0; i < sizeof(frame) - 4; i++)
    {
        crc ^= (uint)(bytes[i] << 24);
        for (int i = 0; i < 8; i++)
        {
            if ((crc & 0x80000000) != 0)
            {
                crc = (uint)((crc << 1) ^ polynomial);
            }
            else
            {
                crc <<= 1;
            }
        }
    }

    return crc;
}

void writeEth(L2FrameTCP frame) {
  return;  
}

*/



//------------------------------------------
//----------UDP PROTOCOL STRUCTS------------
//------------------------------------------
struct L4DatagramUDP {
  bool istcp = false;
  byte destinationport;
  byte sourceport;
  // 255 = main, odpowiada za icmp i takie tam
  // 0-254 = adresy aplikacji, np w I2CCom
  L6Data L6;
};
struct L3PacketUDP {
  uint32_t destinationip;
  uint32_t sourceip;
  L4DatagramUDP L4;
};
struct L2FrameUDP {
  byte destinationmac[6];
  byte sourcemac[6];
  L3PacketUDP L3;
  uint32_t crc;
};

//------------------------------------------
//---------UDP PROTOCOL FUNCTIONS-----------
//------------------------------------------

uint32_t calculatecrc32(L2FrameUDP frame) {
    byte* bytes = (byte*)&frame;
    const uint32_t polynomial = 0x04C11DB7;
    uint32_t crc = 0xFFFFFFFF;
    for(int i = 0; i < sizeof(frame) - 4; i++)
    {
        crc ^= (uint)(bytes[i] << 24); /* move byte into MSB of 32bit CRC */
        for (int i = 0; i < 8; i++)
        {
            if ((crc & 0x80000000) != 0) /* test for MSB = bit 31 */
            {
                crc = (uint)((crc << 1) ^ polynomial);
            }
            else
            {
                crc <<= 1;
            }
        }
    }

    return crc;
}

void writeEth(L2FrameUDP frame) {
  return;  
}

uint8_t readEth() {
  return 0;
}

volatile PIO pio = pio0;
volatile uint offset0 = pio_add_program(pio, &transfer_program);
volatile uint offset1 = pio_add_program(pio,  &receive_program);
volatile uint sm_transfer = pio_claim_unused_sm(pio, true);
volatile uint sm_receive  = pio_claim_unused_sm(pio, true);


void setup() {
  // data pins
  pinMode(1, OUTPUT);
  pinMode(2, OUTPUT);
  pinMode(3, OUTPUT);
  pinMode(4, OUTPUT);
  pinMode(5, OUTPUT);
  pinMode(6, OUTPUT);

  // lcd pins
  pinMode(12, OUTPUT); //RS
  pinMode(11, OUTPUT); //ENABLE
  pinMode(10, OUTPUT); //D4
  pinMode(9, OUTPUT);  //D5
  pinMode(8, OUTPUT);  //D6
  pinMode(7, OUTPUT);  //D7
  //pinMode(26, OUTPUT); // V0 CONTRAST
  //analogWrite(26, 60);
  
  // function buttons
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
  pinMode(15, INPUT_PULLUP);
  
  pinMode(LED_BUILTIN, OUTPUT);
  transfer_program_init(pio, sm_transfer, offset0, 0);
  receive_program_init (pio, sm_receive,  offset1, 3);
  multicore_launch_core1(loopreceive);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(2000);
  digitalWrite(LED_BUILTIN, LOW);
  
  lcd.begin(16, 2);
  lcd.clear();
  lcd.setCursor(0,0);
  for(byte x : macaddress) {
    lcd.print(hex[(x >> 4) & 0b1111]);
    lcd.print(hex[x & 0b1111]);
  }
  lcd.setCursor(0,1);
  lcd.write("<PING 10.0.0.2");
  lcd.setCursor(0,0);
  pio_sm_put_blocking(pio, sm_transfer, 0b00100111111111111111111111111111);
}

void loop() {
  //pio_sm_put_blocking(pio, sm_transfer, 0b11111111111111111111111111111111);
  //pio_sm_put_blocking(pio, sm_transfer, 0b001010101010101010101010101011111);
}


void loopreceive() {
  while(true) {
    pio_sm_get_blocking(pio, sm_receive);
  }
}
