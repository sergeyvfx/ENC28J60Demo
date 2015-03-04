/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2005 Pascal Stang
 * All rights reserved.
 *
 * Original Author: Pascal Stang
 * Contributor(s): Guido Socher
 *                 nuelectronics.com -- Ethershield for Arduino
 *                 Sergey Sharybin -- PIC18F4550 adoptation
 *
 * Microchip ENC28J60 Ethernet Interface Driver
 *
 * This driver provides initialization and transmit/receive
 * functions for the Microchip ENC28J60 10Mb Ethernet Controller and PHY.
 * This chip is novel in that it is a full MAC+PHY interface all in a 28-pin
 * chip, using an SPI interface to the host processor.
 */

#include "enc28j60.h"

#include "chip_configuration.h"
#include "io_mapping.h"
#include "spi.h"

static uint8_t Enc28j60Bank = 0xffffff;
static uint16_t NextPacketPtr;

void ENC28J60_WriteOp(uint8_t op, uint8_t addr, uint8_t data) {
  SPI_Write(op | (addr & ADDR_MASK), data);
}

uint8_t ENC28J60_ReadOp(uint8_t op, uint8_t addr) {
  SSP_CS_IO = 0;  /* Activate the SS SPI Select pin. */
  SSPBUF = op | (addr & ADDR_MASK);  /* Start register address transmission */
  while (!PIR1bits.SSPIF);  /* Wait for Data Transmit/Receipt complete. */
  PIR1bits.SSPIF = 0;
  SSPBUF = 0x00;  /* Send Dummy transmission for reading the data. */
  while (!PIR1bits.SSPIF);  /* Wait for Data Transmit/Receipt complete. */
  PIR1bits.SSPIF = 0;
  /* Do dummy read if needed (for mac and mii, see datasheet page 29). */
  if (addr & 0x80) {
    SSPBUF = 0x00;  /* Send Dummy transmission for reading the data. */
    while (!PIR1bits.SSPIF);  /* Wait for Data Transmit/Receipt complete. */
    PIR1bits.SSPIF = 0;
  }
  SSP_CS_IO = 1;  /* CS pin is not active. */
  return SSPBUF;
}

void ENC28J60_SetBank(uint8_t addr) {
  /* Set the bank if needed. */
  if ((addr & BANK_MASK) != Enc28j60Bank) {
    ENC28J60_WriteOp(ENC28J60_BIT_FIELD_CLR, ECON1, (ECON1_BSEL1|ECON1_BSEL0));
    ENC28J60_WriteOp(ENC28J60_BIT_FIELD_SET, ECON1, (addr & BANK_MASK) >> 5);
    Enc28j60Bank = (addr & BANK_MASK);
  }
}

uint8_t ENC28J60_Read(uint8_t addr) {
  ENC28J60_SetBank(addr);
  return ENC28J60_ReadOp(ENC28J60_READ_CTRL_REG, addr);
}

void ENC28J60_Write(uint8_t addr, uint8_t data) {
  ENC28J60_SetBank(addr);
  ENC28J60_WriteOp(ENC28J60_WRITE_CTRL_REG, addr, data);
}

void ENC28J60_PhyWrite(uint8_t addr, uint16_t data) {
  /* Set the PHY register address. */
  ENC28J60_Write(MIREGADR, addr);
  /* Write the PHY data. */
  ENC28J60_Write(MIWRL, data);
  ENC28J60_Write(MIWRH, data >> 8);
  /* Wait until the PHY write completes. */
  while (ENC28J60_Read(MISTAT) & MISTAT_BUSY) {
    __delay_us(15);
  }
}

uint8_t ENC28J60_GetRev(void) {
  return ENC28J60_Read(EREVID);
}

void ENC28J60_Init(uint8_t *macaddr) {
  /* Perform system reset. */
  ENC28J60_WriteOp(ENC28J60_SOFT_RESET, 0, ENC28J60_SOFT_RESET);
  /* check CLKRDY bit to see if reset is complete */
  __delay_ms(10);
  while (!(ENC28J60_Read(ESTAT) & ESTAT_CLKRDY));

  /* ** Do bank 0 stuff ** */
  /* Initialize receive buffer. 16-bit transfers, must write low byte first. */
  NextPacketPtr = RXSTART_INIT;  /* set receive buffer start address. */
  /* Rx start. */
  ENC28J60_Write(ERXSTL, RXSTART_INIT & 0xff);
  ENC28J60_Write(ERXSTH, RXSTART_INIT >> 8);
  /* Set receive pointer address. */
  ENC28J60_Write(ERXRDPTL, RXSTART_INIT & 0xff);
  ENC28J60_Write(ERXRDPTH, RXSTART_INIT >> 8);
  /* RX end. */
  ENC28J60_Write(ERXNDL, RXSTOP_INIT & 0xff);
  ENC28J60_Write(ERXNDH, RXSTOP_INIT >> 8);
  /* TX start. */
  ENC28J60_Write(ETXSTL, TXSTART_INIT & 0xff);
  ENC28J60_Write(ETXSTH, TXSTART_INIT >> 8);
  /* TX end. */
  ENC28J60_Write(ETXNDL, TXSTOP_INIT & 0xff);
  ENC28J60_Write(ETXNDH, TXSTOP_INIT >> 8);

  /* Do bank 1 stuff, packet filter:
   * For broadcast packets we allow only ARP packtets
   * All other packets should be unicast only for our mac (MAADR)
   *
   * The pattern to match on is therefore
   * Type     ETH.DST
   * ARP      BROADCAST
   * 06 08 -- ff ff ff ff ff ff -> ip checksum for theses bytes=f7f9
   * in binary these poitions are:11 0000 0011 1111
   * This is hex 303F->EPMM0=0x3f,EPMM1=0x30
   */
  ENC28J60_Write(ERXFCON, ERXFCON_UCEN|ERXFCON_CRCEN|ERXFCON_PMEN);
  ENC28J60_Write(EPMM0, 0x3f);
  ENC28J60_Write(EPMM1, 0x30);
  ENC28J60_Write(EPMCSL, 0xf9);
  ENC28J60_Write(EPMCSH, 0xf7);

  /* Do bank 2 stuff. */
  /* Enable MAC receive. */
  ENC28J60_Write(MACON1, MACON1_MARXEN|MACON1_TXPAUS|MACON1_RXPAUS);
  /* Bring MAC out of reset. */
  ENC28J60_Write(MACON2, 0x00);
  /* Enable automatic padding to 60bytes and CRC operations. */
  ENC28J60_WriteOp(ENC28J60_BIT_FIELD_SET,
                   MACON3,
                   MACON3_PADCFG0|MACON3_TXCRCEN|MACON3_FRMLNEN);
  /* Set inter-frame gap (non-back-to-back). */
  ENC28J60_Write(MAIPGL, 0x12);
  ENC28J60_Write(MAIPGH, 0x0C);
  /* Set inter-frame gap (back-to-back). */
  ENC28J60_Write(MABBIPG, 0x12);
  /* Set the maximum packet size which the controller will accept.
   * Do not send packets longer than MAX_FRAMELEN.
   */
  ENC28J60_Write(MAMXFLL, MAX_FRAMELEN & 0xff);
  ENC28J60_Write(MAMXFLH, MAX_FRAMELEN >> 8);

  /* ** Do bank 3 stuff ** */
  /* Write MAC address.
   * NOTE: MAC address in ENC28J60 is byte-backward.
   */
  ENC28J60_Write(MAADR5, macaddr[0]);
  ENC28J60_Write(MAADR4, macaddr[1]);
  ENC28J60_Write(MAADR3, macaddr[2]);
  ENC28J60_Write(MAADR2, macaddr[3]);
  ENC28J60_Write(MAADR1, macaddr[4]);
  ENC28J60_Write(MAADR0, macaddr[5]);

  /* No loopback of transmitted frames. */
  ENC28J60_PhyWrite(PHCON2, PHCON2_HDLDIS);

  /* switch to bank 0. */
  ENC28J60_SetBank(ECON1);
  /* Enable interrutps. */
  ENC28J60_WriteOp(ENC28J60_BIT_FIELD_SET, EIE, EIE_INTIE|EIE_PKTIE);
  /* Enable packet reception. */
  ENC28J60_WriteOp(ENC28J60_BIT_FIELD_SET, ECON1, ECON1_RXEN);
}

void ENC28J60_ClkOut(uint8_t clk) {
  /* Setup clkout: 2 is 12.5MHz: */
  ENC28J60_Write(ECOCON, clk & 0x7);
}

void ENC28J60_ReadBuffer(uint16_t len, uint8_t *data) {
  SSP_CS_IO = 0;
  /* Issue read command */
  SSPBUF = ENC28J60_READ_BUF_MEM;
  while (!PIR1bits.SSPIF);
  PIR1bits.SSPIF = 0;
  while (len) {
    len--;
    /* Read data. */
    SSPBUF = 0;
    while (!PIR1bits.SSPIF);
    PIR1bits.SSPIF = 0;
    *data = SSPBUF;
  data++;
  }
  *data='\0';
  SSP_CS_IO = 1;
}

/* Gets a packet from the network receive buffer, if one is available.
 * The packet will by headed by an ethernet header.
 *      maxlen  The maximum acceptable length of a retrieved packet.
 *      packet  Pointer where packet data should be stored.
 * Returns: Packet length in bytes if a packet was retrieved, zero otherwise.
 */
uint16_t ENC28J60_PacketReceive(uint16_t maxlen, uint8_t *packet) {
  uint16_t rxstat;
  uint16_t len;
  /* Check if a packet has been received and buffered. */
  // if(!(enc28j60Read(EIR) & EIR_PKTIF) ) {
  /* The above does not work. See Rev. B4 Silicon Errata point 6. */
  if (ENC28J60_Read(EPKTCNT) ==0) {
    return 0;
  }
  /* Set the read pointer to the start of the received packet. */
  ENC28J60_Write(ERDPTL, (NextPacketPtr));
  ENC28J60_Write(ERDPTH, (NextPacketPtr) >> 8);
  /* Read the next packet pointer. */
  NextPacketPtr  = ENC28J60_ReadOp(ENC28J60_READ_BUF_MEM, 0);
  NextPacketPtr |= ENC28J60_ReadOp(ENC28J60_READ_BUF_MEM, 0) << 8;
  /* Read the packet length (see datasheet page 43). */
  len  = ENC28J60_ReadOp(ENC28J60_READ_BUF_MEM, 0);
  len |= ENC28J60_ReadOp(ENC28J60_READ_BUF_MEM, 0) << 8;
  len -= 4; /* Remove the CRC count. */
  /* Read the receive status (see datasheet page 43). */
  rxstat  = ENC28J60_ReadOp(ENC28J60_READ_BUF_MEM, 0);
  rxstat |= ENC28J60_ReadOp(ENC28J60_READ_BUF_MEM, 0) << 8;
  /* Llimit retrieve length */
  if (len > maxlen - 1) {
      len = maxlen - 1;
  }
  /* Check CRC and symbol errors (see datasheet page 44, table 7-3):
   * The ERXFCON.CRCEN is set by default. Normally we should not
   * need to check this.
   */
  if ((rxstat & 0x80) == 0) {
      /* Invalid. */
      len = 0;
  } else {
      /* Copy the packet from the receive buffer. */
      ENC28J60_ReadBuffer(len, packet);
  }
  /* Move the RX read pointer to the start of the next received packet.
   * This frees the memory we just read out.
   */
  ENC28J60_Write(ERXRDPTL, (NextPacketPtr));
  ENC28J60_Write(ERXRDPTH, (NextPacketPtr)>>8);
  /* Decrement the packet counter indicate we are done with this packet. */
  ENC28J60_WriteOp(ENC28J60_BIT_FIELD_SET, ECON2, ECON2_PKTDEC);
  return len;
}

void ENC28J60_WriteBuffer(uint16_t len, uint8_t *data) {
  SSP_CS_IO = 0;
  /* Issue write command. */
  SSPBUF = ENC28J60_WRITE_BUF_MEM;
  while (!PIR1bits.SSPIF);
  PIR1bits.SSPIF = 0;

  while (len) {
    len--;
    /* Write data. */
    SSPBUF = *data;
    data++;
    while (!PIR1bits.SSPIF);
    PIR1bits.SSPIF = 0;
  }
  SSP_CS_IO = 1;
}

void ENC28J60_PacketSend(uint16_t len, uint8_t *packet) {
  /* Set the write pointer to start of transmit buffer area. */
  ENC28J60_Write(EWRPTL, TXSTART_INIT & 0xff);
  ENC28J60_Write(EWRPTH, TXSTART_INIT >> 8);
  /* Set the TXND pointer to correspond to the packet size given. */
  ENC28J60_Write(ETXNDL, (TXSTART_INIT + len) & 0xff);
  ENC28J60_Write(ETXNDH, (TXSTART_INIT + len) >> 8);
  /* Write per-packet control byte (0x00 means use macon3 settings). */
  ENC28J60_WriteOp(ENC28J60_WRITE_BUF_MEM, 0, 0x00);
  /* Copy the packet into the transmit buffer. */
  ENC28J60_WriteBuffer(len, packet);
  /* Send the contents of the transmit buffer onto the network. */
  ENC28J60_WriteOp(ENC28J60_BIT_FIELD_SET, ECON1, ECON1_TXRTS);
  /* Reset the transmit logic problem. See Rev. B4 Silicon Errata point 12. */
  if ((ENC28J60_Read(EIR) & EIR_TXERIF)) {
    ENC28J60_WriteOp(ENC28J60_BIT_FIELD_CLR, ECON1, ECON1_TXRTS);
  }
}
