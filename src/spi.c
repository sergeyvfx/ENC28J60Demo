/* Copyright (C) 2015 Sergey Sharybin <sergey.vfx@gmail.com>
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

#include "spi.h"
#include "chip_configuration.h"
#include "io_mapping.h"

/* TODOs:
 * - Make transmittion/sampling configurable.
 * - Enable more than single CS bit.
 */

void SPI_Init(void) {
  SSP_CS_TRIS = 0;
  SSP_SDI_TRIS = 1;
  SSP_SCK_TRIS = 0;
  SSP_SDO_TRIS  = 0;

  PIR1bits.SSPIF = 0;
  SSPSTAT = 0b00000000;
  /* TODO(sergey): Make this configurable. */
  /* Input data sampled at middle of data output time. */
  SSPSTATbits.SMP = 0;
  /* Transmit occurs on transition from active to Idle clock state. */
  SSPSTATbits.CKE = 1;

  SSPCON1 = 0b00000000;
  SSPCON1bits.SSPM = 0b0000; /* SPI Master mode, clock = FOSC/4. */
  SSPCON1bits.CKP = 0;
  SSPCON1bits.SSPEN = 1; /* Enable serial port. */

  SSP_CS_IO = 1;
}

void SPI_Write(uint8_t addr, uint8_t data) {
  SSP_CS_IO = 0;
  SSPBUF = addr;
  while (!PIR1bits.SSPIF);
  PIR1bits.SSPIF = 0;
  SSPBUF = data;
  while (!PIR1bits.SSPIF);
  PIR1bits.SSPIF = 0;
  SSP_CS_IO = 1;
}

uint8_t SPI_Read(uint8_t addr) {
  SSP_CS_IO = 0;
  SSPBUF = 0x00;
  while (!PIR1bits.SSPIF);
  PIR1bits.SSPIF = 0;
  SSP_CS_IO = 1;
  return SSPBUF;
}
