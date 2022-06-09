//#include <hardware/pio.h>
//#include "rt.pio.h"
#include "I2CCom/I2CCom.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include <LiquidCrystal.h>
#include <bitset>
//#include "hardware/watchdog.h"
#include "hardware/uart.h"
#include "pico/util/queue.h"

#define UART_ID uart0
#define BAUD_RATE 9600
#define UART_TX_PIN 0
#define UART_RX_PIN 1

volatile bool is_writing = false;

queue_t receivedpackets;

char hex[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

byte macaddress[6] = {0b10000101, 0b10010000, 0b00000000, 0b00000000, 0b00000000, 0x1};

uint32_t gatewayaddress;
uint8_t gatewaymac[6];
uint32_t ipaddress;
uint32_t ipmask;

uint32_t otherhostip = 0;

volatile int lcdline = 1;
String protocols[4] = {"DHCP", "ICMP", "SEND", "ARP "};

bool got_ip_dhcp = false;
bool got_offer_dhcp = false;

LiquidCrystal lcd(12, 11, 10, 9, 8, 7);


class L7Data {
  public:
    byte data9;
    byte data8;
    byte data7;
    byte data6;
    byte data5;
    byte data4;
    byte data3;
    byte data2;
    byte data1;
    byte data0;
    L7Data(byte _data[10]) : data0{_data[0]}, data1{_data[1]}, data2{_data[2]}, data3{_data[3]}, data4{_data[4]}, data5{_data[5]}, data6{_data[6]}, data7{_data[7]}, data8{_data[8]}, data9{_data[9]} {
      //memcpy((void*) data, (const void *) _data, sizeof(data));
    }
    L7Data& operator=(const L7Data&) = default;
};
class L6Data {
  public:
    byte protocolid;
    //0 DHCP
    //1 ICMP
    //2 SEND
    L7Data L7;
    L6Data(byte _protocolid, L7Data l7) : protocolid{_protocolid}, L7{l7} {

    }
    L6Data& operator=(const L6Data&) = default;
};

class L5Session {
  public:
    byte sessionid;
    L6Data L6;
    L5Session(byte _sessionid, L6Data l6) : sessionid{_sessionid}, L6{l6} {

    }
    L5Session& operator=(const L5Session&) = default;
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
    L5Session L5;
    L4DatagramUDP(byte _destinationport, byte _sourceport, L5Session l5, bool _istcp = false) : destinationport{_destinationport}, sourceport{_sourceport}, L5{l5}, istcp{_istcp} {

    }
    L4DatagramUDP& operator=(const L4DatagramUDP&) = default;
};
class L3PacketUDP {
  public:
    uint32_t destinationip;
    uint32_t sourceip;
    uint32_t sourcemask;
    byte TTL;
    L4DatagramUDP L4;
    L3PacketUDP(uint32_t _destinationip, uint32_t _sourceip, uint32_t _sourcemask, byte _TTL, L4DatagramUDP l4) : destinationip{_destinationip}, sourceip{_sourceip}, sourcemask{_sourcemask}, TTL{_TTL}, L4{l4} {

    }
    L3PacketUDP& operator=(const L3PacketUDP&) = default;
};
class L2FrameUDP {
  public:
    byte destinationmac0;
    byte destinationmac1;
    byte destinationmac2;
    byte destinationmac3;
    byte destinationmac4;
    byte destinationmac5;
    byte sourcemac0;
    byte sourcemac1;
    byte sourcemac2;
    byte sourcemac3;
    byte sourcemac4;
    byte sourcemac5;
    L3PacketUDP L3;
    uint32_t crc;
    L2FrameUDP(byte _destinationmac[6], byte _sourcemac[6], L3PacketUDP l3, uint32_t _crc = 0) : sourcemac0{_sourcemac[0]}, sourcemac1{_sourcemac[1]}, sourcemac2{_sourcemac[2]}, sourcemac3{_sourcemac[3]}, sourcemac4{_sourcemac[4]}, sourcemac5{_sourcemac[5]}, destinationmac0{_destinationmac[0]}, destinationmac1{_destinationmac[1]}, destinationmac2{_destinationmac[2]}, destinationmac3{_destinationmac[3]}, destinationmac4{_destinationmac[4]}, destinationmac5{_destinationmac[5]}, L3{l3}, crc{_crc} {
      //memcpy((void *)&destinationmac, (const void *)&_destinationmac, sizeof(destinationmac));
      //memcpy((void *)&sourcemac, (const void *)&_sourcemac, sizeof(sourcemac));
    }
    L2FrameUDP& operator=(const L2FrameUDP&) = default;
};
/*class L1BitsUDP {
  public:
  char* l2_bytes;
  L1BitsUDP(L2FrameUDP l2) : l2_bytes{reinterpret_cast<char*>(&l2)} {

  }
  L1BitsUDP& operator=(const L1BitsUDP&) = default;
  };*/
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

/*
volatile PIO pio = pio0;
volatile uint offset0 = pio_add_program(pio, &transfer_program);
volatile uint offset1 = pio_add_program(pio,  &receive_program);
volatile uint sm_transfer = pio_claim_unused_sm(pio, true);
volatile uint sm_receive  = pio_claim_unused_sm(pio, true);
*/

void lcdwritepacket(L2FrameUDP l2, bool incoming, int ipsystem = DEC) {
  lcd.setCursor(0, lcdline);
  lcd.print(incoming ? '<' : '>');
  lcd.print(protocols[l2.L3.L4.L5.L6.protocolid]);
  lcd.print(' ');
  byte largerthan99 = 0;
  uint32_t curip = (incoming ? l2.L3.sourceip : l2.L3.destinationip);
  if (ipsystem == -1) { //auto
    for (int i = 3; i >= 0; i--) {
      if (((curip >> (i * 8)) & 0b11111111) > 99) largerthan99++;
    }
    if (largerthan99 >= 2) {
      ipsystem = HEX;
    }
  }
  while(is_writing == true) {}
  is_writing = true;
  if(l2.L3.L4.L5.L6.protocolid != 2) { //other
    if(ipsystem == HEX) {
      lcd.print("0x");
      for (int i = 3; i >= 0; i--) {
        lcd.print((curip >> (i * 8)) & 0b11111111, HEX);
      }
    } else if (ipsystem == BIN) {
      lcd.print("0b");
      for (int i = 3; i >= 0; i--) {
        lcd.print((curip >> (i * 8)) & 0b11111111, BIN);
      }
    } else if (ipsystem == OCT) {
      lcd.print("0o");
      for (int i = 3; i >= 0; i--) {
        lcd.print((curip >> (i * 8)) & 0b11111111, OCT);
      }
    } else { //assume DEC
      for (int i = 3; i >= 0; i--) {
        lcd.print((curip >> (i * 8)) & 0b11111111, DEC);
        if (i != 0) lcd.print(".");
      }
    }
  } else { //send
    int duration = (l2.L3.L4.L5.L6.L7.data0 << (8*3)) + (l2.L3.L4.L5.L6.L7.data1 << (8*2)) + (l2.L3.L4.L5.L6.L7.data2 << (8)) + l2.L3.L4.L5.L6.L7.data3;
    lcd.print(duration);
    lcd.print('s');
  }
  lcdline = (lcdline == 0) ? 1 : 0;
  is_writing = false;
}

void sendpacketUDP(L2FrameUDP l2) {

  while(!uart_is_writable(UART_ID)) {}
  
  //std::bitset<sizeof(L2FrameUDP) * 8> packet = *(reinterpret_cast<std::bitset<sizeof(L2FrameUDP) * 8>*>(&l2));
  uint8_t* bytes = reinterpret_cast<uint8_t*>(&l2);
  //lcd.print(((unsigned int)packet[0] << 7) + ((unsigned int)packet[1] << 6) + ((unsigned int)packet[2] << 5) + ((unsigned int)packet[3] << 4) + ((unsigned int)packet[4] << 3) + ((unsigned int)packet[5] << 2) + ((unsigned int)packet[6] << 1) + (unsigned int)packet[7], BIN);
  //pio_sm_put_blocking(pio, sm_transfer, 0xFFFFFFFF);
  //int i = 0;
  //while (i < (sizeof(L2FrameUDP) * 8)) {
  //  pio_sm_put_blocking(pio, sm_transfer, (0x0 << 31) + (0x0 << 30) + (packet[i + 29] << 29) + (packet[i + 28] << 28) + (packet[i + 27] << 27) + (packet[i + 26] << 26) + (packet[i + 25] << 25) + (packet[i + 24] << 24) + (packet[i + 23] << 23) + (packet[i + 23] << 23) + (packet[i + 22] << 22) + (packet[i + 21] << 21) + (packet[i + 20] << 20) + (packet[i + 19] << 19) + (packet[i + 18] << 18) + (packet[i + 17] << 17) + (packet[i + 16] << 16) + (packet[i + 15] << 15) + (packet[i + 14] << 14) + (packet[i + 13] << 13) + (packet[i + 12] << 12) + (packet[i + 11] << 11) + (packet[i + 10] << 10) + (packet[i + 9] << 9) + (packet[i + 8] << 8) + (packet[i + 7] << 7) + (packet[i + 6] << 6) + (packet[i + 5] << 5) + (packet[i + 4] << 4) + (packet[i + 3] << 3) + (packet[i + 2] << 2) + (packet[i + 1] << 1) + packet[i]);
  //  i += 30;
  //  watchdog_update();
  //}
  //int bitsleft = 12;
  //pio_sm_put_blocking(pio, sm_transfer, (packet[(sizeof(L2FrameUDP) * 8) - 1] << 11) + (packet[(sizeof(L2FrameUDP) * 8) - 1 - 1] << 10) + (packet[(sizeof(L2FrameUDP) * 8) - 1 - 2] << 9) + (packet[(sizeof(L2FrameUDP) * 8) - 1 - 3] << 8) + (packet[(sizeof(L2FrameUDP) * 8) - 1 - 4] << 7) + (packet[(sizeof(L2FrameUDP) * 8) - 1 - 5] << 6) + (packet[(sizeof(L2FrameUDP) * 8) - 1 - 6] << 5) + (packet[(sizeof(L2FrameUDP) * 8) - 1 - 7] << 4) + (packet[(sizeof(L2FrameUDP) * 8) - 1 - 8] << 3) + (packet[(sizeof(L2FrameUDP) * 8) - 1 - 9] << 2) + (packet[(sizeof(L2FrameUDP) * 8) - 1 - 10] << 1) + packet[(sizeof(L2FrameUDP) * 8) - 1 - 11]);
  uart_write_blocking(UART_ID, bytes, sizeof(L2FrameUDP));
}


void setup() {
  /*
  // data pins
  pinMode(1, OUTPUT);
  pinMode(2, OUTPUT);
  pinMode(3, OUTPUT);
  pinMode(4, OUTPUT);
  pinMode(5, OUTPUT);
  pinMode(6, OUTPUT);
  */
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
  
  /*
  pinMode(LED_BUILTIN, OUTPUT);
  transfer_program_init(pio, sm_transfer, offset0, 0);
  receive_program_init (pio, sm_receive,  offset1, 3);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(2000);
  digitalWrite(LED_BUILTIN, LOW);
  */
  
  uart_init(UART_ID, BAUD_RATE);
  uart_set_fifo_enabled(UART_ID, true);
  
  gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
  gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
   
  //watchdog_enable(20000, 0);

  queue_init(&receivedpackets, sizeof(L2FrameUDP), 16);

  lcd.begin(16, 2);
  lcd.clear();
  lcd.setCursor(0, 0);
  for (byte x : macaddress) {
    lcd.print(hex[(x >> 4) & 0b1111]);
    lcd.print(hex[x & 0b1111]);
  }
  //if (watchdog_caused_reboot()) {
    //lcd.setCursor(0, 0);
    //lcd.print("Watchdog trigger");
  //}
  lcd.setCursor(0, 1);
  //watchdog_update();


  multicore_launch_core1(loopreceive);
  
  while(!got_ip_dhcp) {
   // watchdog_update();

    if(queue_get_level(&receivedpackets) >= 1) {
      byte temparr1[6] = {0, 0, 0, 0, 0, 0};
      byte temparr2[6] = {0, 0, 0, 0, 0, 0};
      byte temparr3[10] = {0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};

      L7Data l7x(temparr3);
      L6Data l6x(0x0, l7x);
      L5Session l5x(0x0, l6x);
      L4DatagramUDP l4x(0x0, 0x0, l5x, false);
      L3PacketUDP l3x(0x0, 0x0, 0x0, 0x0, l4x);
      L2FrameUDP received(temparr1, temparr2, l3x, 0x0);
      queue_remove_blocking(&receivedpackets, (void*)&received);
      lcdwritepacket(received, true, DEC);
      if((received.L3.L4.L5.L6.protocolid == 0) && (received.L3.L4.L5.L6.L7.data0 == 53) && (received.L3.L4.L5.L6.L7.data1 == 0x2)) {
        byte dhcpdata[10] = {53, 0x3, received.L3.L4.L5.L6.L7.data2, received.L3.L4.L5.L6.L7.data3, received.L3.L4.L5.L6.L7.data4, received.L3.L4.L5.L6.L7.data5, received.L3.L4.L5.L6.L7.data6, received.L3.L4.L5.L6.L7.data7, received.L3.L4.L5.L6.L7.data8, received.L3.L4.L5.L6.L7.data9}; // DHCPREQUEST
        L7Data l7(dhcpdata);
        //memcpy(&(l7.data), &dhcpdata, sizeof(dhcpdata));
        byte protocolid = 0; //dhcp
        L6Data l6(protocolid, l7);
        byte sessionid = 0;
        L5Session l5(sessionid, l6);
        byte destinationport = 67;
        byte sourceport = 68;
        bool istcp = false;
        L4DatagramUDP l4(destinationport, sourceport, l5, istcp);    
        uint32_t destinationip    = 0xFFFFFFFF;
        uint32_t sourceip         = 0x00000000;
        uint32_t sourcemask       = 0x00000000;
        byte TTL = 254;
        L3PacketUDP l3(destinationip, sourceip, sourcemask, TTL, l4);
        //watchdog_update();
        
        byte destinationmac[6]  = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        L2FrameUDP l2(destinationmac, macaddress, l3, 0x0);
        //l2.crc = calculatecrc32(l2);
        sendpacketUDP(l2);
        lcdwritepacket(l2, false, HEX);
        got_offer_dhcp = true;
      } else if ((received.L3.L4.L5.L6.protocolid == 0) && (received.L3.L4.L5.L6.L7.data0 == 53) && (received.L3.L4.L5.L6.L7.data1 == 0x5)) {
        gatewayaddress = received.L3.sourceip;
        gatewaymac[0] = received.sourcemac0;
        gatewaymac[1] = received.sourcemac1;
        gatewaymac[2] = received.sourcemac2;
        gatewaymac[3] = received.sourcemac3;
        gatewaymac[4] = received.sourcemac4;
        gatewaymac[5] = received.sourcemac5;
        ipaddress = received.L3.destinationip;
        ipmask = received.L3.sourcemask;
        got_ip_dhcp = true;  
      }
    } else {
      if(!got_offer_dhcp) {
        byte dhcpdata[10] = {53, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0}; // DHCPDISCOVER
        L7Data l7(dhcpdata);
        //memcpy(&(l7.data), &dhcpdata, sizeof(dhcpdata));
        byte protocolid = 0; //dhcp
        L6Data l6(protocolid, l7);
        byte sessionid = 0;
        L5Session l5(sessionid, l6);
        byte destinationport = 67;
        byte sourceport = 68;
        bool istcp = false;
        L4DatagramUDP l4(destinationport, sourceport, l5, istcp);    
        uint32_t destinationip    = 0xFFFFFFFF;
        uint32_t sourceip         = 0x00000000;
        uint32_t sourcemask       = 0x00000000;
        byte TTL = 254;
        L3PacketUDP l3(destinationip, sourceip, sourcemask, TTL, l4);
        
       // watchdog_update();
        
        byte destinationmac[6]  = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        L2FrameUDP l2(destinationmac, macaddress, l3, 0x0);
        l2.crc = calculatecrc32(l2);
        sendpacketUDP(l2);
        lcdwritepacket(l2, false, HEX);
      }
    }
    //watchdog_update();
    delay(1000);
  }
}

bool button15pressed = false; // send butotn
bool button14pressed = false; // icmp button (has priority)
uint32_t press15started = 0;


void loopreceive() {
  uint8_t rxbuffer;
  queue_t rxqueue;
  //queue_init(&rxqueue, 1, (sizeof(L2FrameUDP) * 8) - 1);
  queue_init(&rxqueue, 1, sizeof(L2FrameUDP) * 8);
  bool avail = false;
  int lastavail = 0;
  int samecounter = 0;
  
  while (true) {
    avail = uart_is_readable(UART_ID);
    /*if(avail == lastavail) {
      samecounter++;
      if(samecounter >= 40) {
        samecounter = 0;
        while(is_writing == true) {}
        is_writing = true;
        lcd.setCursor(0, lcdline);
        lcd.print("Connection lost");
        lcdline = (lcdline == 0) ? 1 : 0;
        is_writing = false;
      } else {
        lastavail = avail;
        samecounter = 0;
      } 
    }*/
    if(avail) {
      uart_read_blocking(UART_ID, &rxbuffer, 1);
      queue_add_blocking(&rxqueue, (void*)&rxbuffer);
    }
    if(queue_get_level(&rxqueue) == sizeof(L2FrameUDP)) {
      uint8_t bytes[sizeof(L2FrameUDP)];
      for(int i = 0; i < sizeof(L2FrameUDP); i++) {
        queue_remove_blocking(&rxqueue, &(bytes[i]));
      }
      L2FrameUDP received = *(reinterpret_cast<L2FrameUDP*>(bytes));
      queue_add_blocking(&receivedpackets, (void*)&received);
    }
    /*
    if(avail) {
      int bytecount = 0; // queue is 351B == (sizeof(L2FrameUDP) * 8) - 1, only 7 correct packets can be stored, the last 43 bytes of data is to account for the bad bytes to be discared
      while((avail == true) && (bytecount < ((sizeof(L2FrameUDP) * 7) - 1))) {
        uart_read_blocking(UART_ID, &rxbuffer, 1);
        queue_try_add(&rxqueue, (void*)&rxbuffer);
        bytecount++;
        avail = uart_is_readable(UART_ID);
      }
      int queuelevel = queue_get_level(&rxqueue);
      int multiple = 0;
      while((multiple * sizeof(L2FrameUDP)) < queuelevel) {
        multiple = multiple + 1;
      }
      multiple = multiple - 1;
      for(int i = 0; i < (queuelevel - (multiple * sizeof(L2FrameUDP))); i++) {
        queue_remove_blocking(&rxqueue, NULL);
      }
      for(int i = 0; i < multiple; i++) {
        uint8_t bytes[44];  
        for(int i2 = 0; i < 44; i2++) {
          if(queue_try_remove(&rxqueue, (void*)&(bytes[i2]))) {
          }
          else {
            while(is_writing == true) {}
            is_writing = true;
            lcd.setCursor(0, lcdline);
            lcd.print("Remove error");
            lcdline = (lcdline == 0) ? 1 : 0;
            is_writing = false;
          }
          L2FrameUDP received = *(reinterpret_cast<L2FrameUDP*>(bytes));
          while(!queue_try_add(&receivedpackets, (void*)&received)) {}
        }
      } 
    }
    */
    delay(1000);  
  }
}

void loop() {/*
  if(queue_get_level(&receivedpackets) >= 1) {
    byte temparr1[6] = {0, 0, 0, 0, 0, 0};
    byte temparr2[6] = {0, 0, 0, 0, 0, 0};
    byte temparr3[10] = {0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
    L2FrameUDP received(temparr1, temparr2, {0x0, 0x0, 0x0, 0x0, {0x0, 0x0, {0x0, {0x0, {temparr3}}}, false}}, 0x0);
    queue_remove_blocking(&receivedpackets, (void*)&received);
    lcdwritepacket(received, true, DEC);
    if(received.L3.L4.L5.L6.protocolid == 4) {
      otherhostip = (received.L3.L4.L5.L6.L7.data0 << (8*3)) + (received.L3.L4.L5.L6.L7.data1 << (8*2)) + (received.L3.L4.L5.L6.L7.data2 << (8)) + received.L3.L4.L5.L6.L7.data3;
    } else if(received.L3.L4.L5.L6.protocolid == 1) {
      if(otherhostip == 0) {
        byte icmpdata[10] = {macaddress[0],  macaddress[1], macaddress[2], macaddress[3], macaddress[4], 0x2, 0x0, 0x0, 0x0, 0x0};
        L7Data l7(icmpdata);
        L6Data l6(4 /* arp *\/, l7);
        L5Session l5(0, l6);
        L4DatagramUDP l4(0, 0, l5, false);
        uint32_t destinationip    = gatewayaddress;
        uint32_t sourceip         = ipaddress;
        uint32_t sourcemask       = ipmask;
        byte TTL = 254;
        L3PacketUDP l3(gatewayaddress, sourceip, sourcemask, TTL, l4);
        
        //watchdog_update();
        
        L2FrameUDP l2(gatewaymac, macaddress, l3, 0x0);
        l2.crc = calculatecrc32(l2);
        sendpacketUDP(l2);
        lcdwritepacket(l2, false, HEX);
        //watchdog_update();
      } else {
        byte senddata[10] = {0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0}; // 0: 0 echo reply
        L7Data l7(senddata);
        L6Data l6(1 /* send *\/, l7);
        L5Session l5(0, l6);
        L4DatagramUDP l4(0, 0, l5, false);
        uint32_t destinationip    = gatewayaddress;
        uint32_t sourceip         = otherhostip;
        uint32_t sourcemask       = ipmask;
        byte TTL = 254;
        L3PacketUDP l3(destinationip, sourceip, sourcemask, TTL, l4);
        
        //watchdog_update();
        
        L2FrameUDP l2(gatewaymac, macaddress, l3, 0x0);
        l2.crc = calculatecrc32(l2);
        sendpacketUDP(l2);
        lcdwritepacket(l2, false, HEX);
        //watchdog_update();
      }
    } else if (received.L3.L4.L5.L6.protocolid == 2) { // send  
      //nothing to be done here actually
    }
  } else {
    if(button14pressed) {
      if(digitalRead(14) == LOW) {
        button14pressed = true;
      } else {
        //send arp, send icmp
        button14pressed = false;
        if(otherhostip == 0) {
          byte icmpdata[10] = {macaddress[0],  macaddress[1], macaddress[2], macaddress[3], macaddress[4], 0x2, 0x0, 0x0, 0x0, 0x0};
          L7Data l7(icmpdata);
          L6Data l6(4 /* arp *\/, l7);
          L5Session l5(0, l6);
          L4DatagramUDP l4(0, 0, l5, false);
          uint32_t destinationip    = gatewayaddress;
          uint32_t sourceip         = ipaddress;
          uint32_t sourcemask       = ipmask;
          byte TTL = 254;
          L3PacketUDP l3(gatewayaddress, sourceip, sourcemask, TTL, l4);
          
         // watchdog_update();
          
          L2FrameUDP l2(gatewaymac, macaddress, l3, 0x0);
          l2.crc = calculatecrc32(l2);
          sendpacketUDP(l2);
          lcdwritepacket(l2, false, HEX);
          //watchdog_update();
        } else {
          byte icmpdata[10] = {80, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0}; // 8: 0 echo request
          L7Data l7(icmpdata);
          L6Data l6(1 /* icmp *\/, l7);
          L5Session l5(0, l6);
          L4DatagramUDP l4(0, 0, l5, false);
          uint32_t destinationip    = gatewayaddress;
          uint32_t sourceip         = otherhostip;
          uint32_t sourcemask       = ipmask;
          byte TTL = 254;
          L3PacketUDP l3(destinationip, sourceip, sourcemask, TTL, l4);
          
          //watchdog_update();
          
          L2FrameUDP l2(gatewaymac, macaddress, l3, 0x0);
          l2.crc = calculatecrc32(l2);
          sendpacketUDP(l2);
          lcdwritepacket(l2, false, HEX);
          //watchdog_update();
        }
      }
    } else if (button15pressed){
      if(digitalRead(15) == LOW) {
        button15pressed = true;
      } else {
        button15pressed = false;
        int32_t duration = press15started - millis();
        press15started = 0;
        //send arp, send send
        if(otherhostip == 0) {
          byte icmpdata[10] = {macaddress[0],  macaddress[1], macaddress[2], macaddress[3], macaddress[4], 0x2, 0x0, 0x0, 0x0, 0x0};
          L7Data l7(icmpdata);
          L6Data l6(4 /* arp *\/, l7);
          L5Session l5(0, l6);
          L4DatagramUDP l4(0, 0, l5, false);
          uint32_t destinationip    = gatewayaddress;
          uint32_t sourceip         = ipaddress;
          uint32_t sourcemask       = ipmask;
          byte TTL = 254;
          L3PacketUDP l3(gatewayaddress, sourceip, sourcemask, TTL, l4);
          
          //watchdog_update();
          
          L2FrameUDP l2(gatewaymac, macaddress, l3, 0x0);
          l2.crc = calculatecrc32(l2);
          sendpacketUDP(l2);
          lcdwritepacket(l2, false, HEX);
          //watchdog_update();
        } else {
          byte senddata[10] = {0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
          L7Data l7(senddata);
          L6Data l6(2 /* send *\/, l7);
          L5Session l5(0, l6);
          L4DatagramUDP l4(0, 0, l5, false);
          uint32_t destinationip    = gatewayaddress;
          uint32_t sourceip         = otherhostip;
          uint32_t sourcemask       = ipmask;
          byte TTL = 254;
          L3PacketUDP l3(destinationip, sourceip, sourcemask, TTL, l4);
          
          //watchdog_update();
          
          L2FrameUDP l2(gatewaymac, macaddress, l3, 0x0);
          l2.crc = calculatecrc32(l2);
          sendpacketUDP(l2);
          lcdwritepacket(l2, false, HEX);
          //watchdog_update();
        }
      }
    } else {
      if(digitalRead(14) == LOW) {
        button14pressed = true;
      } else if(digitalRead(15) == LOW) {
        button15pressed = true;
        press15started = millis();
      }
    }
    delay(1000);
  }*/
}
