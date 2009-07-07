/**
 *  \file   msp430_adc12.c
 *  \brief  MSP430 Adc12 controller
 *  \author Antoine Fraboulet
 *  \date   2006
 **/

#include "arch/common/hardware.h"
#include "msp430.h"
#include "src/options.h"

#if defined(__msp430_have_adc12)

/* ************************************************** */
/* ************************************************** */
/* ************************************************** */

#if defined(DEBUG)
#define HW_DMSG_ADC12(x...) HW_DMSG(x)
#else 
#define HW_DMSG_ADC12(x...) do { } while (0)
#endif

#define DEBUG_LEVEL_2 

#if defined(DEBUG_LEVEL_2)
#define HW_DMSG_2_DBG(x...) HW_DMSG(x)
#else
#define HW_DMSG_2_DBG(x...) do { } while (0)
#endif

/* ************************************************** */
/* ************************************************** */
/* ************************************************** */

tracer_id_t MSP430_TRACER_ADC12STATE;
tracer_id_t MSP430_TRACER_ADC12INPUT[ADC12_CHANNELS];
int         msp430_adc12_channels_valid[ADC12_CHANNELS];
char        msp430_adc12_channels_name[ADC12_CHANNELS][MAX_FILENAME];
uint16_t*   msp430_adc12_channels_data[ADC12_CHANNELS];
uint32_t    msp430_adc12_channels_data_max[ADC12_CHANNELS];

#define ADC12_CHANNEL_NAMES 20
char trace_names[ADC12_CHANNELS][ADC12_CHANNEL_NAMES] = {
  "adc12_input00", "adc12_input01", "adc12_input02", "adc12_input03",
  "adc12_input04", "adc12_input05", "adc12_input06", "adc12_input07",
  "adc12_input08", "adc12_input09", "adc12_input10", "adc12_input11",
  "adc12_input12", "adc12_input13", "adc12_input14", "adc12_input15"
};

#define ADC12_MODES        4
#define ADC12_MODES_NAMES 40
char adc12_modes[ADC12_MODES][ADC12_MODES_NAMES] = {
  "Single Channel Single Conversion",  "Sequence of Channels",
  "Repeat Single Channel",  "Repeat Sequence of Channels"
};

/* ************************************************** */
/* ************************************************** */
/* ************************************************** */

/* ADC12 internal OSC is ~ 5MHz */
/* cycle_nanotime == 200        */
#define NANO                    (  1000*1000*1000)
#define ADC12OSC_FREQ           (     5*1000*1000)

#define ADC12OSC_CYCLE_NANOTIME (NANO / ADC12OSC_FREQ) 

/* ************************************************** */
/* ************************************************** */
/* ************************************************** */

static struct moption_t adc12_in_opt = {
  .longname    = "msp430_adc12",
  .type        = required_argument,
  .helpstring  = "msp430 adc12 input",
  .value       = NULL
};

int msp430_adc12_option_add (void)
{
  options_add( &adc12_in_opt );
  return 0;
}

/* ************************************************** */
/* ************************************************** */
/* ************************************************** */

int msp430_adc12_find_inputs()
{
  char delim1[] = ",";
  char delim2[] = ":";
  char *str1,*str2;
  char *token,*subtoken;
  char *saveptr1,*saveptr2;
  char *filename;
  char name[MAX_FILENAME];
  int  j;
  int id;
  strncpy(name, adc12_in_opt.value, MAX_FILENAME);

  /* --msp430_adc12=1:file,2:file,3:file ... */

  for (j = 1, str1 = name; ; j++, str1 = NULL) 
    {
      token = strtok_r(str1, delim1, &saveptr1);
      if (token == NULL)
	break;
      HW_DMSG_ADC12("msp430:adc12:%d: %s\n", j, token);
    
      str2 = token;
      subtoken = strtok_r(str2, delim2, &saveptr2);
      if (subtoken == NULL) 
	{ 
	  ERROR("msp430:adc12: wrong channel id \n");
	  return 1;	
	}
      id = atoi(subtoken);
      if ((id < 0) || (id >= ADC12_CHANNELS))
	{
	  ERROR("msp430:adc12: wrong channel id %s (%d)\n",subtoken,id);
	  return 1;
	}

      subtoken = strtok_r(NULL, delim2, &saveptr2);
      filename = subtoken;
      if (subtoken == NULL) 
	{
	  ERROR("msp430:adc12: wrong channel filename\n");
	  return 1;	
	}

      subtoken = strtok_r(NULL, delim2, &saveptr2);
      if (subtoken != NULL) 
	{
	  ERROR("msp430:adc12: wrong channel filename trailer %s\n",subtoken);
	  return 1;	
	}

      HW_DMSG_ADC12("msp430:adc12: channel %02d = %s\n",id, filename);
      msp430_adc12_channels_valid[id]    = 1;
      strncpy(msp430_adc12_channels_name[id], filename, MAX_FILENAME);
    }
  return 0;
}

/* ************************************************** */
/* ************************************************** */
/* ************************************************** */

int msp430_adc12_read_inputs()
{
  int i;
  uint32_t j;
  for(i=0; i < ADC12_CHANNELS; i++)
    {
      if (msp430_adc12_channels_valid[i])
	{
	  FILE* f;
	  int   val;

	  f = fopen(msp430_adc12_channels_name[i],"rb");
	  if (f==NULL)
	    {
	      ERROR("msp430:adc12: cannot open file %s\n",msp430_adc12_channels_name[i]);
	      return 1;
	    }
	  while(fscanf(f,"%d",&val) > 0)
	    {
	      msp430_adc12_channels_data_max[i] ++;
	    }
	  msp430_adc12_channels_data[i] = malloc(msp430_adc12_channels_data_max[i]*sizeof(uint16_t));
	  if (msp430_adc12_channels_data[i] == NULL)
	    {
	      ERROR("msp430:adc12: cannot allocate memory for input %s\n",msp430_adc12_channels_name[i]);
	      fclose(f);
	      return 1;
	    }

	  fseek(f,0,SEEK_SET);
	  for(j=0; j<msp430_adc12_channels_data_max[i]; j++)
	    {
	      fscanf(f,"%d",&val);
	      msp430_adc12_channels_data[i][j] = (uint16_t)val;
	    }

	  fclose(f);
	  HW_DMSG_ADC12("msp430:adc12: channel %02d filled with %d samples\n",
			i,msp430_adc12_channels_data_max[i]);
	}
    }
  return 0;
}

int msp430_adc12_delete_inputs()
{
  int i;
  for(i=0; i < ADC12_CHANNELS; i++)
    {
      if (msp430_adc12_channels_valid[i])
	{
	  free(msp430_adc12_channels_data[i]);
	  msp430_adc12_channels_data[i] = NULL;
	}
    }
  return 0;
}

/* ************************************************** */
/* ************************************************** */
/* ************************************************** */

int msp430_adc12_init(void)
{
  int i;

  for(i=0; i<ADC12_CHANNELS; i++)
    {
      MSP430_TRACER_ADC12INPUT[i]       = 0;
      msp430_adc12_channels_valid[i]    = 0;
      msp430_adc12_channels_data[i]     = NULL;
      msp430_adc12_channels_data_max[i] = 0;
      strcpy(msp430_adc12_channels_name[i], "none");

      MCU.adc12.chann_ptr[i] = 0;
      MCU.adc12.chann_time[i] = 0;
      MCU.adc12.chann_period[i] = 0;
    }

  if (adc12_in_opt.isset)
    {
      msp430_adc12_find_inputs();
      msp430_adc12_read_inputs();
    }

  MSP430_TRACER_ADC12STATE    = tracer_event_add_id(1,  "adc12_enable",   "msp430");
  for(i=0; i<ADC12_CHANNELS; i++)
    {
      if (msp430_adc12_channels_valid[i])
	{
	  MSP430_TRACER_ADC12INPUT[i] = tracer_event_add_id(16,  trace_names[i],   "msp430");
	}

      MCU.adc12.mem[i].s = 0;
    }

  
  return 0;
}

/* ************************************************** */
/* ************************************************** */
/* ************************************************** */

void msp430_adc12_reset(void)
{
  int i;
  /* set initial values */
  HW_DMSG_ADC12("msp430:adc12:reset()\n");
  MCU.adc12.ctl0.s = 0;
  MCU.adc12.ctl1.s = 0;
  MCU.adc12.ifg    = 0;
  MCU.adc12.ie     = 0;
  MCU.adc12.iv     = 0;
  for(i=0; i<ADC12_CHANNELS; i++)
    {
      MCU.adc12.mctl[i].s = 0;
    }

  MCU.adc12.adc12osc_freq           = ADC12OSC_FREQ;
  MCU.adc12.adc12osc_cycle_nanotime = ADC12OSC_CYCLE_NANOTIME;
}

/* ************************************************** */
/* ************************************************** */
/* ************************************************** */

#define CHECK_ENC()					\
  do {							\
    if (MCU.adc12.ctl0.b.enc == 0)			\
      {							\
	MCU.adc12.state = ADC12_STATE_WAIT_ENABLE;	\
	return;						\
      }							\
  } while(0)


static const int shtdiv[16] = 
{
  4, 8, 16, 32, 64, 96, 128, 192, 256, 384, 512, 768, 1024, 1024, 1024, 1024 
};


void msp430_adc12_update(void)
{
  if (MCU.adc12.ctl0.b.adc12on == 0)
    {
      MCU.adc12.state = ADC12_STATE_OFF;
      return;
    }

  MCU.adc12.adc12osc_temp     += MACHINE_TIME_GET_INCR();
  MCU.adc12.adc12osc_increment = MCU.adc12.adc12osc_temp / MCU.adc12.adc12osc_cycle_nanotime;
  MCU.adc12.adc12osc_temp      = MCU.adc12.adc12osc_temp % MCU.adc12.adc12osc_cycle_nanotime;
  MCU.adc12.adc12osc_counter  += MCU.adc12.adc12osc_increment;

  MCU.adc12.adc12clk_temp     += MCU.adc12.adc12osc_increment;
  MCU.adc12.adc12clk_increment = MCU.adc12.adc12clk_temp / (MCU.adc12.ctl1.b.adc12divx + 1);
  MCU.adc12.adc12clk_temp      = MCU.adc12.adc12clk_temp % (MCU.adc12.ctl1.b.adc12divx + 1);

  /* FIXME:: take MSC into account */
  if (MCU.adc12.ctl1.b.shp == 1)
    {
      if ((0 <= MCU.adc12.current_channel) && (MCU.adc12.current_channel <= 7))
	{
	  /* registers MEM0 to MEM7  */
	  MCU.adc12.sht0_temp         += MCU.adc12.adc12clk_increment;
	  MCU.adc12.sht0_increment     = MCU.adc12.sht0_temp / (shtdiv[MCU.adc12.ctl0.b.sht0x]);
	  MCU.adc12.sht0_temp          = MCU.adc12.sht0_temp % (shtdiv[MCU.adc12.ctl0.b.sht0x]);
	  MCU.adc12.sampcon            = MCU.adc12.sht0_increment;
	}
      else if ((8 <= MCU.adc12.current_channel) && (MCU.adc12.current_channel <= 15))
	{
	  /* registers MEM8 to MEM15 */
	  MCU.adc12.sht1_temp         += MCU.adc12.adc12clk_increment;
	  MCU.adc12.sht1_increment     = MCU.adc12.sht1_temp / (shtdiv[MCU.adc12.ctl0.b.sht1x]);
	  MCU.adc12.sht1_temp          = MCU.adc12.sht1_temp % (shtdiv[MCU.adc12.ctl0.b.sht1x]);
	  MCU.adc12.sampcon            = MCU.adc12.sht1_increment;
	}
      else
	{
	  ERROR("msp430:adc12: channel out of bounds\n");
	  machine_exit(1);
	}
    }
  else
    {
      // FIXME::sampcon = SHI
      ERROR("msp430:adc12: ADC does not currently support SHP = 0 mode\n");
      MCU.adc12.sampcon = 0;
    }


  switch (MCU.adc12.state)
    {
      /***************/
      /* OFF         */
      /***************/
    case ADC12_STATE_OFF: 
      MCU.adc12.state = ADC12_STATE_WAIT_ENABLE;
      /* no break */

      /***************/
      /* ENABLE      */
      /***************/
    case ADC12_STATE_WAIT_ENABLE:
      if (MCU.adc12.ctl0.b.enc)
	{
	  MCU.adc12.state = ADC12_STATE_WAIT_TRIGGER; 
	}
      if ((MCU.adc12.ctl0.b.enc) && 
	  (MCU.adc12.ctl0.b.adc12sc == 1) &&
	  (MCU.adc12.ctl1.b.shsx == 0))
	{
	  MCU.adc12.state = ADC12_STATE_SAMPLE; 
	}
      break;
      
      /***************/
      /* TRIGGER     */
      /***************/
    case ADC12_STATE_WAIT_TRIGGER:
      CHECK_ENC();
      break;

      /***************/
      /* SAMPLE      */
      /***************/
    case ADC12_STATE_SAMPLE:
      CHECK_ENC();
      break;

      /***************/
      /* CONVERT     */
      /***************/
    case ADC12_STATE_CONVERT:
      CHECK_ENC();
      break;

      /***************/
      /* STORE       */
      /***************/
    case ADC12_STATE_STORE:
      CHECK_ENC();
      break;
    }

}

/* ************************************************** */
/* ************************************************** */
/* ************************************************** */

int16_t msp430_adc12_read16(uint16_t addr)
{
  int16_t ret = 0;

  /*
  static int tinyos_read_this_too_many_times = 0;
  if (tinyos_read_this_too_many_times == 0)
    {
      HW_DMSG_ADC12("msp430:adc12: read16 at [0x%04x] \n",addr);
      tinyos_read_this_too_many_times = 1;
    }
  */

  switch (addr)
    {
    case ADC12CTL0    : /* 16 */
      HW_DMSG_ADC12("msp430:adc12:read16: ADC12CTL0 = 0x%04x\n",MCU.adc12.ctl0.s);
      ret = MCU.adc12.ctl0.s;
      break;

    case ADC12CTL1    : /* 16 */
      HW_DMSG_ADC12("msp430:adc12:read16: ADC12CTL1 = 0x%04x\n",MCU.adc12.ctl1.s);
      ret = MCU.adc12.ctl1.s;
      break;

    case ADC12IFG     : /* 16 */
      HW_DMSG_ADC12("msp430:adc12:read16: ADC12IFG = 0x%04x\n",MCU.adc12.ifg);
      ret = MCU.adc12.ifg;
      break;

    case ADC12IE      : /* 16 */
      HW_DMSG_ADC12("msp430:adc12:read16: ADC12IFG = 0x%04x\n",MCU.adc12.ie);
      ret = MCU.adc12.ie;
      break;

    case  ADC12IV     : /* 16 */
      HW_DMSG_ADC12("msp430:adc12:read16: ADC12IV = 0x%04x\n",MCU.adc12.iv);
      ret = MCU.adc12.iv;
      break;

    case  ADC12MEM0   : /* 16 */
    case  ADC12MEM1   :
    case  ADC12MEM2   :
    case  ADC12MEM3   :
    case  ADC12MEM4   :
    case  ADC12MEM5   :
    case  ADC12MEM6   :
    case  ADC12MEM7   :
    case  ADC12MEM8   :
    case  ADC12MEM9   :
    case  ADC12MEM10  :
    case  ADC12MEM11  :
    case  ADC12MEM12  :
    case  ADC12MEM13  :
    case  ADC12MEM14  :
    case  ADC12MEM15  :
      HW_DMSG_ADC12("msp430:adc12:read16: ADC12MEM%d = 0x%04x\n",addr - ADC12MEM0, 
		    MCU.adc12.mem[(addr - ADC12MEM0) / 2].s);
      ret = MCU.adc12.mem[(addr - ADC12MEM0) / 2].s;
      break;

    default:
      HW_DMSG_ADC12("msp430:adc12:read16: at [0x%04x], unknown address\n",addr);
      break;
    }

  return ret;
}

/* ************************************************** */
/* ************************************************** */
/* ************************************************** */

int8_t msp430_adc12_read8(uint16_t addr)
{
  int8_t ret = 0;

  /*
  static int tinyos_read_this_too_many_times = 0;
  if (tinyos_read_this_too_many_times == 0)
    {
      HW_DMSG_ADC12("msp430:adc12: read8 at [0x%04x] \n",addr);
      tinyos_read_this_too_many_times = 1;
    }
  */

  switch (addr) 
    {
    case  ADC12MCTL0  : /*  8 */
    case  ADC12MCTL1  :
    case  ADC12MCTL2  :
    case  ADC12MCTL3  :
    case  ADC12MCTL4  :
    case  ADC12MCTL5  :
    case  ADC12MCTL6  :
    case  ADC12MCTL7  :
    case  ADC12MCTL8  :
    case  ADC12MCTL9  :
    case  ADC12MCTL10 :
    case  ADC12MCTL11 :
    case  ADC12MCTL12 :
    case  ADC12MCTL13 :
    case  ADC12MCTL14 :
    case  ADC12MCTL15 :
      HW_DMSG_ADC12("msp430:adc12:read08: ADC12MEMCTL%d = 0x%02x\n", addr - ADC12MCTL0,
		    MCU.adc12.mctl[addr - ADC12MCTL0].s);
      ret = MCU.adc12.mctl[addr - ADC12MCTL0].s & 0xff;
      break;
    default:
      HW_DMSG_ADC12("msp430:adc12:read08: at [0x%04x], unknown address\n",addr);
      break;
    }
  return ret;
}

/* ************************************************** */
/* ************************************************** */
/* ************************************************** */

void msp430_adc12_ctl0details(char* msg, int16_t *val)
{
  struct adc12ctl0_t *ctl0 = (struct adc12ctl0_t*) val;
  HW_DMSG_ADC12("msp430:adc12:%s sht1x      %d\n",msg,ctl0->sht1x);
  HW_DMSG_ADC12("msp430:adc12:%s sht0x      %d\n",msg,ctl0->sht0x);
  HW_DMSG_ADC12("msp430:adc12:%s msc        %d\n",msg,ctl0->msc);
  HW_DMSG_ADC12("msp430:adc12:%s ref2_5v    %d\n",msg,ctl0->ref2_5v);
  HW_DMSG_ADC12("msp430:adc12:%s refon      %d\n",msg,ctl0->refon);
  HW_DMSG_ADC12("msp430:adc12:%s adc12on    %d\n",msg,ctl0->adc12on);
  HW_DMSG_ADC12("msp430:adc12:%s adc12ovie  %d\n",msg,ctl0->adc12ovie);
  HW_DMSG_ADC12("msp430:adc12:%s adc12tovie %d\n",msg,ctl0->adc12tovie);
  HW_DMSG_ADC12("msp430:adc12:%s enc        %d\n",msg,ctl0->enc);
  HW_DMSG_ADC12("msp430:adc12:%s adc12sc    %d\n",msg,ctl0->adc12sc);
}

void msp430_adc12_ctl1details(char* msg, int16_t *val)
{
  struct adc12ctl1_t *ctl1 = (struct adc12ctl1_t*) val;
  HW_DMSG_ADC12("msp430:adc12:%s cstartaddx %d\n",msg,ctl1->cstartaddx);
  HW_DMSG_ADC12("msp430:adc12:%s shsx       %d\n",msg,ctl1->shsx);
  HW_DMSG_ADC12("msp430:adc12:%s shp        %d\n",msg,ctl1->shp);
  HW_DMSG_ADC12("msp430:adc12:%s issh       %d\n",msg,ctl1->issh);
  HW_DMSG_ADC12("msp430:adc12:%s adc12divx  %d\n",msg,ctl1->adc12divx);
  HW_DMSG_ADC12("msp430:adc12:%s adc12sselx %d\n",msg,ctl1->adc12sselx);
  HW_DMSG_ADC12("msp430:adc12:%s conseqx    %d\n",msg,ctl1->conseqx);
  HW_DMSG_ADC12("msp430:adc12:%s adc12busy  %d\n",msg,ctl1->adc12busy);
}

/* ************************************************** */
/* ************************************************** */
/* ************************************************** */

void msp430_adc12_start_enc(void)
{
  char buff[70];
#define FORMAT "msp430:adc12:    * %-50s *\n"

#define ADC12PPP(x...) do {			\
    snprintf(buff,70,x);			\
    HW_DMSG_ADC12(FORMAT,buff);			\
  } while (0)

  ADC12PPP("sht0 = 0x%x /%d",MCU.adc12.ctl0.b.sht0x & 0xf,shtdiv[MCU.adc12.ctl0.b.sht0x]);
  ADC12PPP("sht1 = 0x%x /%d",MCU.adc12.ctl0.b.sht1x & 0xf,shtdiv[MCU.adc12.ctl0.b.sht1x]);

  switch (MCU.adc12.ctl1.b.adc12sselx)
    {
    case 0: /* ADC12OSC */
      ADC12PPP("CLK source = ADC12OSC");
      break;
    case 1: /* ACLK */
      ADC12PPP("CLK source = ACLK");
      break;
    case 2: /* MCLK */
      ADC12PPP("CLK source = MCLK");
      break;
    case 3: /* SMCLK */
      ADC12PPP("CLK source = SMCLK");
      break;
    }
  ADC12PPP("CLK div %d :: /%d",MCU.adc12.ctl1.b.adc12divx,MCU.adc12.ctl1.b.adc12divx+1);
  ADC12PPP("ISSH input signal [%s]",(MCU.adc12.ctl1.b.issh == 0) ? "normal":"inverted");
  ADC12PPP("SHP %d :: sourced from %s signal",MCU.adc12.ctl1.b.shp,(MCU.adc12.ctl1.b.shp == 0) ? "sample-input":"sampling");
  switch (MCU.adc12.ctl1.b.shsx)
    {
    case 0: /* ADC12SX bit */
      ADC12PPP("SHS source = ADC12SC bit");
      break;
    case 1: /* TimerA.out1 */
      ADC12PPP("SHS source = TimerA.OUT1");
      break;
    case 2: /* TimerB.out0 */
      ADC12PPP("SHS source = TimerB.OUT0");
      break;
    case 3: /* TimerB.out1 */
      ADC12PPP("SHS source = TimerB.OUT1");
      break;
    }
  ADC12PPP("ADC12SC             : %d",MCU.adc12.ctl0.b.adc12sc);
  ADC12PPP("start sequence      : %d",MCU.adc12.ctl1.b.cstartaddx);
  ADC12PPP("conversion sequence : %s",adc12_modes[MCU.adc12.ctl1.b.conseqx]);
}

/* ************************************************** */
/* ************************************************** */
/* ************************************************** */

void msp430_adc12_write16(uint16_t addr, int16_t val)
{
  switch (addr)
    {
    case ADC12CTL0    : /* 16 */
      HW_DMSG_ADC12("msp430:adc12:write16: ADC12CTL0 = 0x%04x\n",val);
      if (val != MCU.adc12.ctl0.s)
	{
	  msp430_adc12_ctl0details("    ctl0 set ",&val);
	}
      if (MCU.adc12.ctl0.b.enc == 0)
	{
	  struct adc12ctl0_t *bval = (struct adc12ctl0_t *) &val;
	  if (bval->enc == 1)
	    {
	      HW_DMSG_ADC12("msp430:adc12:    ** START ENC **\n");
	      /* configuratioin is fixed */
	      msp430_adc12_start_enc();
	      MCU.adc12.current_channel = MCU.adc12.ctl1.b.cstartaddx;
	    }
	  MCU.adc12.ctl0.s = val;
	}
      else
	{
	  MCU.adc12.ctl0.s = (MCU.adc12.ctl0.s & 0xfff0) | (val & 0xf);
	}
      break;

    case ADC12CTL1    : /* 16 */
      HW_DMSG_ADC12("msp430:adc12:write16: ADC12CTL1 = 0x%04x\n",val);
      if (val != MCU.adc12.ctl1.s)
	{
	  msp430_adc12_ctl1details("    ctl1 set ",&val);
	}
      if (MCU.adc12.ctl0.b.enc == 0)
	{
	  MCU.adc12.ctl1.s = val;
	}
      else
	{
	  MCU.adc12.ctl1.s = (MCU.adc12.ctl1.s & 0xfff0) | (val & 0xf);
	}
      break;

    case ADC12IFG     : /* 16 */
      if (MCU.adc12.ifg != val)
	{
	  HW_DMSG_ADC12("msp430:adc12:write16: ADC12IFG changed from 0x%04x to 0x%04x\n",MCU.adc12.ifg,val);
	}
      MCU.adc12.ifg = val;
      break;

    case ADC12IE      : /* 16 */
      if (MCU.adc12.ie != val)
	{
	  HW_DMSG_ADC12("msp430:adc12:write16: ADC12IE changed from 0x%04x to 0x%04x\n",MCU.adc12.ie,val);
	}
      MCU.adc12.ie = val;
      break;

    case  ADC12IV     : /* 16 */
      HW_DMSG_ADC12("msp430:adc12:write16: write to ADC12IV, read only register\n");
      break;

    case  ADC12MEM0   : /* 16 */
    case  ADC12MEM1   :
    case  ADC12MEM2   :
    case  ADC12MEM3   :
    case  ADC12MEM4   :
    case  ADC12MEM5   :
    case  ADC12MEM6   :
    case  ADC12MEM7   :
    case  ADC12MEM8   :
    case  ADC12MEM9   :
    case  ADC12MEM10  :
    case  ADC12MEM11  :
    case  ADC12MEM12  :
    case  ADC12MEM13  :
    case  ADC12MEM14  :
    case  ADC12MEM15  :
      HW_DMSG_ADC12("msp430:adc12:write16: ADC12MEM%d = 0x%04x\n", (addr - ADC12MEM0)/2, val & 0xffff);
      MCU.adc12.mem[(addr - ADC12MEM0) / 2].b.value = val & 0x0FFF;
      break;

    default:
      HW_DMSG_ADC12("msp430:adc12:write16: at [0x%04x] = 0x%04x, unknown address\n",addr,val & 0xffff);
      break;
    }
}

/* ************************************************** */
/* ************************************************** */
/* ************************************************** */

void msp430_adc12_mctl_details(int n, struct adc12mctlx_t *mctl)
{
  HW_DMSG_ADC12("msp430:adc12:    mctl%d  eos :%d\n",n,mctl->eos);
  HW_DMSG_ADC12("msp430:adc12:    mctl%d  sref:%x\n",n,mctl->sref);
  HW_DMSG_ADC12("msp430:adc12:    mctl%d  inch:%x\n",n,mctl->inch);
}

void msp430_adc12_write8(uint16_t addr, int8_t val)
{
  switch (addr)
    {
    case  ADC12MCTL0  : /*  8 */
    case  ADC12MCTL1  :
    case  ADC12MCTL2  :
    case  ADC12MCTL3  :
    case  ADC12MCTL4  :
    case  ADC12MCTL5  :
    case  ADC12MCTL6  :
    case  ADC12MCTL7  :
    case  ADC12MCTL8  :
    case  ADC12MCTL9  :
    case  ADC12MCTL10 :
    case  ADC12MCTL11 :
    case  ADC12MCTL12 :
    case  ADC12MCTL13 :
    case  ADC12MCTL14 :
    case  ADC12MCTL15 :
      HW_DMSG_ADC12("msp430:adc12:write08: ADC12MCTL%d = 0x%02x\n", addr - ADC12MCTL0, val & 0xff);
      if (MCU.adc12.ctl0.b.enc == 0)
	{
	  MCU.adc12.mctl[addr - ADC12MCTL0].s = val & 0xff;
	  msp430_adc12_mctl_details(addr - ADC12MCTL0, & (MCU.adc12.mctl[addr - ADC12MCTL0].b));
	}
      break;
    default:
      HW_DMSG_ADC12("msp430:adc12:write08: unknown address for [0x%04x] = 0x%02x\n",addr,val & 0xff);
      break;
    }
}

/* ************************************************** */
/* ************************************************** */
/* ************************************************** */

#endif
