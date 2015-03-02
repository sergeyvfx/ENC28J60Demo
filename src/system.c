/* Adopted from Microchip Libraries for Application with this license:
 *
 * Software License Agreement:
 *
 * The software supplied herewith by Microchip Technology Incorporated
 * (the "Company") for its PIC(R) Microcontroller is intended and
 * supplied to you, the Company's customer, for use solely and
 * exclusively on Microchip PIC Microcontroller products. The
 * software is owned by the Company and/or its supplier, and is
 * protected under applicable copyright laws. All rights are reserved.
 * Any use in violation of the foregoing restrictions may subject the
 * user to criminal sanctions under applicable laws, as well as to
 * civil liability for the breach of the terms and conditions of this
 * license.

 * THIS SOFTWARE IS PROVIDED IN AN "AS IS" CONDITION. NO WARRANTIES,
 * WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT NOT LIMITED
 * TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. THE COMPANY SHALL NOT,
 * IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL OR
 * CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 *
 * Contributor(s): Sergey Sharybin
 */

#include "system.h"

#include "system.h"
#include "chip_configuration.h"
#include "app_network.h"

typedef enum {
    SOFT_START_POWER_OFF,
    SOFT_START_POWER_START,
    SOFT_START_POWER_ENABLED,
    SOFT_START_POWER_READY
} SOFT_START_STATUS;

static SOFT_START_STATUS softStartStatus = SOFT_START_POWER_OFF;

void SYSTEM_Initialize() {
  /* Configure ports as inputs (1) or outputs(0) */
  TRISA = 0b00000000;
  TRISB = 0b00000000;
  TRISC = 0b00000000;
#if defined(__18F4550)
  TRISD = 0b00000000;
  TRISE = 0b00000000;
#endif

  /* Clear all ports */
  PORTA = 0b00000000;
  PORTB = 0b00000000;
  PORTC = 0b00000000;
#if defined(__18F4550)
  PORTD = 0b00000000;
  PORTE = 0b00000000;
#endif

  LATA = 0b00000000;
  LATB = 0b00000000;
  LATC = 0b00000000;
#if defined(__18F4550)
  LATD = 0b00000000;
  LATE = 0b00000000;
#endif

  APP_network_init();
}

/* Runs system level tasks that keep the system running.
 * PreCondition: System has been initalized with SYSTEM_Initialize()
 */
void SYSTEM_Tasks(void) {
  APP_network_loop();
}

#if defined(__XC8)
void interrupt SYS_InterruptHigh(void)
{
#  if defined(USB_INTERRUPT)
    USBDeviceTasks();
#  endif
}
#else
void YourHighPriorityISRCode();
void YourLowPriorityISRCode();

  /* On PIC18 devices, addresses 0x00, 0x08, and 0x18 are used for
   * the reset, high priority interrupt, and low priority interrupt
   * vectors.  However, the current Microchip USB bootloader
   * examples are intended to occupy addresses 0x00-0x7FF or
   * 0x00-0xFFF depending on which bootloader is used.  Therefore,
   * the bootloader code remaps these vectors to new locations
   * as indicated below.  This remapping is only necessary if you
   * wish to program the hex file generated from this project with
   * the USB bootloader.  If no bootloader is used, edit the
   * usb_config.h file and comment out the following defines:
   * #define PROGRAMMABLE_WITH_USB_HID_BOOTLOADER
   * #define PROGRAMMABLE_WITH_USB_LEGACY_CUSTOM_CLASS_BOOTLOADER
   */

#  if defined(PROGRAMMABLE_WITH_USB_HID_BOOTLOADER)
#    define REMAPPED_RESET_VECTOR_ADDRESS      0x1000
#    define REMAPPED_HIGH_INTERRUPT_VECTOR_ADDRESS  0x1008
#    define REMAPPED_LOW_INTERRUPT_VECTOR_ADDRESS  0x1018
#  elif defined(PROGRAMMABLE_WITH_USB_MCHPUSB_BOOTLOADER)
#    define REMAPPED_RESET_VECTOR_ADDRESS      0x800
#    define REMAPPED_HIGH_INTERRUPT_VECTOR_ADDRESS  0x808
#    define REMAPPED_LOW_INTERRUPT_VECTOR_ADDRESS  0x818
#  else
#    define REMAPPED_RESET_VECTOR_ADDRESS      0x00
#    define REMAPPED_HIGH_INTERRUPT_VECTOR_ADDRESS  0x08
#    define REMAPPED_LOW_INTERRUPT_VECTOR_ADDRESS  0x18
#  endif

#  if defined(PROGRAMMABLE_WITH_USB_HID_BOOTLOADER)||defined(PROGRAMMABLE_WITH_USB_MCHPUSB_BOOTLOADER)
extern void _startup(void);        // See c018i.c in your C18 compiler dir
#    pragma code REMAPPED_RESET_VECTOR = REMAPPED_RESET_VECTOR_ADDRESS
void _reset(void) {
  _asm goto _startup _endasm
}
#  endif

#  pragma code REMAPPED_HIGH_INTERRUPT_VECTOR = REMAPPED_HIGH_INTERRUPT_VECTOR_ADDRESS
void Remapped_High_ISR(void) {
  _asm goto YourHighPriorityISRCode _endasm
}
#  pragma code REMAPPED_LOW_INTERRUPT_VECTOR = REMAPPED_LOW_INTERRUPT_VECTOR_ADDRESS
void Remapped_Low_ISR (void) {
  _asm goto YourLowPriorityISRCode _endasm
}

#  if defined(PROGRAMMABLE_WITH_USB_HID_BOOTLOADER)||defined(PROGRAMMABLE_WITH_USB_MCHPUSB_BOOTLOADER)
  /* Note: If this project is built while one of the bootloaders has
   * been defined, but then the output hex file is not programmed with
   * the bootloader, addresses 0x08 and 0x18 would end up programmed with 0xFFFF.
   * As a result, if an actual interrupt was enabled and occured, the PC would jump
   * to 0x08 (or 0x18) and would begin executing "0xFFFF" (unprogrammed space).  This
   * executes as nop instructions, but the PC would eventually reach the REMAPPED_RESET_VECTOR_ADDRESS
   * (0x1000 or 0x800, depending upon bootloader), and would execute the "goto _startup".  This
   * would effective reset the application.
   *
   * To fix this situation, we should always deliberately place a
   * "goto REMAPPED_HIGH_INTERRUPT_VECTOR_ADDRESS" at address 0x08, and a
   * "goto REMAPPED_LOW_INTERRUPT_VECTOR_ADDRESS" at address 0x18.  When the output
   * hex file of this project is programmed with the bootloader, these sections do not
   * get bootloaded (as they overlap the bootloader space).  If the output hex file is not
   * programmed using the bootloader, then the below goto instructions do get programmed,
   * and the hex file still works like normal.  The below section is only required to fix this
   * scenario.
   */
#  pragma code HIGH_INTERRUPT_VECTOR = 0x08
void High_ISR(void) {
  _asm goto REMAPPED_HIGH_INTERRUPT_VECTOR_ADDRESS _endasm
}
#  pragma code LOW_INTERRUPT_VECTOR = 0x18
void Low_ISR(void) {
  _asm goto REMAPPED_LOW_INTERRUPT_VECTOR_ADDRESS _endasm
}
#  endif  /* end of "#if defined(PROGRAMMABLE_WITH_USB_HID_BOOTLOADER)||defined(PROGRAMMABLE_WITH_USB_LEGACY_CUSTOM_CLASS_BOOTLOADER)" */

#  pragma code

  /* These are your actual interrupt handling routines. */
#  pragma interrupt YourHighPriorityISRCode
void YourHighPriorityISRCode(void) {
  /* Check which interrupt flag caused the interrupt.
   * Service the interrupt
   * Clear the interrupt flag
   * Etc.
   */
#    if defined(USB_INTERRUPT)
  USBDeviceTasks();
#    endif

  /* This return will be a "retfie fast", since this is in a
   * #pragma interrupt section. */
}

#  pragma interruptlow YourLowPriorityISRCode
void YourLowPriorityISRCode(void) {
  /* Check which interrupt flag caused the interrupt.
   * Service the interrupt
   * Clear the interrupt flag
   * Etc.

  /* This return will be a "retfie", since this is in a
    * #pragma interruptlow section.
    */
}
#endif
