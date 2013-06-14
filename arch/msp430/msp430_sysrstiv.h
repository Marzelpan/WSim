
/**
 *  \file   msp430_sysrstiv.h
 *  \brief  MSP430 Reset reason
 *  \author David Graeff
 *  \date   2013
 **/

#ifndef MSP430_SYSRSTIV_H
#define MSP430_SYSRSTIV_H

#if !defined(SYSRSTIV_BASE)
#define SYSRSTIV_BASE  0x190
#endif

#define SYSRSTIV_START SYSRSTIV_BASE
#define SYSRSTIV_END   (SYSRSTIV_BASE + 0x20)

void msp430_sysrstiv_create();
void msp430_sysrstiv_reset();
#define msp430_sysrstiv_update() do { } while (0)

int16_t msp430_sysrstiv_read(uint16_t addr);
void msp430_sysrstiv_write(uint16_t addr, int16_t val);

#endif
