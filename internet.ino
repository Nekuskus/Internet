#include <hardware/pio.h>
#include "rt.pio.h"
#include "I2CCom/I2CCom.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include <LiquidCrystal.h>

char hex[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

byte macaddress[6] = {0b10000101, 0b10010000, 0b00000000, 0b00000000, 0b00000000, 0b00000001};

uint32_t gatewayaddress;
uint32_t gatewaymask;
int lcdline = 1;
String protocols[3] = {"DHCP", "ICMP", "SEND"};


LiquidCrystal lcd(12, 11, 10, 9, 8, 7);


class L7Data {
public:
  byte data[10];
  L7Data() {
    
  }
  L7Data(byte _data[10]) {
    memcpy((void*) data, (const void *) _data, sizeof(data));
  }
};
class L6Data {
public:
  byte protocolid;
  //0 DHCP
  //1 ICMP
  //2 SEND
  L7Data L7;
  L6Data() {
    
  }
  L6Data(byte _protocolid, L7Data l7) {
    protocolid = _protocolid;
    L7 = l7;
  }
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
class L4DatagramUDP {
public:
  bool istcp = false;
  byte destinationport;
  byte sourceport;
  // 1 = apka od przytrzymywania przycisku
  // dhcp: source=68, dest=67
  // 0-254 = adresy aplikacji, np w I2CCom
  L6Data L6;
  L4DatagramUDP() {
    
  }
  L4DatagramUDP(byte _destinationport, byte _sourceport, L6Data l6, bool _istcp = false) {
    destinationport = _destinationport;
    sourceport = _sourceport;
    L6 = l6;
    istcp = _istcp;
  }
};
class L3PacketUDP {
public:
  uint32_t destinationip;
  uint32_t sourceip;
  uint32_t sourcemask;
  byte TTL;
  L4DatagramUDP L4;
  L3PacketUDP() {
  
  }
  L3PacketUDP(uint32_t _destinationip, uint32_t _sourceip, uint32_t _sourcemask, byte _TTL, L4DatagramUDP l4) {
    destinationip = _destinationip;
    sourceip = _sourceip;
    sourcemask = _sourcemask;
    TTL = _TTL;
    L4 = l4;
  }
};
class L2FrameUDP {
public:
  byte destinationmac[6];
  byte sourcemac[6];
  L3PacketUDP L3;
  uint32_t crc;
  L2FrameUDP() {
    
  }
  L2FrameUDP(byte _destinationmac[6], byte _sourcemac[6], L3PacketUDP l3, uint32_t _crc = 0) {
    memcpy((void *)&destinationmac, (const void *)&_destinationmac, sizeof(destinationmac));
    memcpy((void *)&sourcemac, (const void *)&_sourcemac, sizeof(sourcemac));  
  }
};

//------------------------------------------
//---------UDP PROTOCOL FUNCTIONS-----------
//------------------------------------------

uint32_t calculatecrc32(L2FrameUDP frame) {
  byte* bytes = (byte*)&frame;
  const uint32_t polynomial = 0x04C11DB7;
  uint32_t crc = 0xFFFFFFFF;
  for (int i = 0; i < sizeof(frame) - 4; i++)
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
  lcd.setCursor(0, 0);
  for (byte x : macaddress) {
    lcd.print(hex[(x >> 4) & 0b1111]);
    lcd.print(hex[x & 0b1111]);
  }
  lcd.setCursor(0, 1);
  //pio_sm_put_blocking(pio, sm_transfer, 0b00100111111111111111111111111111);

  byte dhcpdata[10] = {53, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0}; // DHCPDISCOVER
  
  L7Data l7(dhcpdata);
  //memcpy(&(l7.data), &dhcpdata, sizeof(dhcpdata));
  
  L6Data l6(0, l7);
  //l6.protocolid = 0; //dhcp
  //l6.L7 = l7;
  
  L4DatagramUDP l4(67, 68, l6, false);
  //l4.destinationport = 67;
  //l4.sourceport = 68;
  //l4.L6 = l6;
  //l4.istcp = false;

  L3PacketUDP l3(0xFFFFFFFF, 0x00000000, 0x00000000, 18, l4);
  //l3.destinationip    = 0xFFFFFFFF;
  //l3.sourceip         = 0x00000000;
  //l3.sourcemask       = 0x00000000;
  //l3.TTL = 18;
  //l3.L4 = l4;

  byte destinationmac[6]  = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  byte sourcemac[6]       = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6};
  L2FrameUDP l2(destinationmac, sourcemac, l3, 0x0);
  //memcpy((void *)&(l2.destinationmac), (const void *)&destinationmac, sizeof(destinationmac));
  //memcpy((void *)&(l2.sourcemac), (const void *)&sourcemac, sizeof(destinationmac));
  //l2.crc = 0x7;
  //l2.crc = calculatecrc32(l2);

  sendpacket(l2);
}


void sendpacket(L2FrameUDP l2) {
  lcd.setCursor(0, lcdline);
  lcd.print('>');
  lcd.print(l2.L3.TTL);
  lcd.print(' ');
  //for(int i = 6; i >= 0; i-=2) {
   // lcd.print((l2.L3.destinationip >> (i*8)) & 0b11111111, DEC);
   // if(i != 0) lcd.print(".");
  //}
  lcd.print(l2.L3.destinationip);
  lcdline = (lcdline == 0) ? 1 : 0;
}

void loop() {

}


void loopreceive() {
  while (true) {
    pio_sm_get_blocking(pio, sm_receive);
  }
}
