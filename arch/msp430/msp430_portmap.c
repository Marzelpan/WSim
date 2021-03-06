/**
 *  \file   msp430_portmap.c
 *  \brief  MSP430 Portmap dummy(!) definition
 *  \author Bernhard Dick
 *  \date   2011
 **/

#include <stdio.h>

#include "arch/common/hardware.h"
#include "msp430.h"

#if defined(__msp430_have_portmap)
/* ************************************************** */
/* ************************************************** */

/* ************************************************** */

void msp430_portmap_create()
{
  msp430_io_register_range8(PORTMAP_IOMEM_BEGIN, PORTMAP_IOMEM_END, msp430_portmap_read8, msp430_portmap_write8);
  msp430_io_register_range16(PORTMAP_IOMEM_BEGIN, PORTMAP_IOMEM_END, msp430_portmap_read, msp430_portmap_write);
}

/* ************************************************** */
/* ************************************************** */

/* ************************************************** */

int msp430_portmap_reset()
{
  return 0;
}

/* ************************************************** */
/* ************************************************** */

/* ************************************************** */

void msp430_portmap_update()
{
}

/* ************************************************** */
/* ************************************************** */

/* ************************************************** */

int8_t msp430_portmap_read8(uint16_t addr)
{
  ERROR("msp430:portmap: bad read address 0x%04x\n", addr);
  return 0;
}

/* ************************************************** */
/* ************************************************** */

/* ************************************************** */

int16_t msp430_portmap_read(uint16_t addr)
{
  ERROR("msp430:portmap: bad read address 0x%04x\n", addr);
  return 0;
}

/* ************************************************** */
/* ************************************************** */

/* ************************************************** */

void msp430_portmap_write8(uint16_t addr, int8_t val)
{
  static uint8_t already_message = 0;
  if (already_message)
    return;
  already_message = 1;
  ERROR("msp430:portmap: write not implemented (0x%04x = 0x%02x)\n", addr, val & 0xff);
}

/* ************************************************** */
/* ************************************************** */

/* ************************************************** */

void msp430_portmap_write(uint16_t addr, int16_t val)
{
  static uint8_t already_message = 0;
  if (already_message)
    return;
  already_message = 1;
  ERROR("msp430:portmap: read not implemented (0x%04x = 0x%02x)\n", addr, val & 0xff);
}

/* ************************************************** */
/* ************************************************** */
/* ************************************************** */

#endif // _have_portmap


