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

#include <string.h>

static uint8_t my_macaddr[6] = {0x54, 0x55, 0x58, 0x10, 0x00, 0x24};
static uint8_t my_ip[4] = {192, 168, 0, 4};
static char baseurl[] = "http://192.168.0.4/";

#define BUFFER_SIZE 250
static unsigned char buf[BUFFER_SIZE + 1];
#define STR_BUFFER_SIZE 22
static char strbuf[STR_BUFFER_SIZE + 1];

#if defined(__18F4550)
#  define LED0_IO LATDbits.LD0
#  define LED1_IO LATDbits.LD1
#  define LED2_IO LATDbits.LD2
#elif defined(__18F2550)
#  define LED0_IO LATCbits.LC1
#  define LED1_IO LATCbits.LC2
#  define LED2_IO LATCbits.LC6
#endif

/* The returned value is stored in the global var strbuf. */
static uint8_t find_key_val(char *str, char *key) {
  uint8_t found = 0;
  uint8_t i = 0;
  char *kp;
  kp = key;
  while (*str &&  *str != ' ' && found == 0) {
    if (*str == *kp) {
      kp++;
      if (*kp == '\0') {
        str++;
        kp = key;
        if (*str == '=') {
          found = 1;
        }
      }
    } else {
      kp = key;
    }
    str++;
  }
  if (found == 1) {
    /* copy the value to a buffer and terminate it with '\0' */
    while (*str &&  *str != ' ' && *str != '&' && i < STR_BUFFER_SIZE) {
      strbuf[i] = *str;
      i++;
      str++;
    }
    strbuf[i] = '\0';
  }
  return found;
}

static int8_t analyse_cmd(char *str) {
  int8_t r = -1;
  if (find_key_val(str, (char*)"cmd")) {
    if (*strbuf < 0x3a && *strbuf > 0x2f) {
      /* Is a ASCII number, return it. */
      r = (*strbuf-0x30);
    }
  }
  return r;
}

static uint16_t print_webpage(uint8_t *buf, uint8_t on_off) {
  int i = 0;
  uint16_t plen;

  plen = NET_fill_tcp_data_p(buf, 0, "HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n");
  plen = NET_fill_tcp_data_p(buf, plen, "<center><p><h1>Welcome to ETH28J60 Demo for PIC18F4550</h1></p> ");
  plen = NET_fill_tcp_data_p(buf, plen, "<hr><br><form METHOD=get action=\"");
  plen = NET_fill_tcp_data(buf, plen, baseurl);
  plen = NET_fill_tcp_data_p(buf, plen, "\">");
  plen = NET_fill_tcp_data_p(buf, plen, "<h2> REMOTE LED is  </h2> ");
  plen = NET_fill_tcp_data_p(buf, plen, "<h1><font color=\"#00FF00\"> ");
  if (on_off) {
    plen = NET_fill_tcp_data_p(buf, plen, "ON");
  } else {
    plen = NET_fill_tcp_data_p(buf, plen, "OFF");
  }
  plen = NET_fill_tcp_data_p(buf, plen, "  </font></h1><br> ");
  if (on_off) {
    plen = NET_fill_tcp_data_p(buf, plen, "<input type=hidden name=cmd value=3>");
    plen = NET_fill_tcp_data_p(buf, plen, "<input type=submit value=\"Switch off\"></form>");
  } else {
    plen = NET_fill_tcp_data_p(buf, plen, "<input type=hidden name=cmd value=2>");
    plen = NET_fill_tcp_data_p(buf, plen, "<input type=submit value=\"Switch on\"></form>");
  }
  return plen;
}


void APP_network_init(void) {
  uint8_t a;
  LED0_IO = 0;
  LED1_IO = 1;
  LED2_IO = 0;

  LATBbits.LB4 = 0;  /* Reset the module. */
  LATBbits.LB5 = 0;
  __delay_ms(10);

  SPI_Init();
  LATBbits.LB4 = 1;  /* Resume the module. */
  LATBbits.LB5 = 1;

  ENC28J60_Init(my_macaddr);
  ENC28J60_ClkOut(2);
  __delay_ms(10);

  LED0_IO = 1;

  /* Debug blink: keep both LEDs on for a bit. */
  ENC28J60_PhyWrite(PHLCON, 0x880);
  for (a = 0; a < 100; ++a)  __delay_ms(10);

  /* LEDA=links status, LEDB=receive/transmit. */
  ENC28J60_PhyWrite(PHLCON, 0x476);
  LED2_IO = 1;
  NET_init(my_macaddr, my_ip, 80);
}

void APP_network_loop(void) {
  uint16_t plen, dat_p;
  int8_t cmd;
  uint8_t on_off = 1;

  plen = ENC28J60_PacketReceive(BUFFER_SIZE, buf);
  /* plen will be unequal to zero if there is a valid packet
   * (without crc error)
   */
  if (plen != 0) {
    /* arp is broadcast if unknown but a host may also verify the mac address by
     * sending it to a unicast address.
     */
    if (NET_eth_type_is_arp_and_my_ip(buf, plen)) {
      NET_make_arp_answer_from_request(buf);
      return;
    }

    /* Check if ip packets are for us. */
    if (NET_eth_type_is_ip_and_my_ip(buf, plen) == 0) {
      return;
    }

    if (buf[IP_PROTO_P] == IP_PROTO_ICMP_V &&
        buf[ICMP_TYPE_P] == ICMP_TYPE_ECHOREQUEST_V)
    {
      NET_make_echo_reply_from_request(buf, plen);
      return;
    }

    /* tcp port www start, compare only the lower byte. */
    if (buf[IP_PROTO_P] == IP_PROTO_TCP_V &&
        buf[TCP_DST_PORT_H_P] == 0 &&
        buf[TCP_DST_PORT_L_P] == 80)
    {
      if (buf[TCP_FLAGS_P] & TCP_FLAGS_SYN_V) {
        /* NET_make_tcp_synack_from_syn does already send the syn, ack. */
        NET_make_tcp_synack_from_syn(buf);
        return;
      }
      if (buf[TCP_FLAGS_P] & TCP_FLAGS_ACK_V) {
        NET_init_len_info(buf);  /* Init some data structures. */
        dat_p = NET_get_tcp_data_pointer();
        if (dat_p == 0) {  /* we can possibly have no data, just ack. */
          if (buf[TCP_FLAGS_P] & TCP_FLAGS_FIN_V) {
            NET_make_tcp_ack_from_any(buf);
          }
          return;
        }
        if (strncmp("GET ", (char *)&(buf[dat_p]), 4) != 0) {
          /* head, post and other methods for possible status codes see:
           *   http://www.w3.org/Protocols/rfc2616/rfc2616-sec10.html
           */
          plen = NET_fill_tcp_data_p(
              buf,
              0,
              "HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n<h1>200 OK</h1>");
          goto SENDTCP;
        }
        if (strncmp("/ ", (char *)&(buf[dat_p + 4]), 2) == 0) {
          plen = print_webpage(buf, on_off);
          goto SENDTCP;
        }
        cmd = analyse_cmd((char *)&(buf[dat_p + 5]));
        if (cmd == 2) {
          on_off = 1;
          LED2_IO = 1;
        } else if (cmd == 3) {
          on_off = 0;
          LED2_IO = 0;
        }
        plen = print_webpage(buf, on_off);
SENDTCP:
        NET_make_tcp_ack_from_any(buf);  /* Send ack for http get. */
        NET_make_tcp_ack_with_data(buf, plen);  /* send data. */
      }
    }
  }
}
