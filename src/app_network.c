/* Copyright (C) 2014 Sergey Sharybin <sergey.vfx@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "app_network.h"
#include "chip_configuration.h"
#include "enc28j60.h"
#include "net.h"
#include "spi.h"

static uint8_t my_macaddr[6] = {0x54, 0x55, 0x58, 0x10, 0x00, 0x24};
static uint8_t my_ip[4] = {192, 168, 0, 4};

#define BUFFER_SIZE 250
static unsigned char buf[BUFFER_SIZE+1];

void APP_network_init(void) {
  uint8_t a;
  LATDbits.LD0 = 0; LATDbits.LD1 = 1; LATDbits.LD2 = 0;

  LATBbits.LB4 = 0; LATBbits.LB5 = 0;  /* Reset the module. */
  __delay_ms(10);

  SPI_Init();
  LATBbits.LB4 = 1; LATBbits.LB5 = 1;  /* Resume the module. */

  ENC28J60_Init(my_macaddr);
  ENC28J60_ClkOut(2);
  __delay_ms(10);

  LATDbits.LD0 = 1;

  /* Debug blink: keep both LEDs on for a bit. */
  ENC28J60_PhyWrite(PHLCON, 0x880);
  for (a = 0; a < 100; ++a)  __delay_ms(10);

  /* LEDA=links status, LEDB=receive/transmit. */
  ENC28J60_PhyWrite(PHLCON, 0x476);
  LATDbits.LD2 = 1;
  NET_init(my_macaddr, my_ip, 80);
}

void APP_network_loop(void) {
  uint16_t plen;
  plen = ENC28J60_PacketReceive(BUFFER_SIZE, buf);
  /* plen will be unequal to zero if there is a valid packet
   * (without crc error)
   */
  if (plen != 0) {
    if(NET_eth_type_is_arp_and_my_ip(buf,plen)){
      NET_make_arp_answer_from_request(buf);
    }
    /* Check if ip packets (icmp or udp) are for us. */
    if (NET_eth_type_is_ip_and_my_ip(buf, plen) != 0) {
      if(buf[IP_PROTO_P] == IP_PROTO_ICMP_V &&
         buf[ICMP_TYPE_P] == ICMP_TYPE_ECHOREQUEST_V)
      {
        /* A ping packet, let's send pong. */
        NET_make_echo_reply_from_request(buf, plen);
      }
    }
  }
}
