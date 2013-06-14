/**
 *  \file   msp430_sysrstiv.h
 *  \brief  MSP430 Reset reason
 *  \author David Graeff
 *  \date   2013
 **/

#include <stdio.h> 
#include <string.h>

#include "arch/common/hardware.h"
#include "msp430.h"

#define SYSRSTIV_NONE      (0x0000)    /* No Interrupt pending */
#define SYSRSTIV_BOR       (0x0002)    /* SYSRSTIV : BOR */
#define SYSRSTIV_RSTNMI    (0x0004)    /* SYSRSTIV : RST/NMI */
#define SYSRSTIV_DOBOR     (0x0006)    /* SYSRSTIV : Do BOR */
#define SYSRSTIV_LPM5WU    (0x0008)    /* SYSRSTIV : Port LPM5 Wake Up */
#define SYSRSTIV_SECYV     (0x000A)    /* SYSRSTIV : Security violation */
#define SYSRSTIV_SVSL      (0x000C)    /* SYSRSTIV : SVSL */
#define SYSRSTIV_SVSH      (0x000E)    /* SYSRSTIV : SVSH */
#define SYSRSTIV_SVML_OVP  (0x0010)    /* SYSRSTIV : SVML_OVP */
#define SYSRSTIV_SVMH_OVP  (0x0012)    /* SYSRSTIV : SVMH_OVP */
#define SYSRSTIV_DOPOR     (0x0014)    /* SYSRSTIV : Do POR */
#define SYSRSTIV_WDTTO     (0x0016)    /* SYSRSTIV : WDT Time out */
#define SYSRSTIV_WDTKEY    (0x0018)    /* SYSRSTIV : WDTKEY violation */
#define SYSRSTIV_KEYV      (0x001A)    /* SYSRSTIV : Flash Key violation */
#define SYSRSTIV_PLLUL     (0x001C)    /* SYSRSTIV : PLL unlock */
#define SYSRSTIV_PERF      (0x001E)    /* SYSRSTIV : peripheral/config area fetch */
#define SYSRSTIV_PMMKEY    (0x0020)    /* SYSRSTIV : PMMKEY violation */

#define SYSBERRIV_            0x0198    /* Bus Error vector generator */
#define SYSUNIV_              0x019A    /* User NMI vector generator */
#define SYSSNIV_              0x019C    /* System NMI vector generator */
#define SYSRSTIV_             0x019E    /* Reset vector generator */

void msp430_sysrstiv_create()
{
  msp430_io_register_range16(SYSRSTIV_START,SYSRSTIV_END,msp430_sysrstiv_read,msp430_sysrstiv_write);
}

void msp430_sysrstiv_reset() {}

int16_t msp430_sysrstiv_read(uint16_t addr)
{
  if (addr == SYSRSTIV_) // reset reason always none
	return SYSRSTIV_NONE;
  
  ERROR("msp430:sysrstiv: read not implemented for 0x%04x\n",addr);
  return 0;
}

void msp430_sysrstiv_write(uint16_t addr, int16_t val)
{
  ERROR("msp430:sysrstiv: not implemented 0x%04x, 0x%04x\n",addr,val);
}
