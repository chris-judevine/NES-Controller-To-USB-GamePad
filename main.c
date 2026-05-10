#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <string.h>

#include "usbdrv.h"
#include "oddebug.h"
#include "osccal.c"

/* Pin assignment:
 * 
 * PB0 (5) = D- (2 ws)
 * PB2 (7) = D+ (3 gn)
 * 
 * PB1 (6) = CLOCK (4)
 * PB3 (2) = LATCH (3)
 * PB4 (3) = DATA  (2)
*/


#define NES_LATCH   (1<<PB3)
#define NES_CLOCK   (1<<PB1)
#define NES_DATA    (1<<PB4)


#define NES_LATCH_LOW()	PORTB &= ~(NES_LATCH);
#define NES_LATCH_HIGH()	PORTB |= NES_LATCH;
#define NES_CLOCK_LOW()	PORTB &= ~(NES_CLOCK);
#define NES_CLOCK_HIGH()	PORTB |= NES_CLOCK;
#define NES_GET_DATA()		(PINB & NES_DATA)

static unsigned char last_read_bytes[1];
static unsigned char last_reported_bytes[1];
static uchar reportBuffer[3];    /* buffer for HID reports */
static uchar idleRate;           /* in 4 ms units */

static void UpdateNES(void);
static char ChangedNES(void);

PROGMEM char usbHidReportDescriptor[USB_CFG_HID_REPORT_DESCRIPTOR_LENGTH] = { /* USB report descriptor */    	0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
    0x09, 0x05,                    // USAGE (Game Pad)
    0xa1, 0x01,                    //   COLLECTION (Application)
    0x09, 0x01,                    //     USAGE (Pointer)
    0xa1, 0x00,                    //     COLLECTION (Physical)
    0x09, 0x30,                    //       USAGE (X)
    0x09, 0x31,                    //       USAGE (Y)
    0x15, 0x00,                    //       LOGICAL_MINIMUM (0)
    0x26, 0xff, 0x00,				//      LOGICAL_MAXIMUM (255)
    0x75, 0x08,                    //       REPORT_SIZE (8)
    0x95, 0x02,                    //       REPORT_COUNT (2)
    0x81, 0x02,                    //       INPUT (Data,Var,Abs)
    0xc0,                          //     END_COLLECTION
    0x05, 0x09,                    //     USAGE_PAGE (Button)
    0x19, 0x01,                    //     USAGE_MINIMUM (Button 1)
    0x29, 0x04,                    //     USAGE_MAXIMUM (Button 4)
    0x15, 0x00,                    //     LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //     LOGICAL_MAXIMUM (1)
    0x75, 0x01,                    //     REPORT_SIZE (1)
    0x95, 0x04,                    //     REPORT_COUNT (4)
    0x81, 0x02,                    //     INPUT (Data,Var,Abs)

    /* Padding.*/
    0x75, 0x01,                    //     REPORT_SIZE (1) 
    0x95, 0x04,                    //     REPORT_COUNT (4)
    0x81, 0x03,                    //     INPUT (Constant,Var,Abs)
    0xc0,                          //   END_COLLECTION
};

static void UpdateNES(void) {
	uchar tmp=0;
	int i;

	NES_LATCH_HIGH();
	_delay_us(12);
	NES_LATCH_LOW();

	for (i=0; i<8; i++)
	{
		_delay_us(6);
		NES_CLOCK_LOW();
		
		tmp <<= 1;	
		if (!NES_GET_DATA()) { tmp |= 1; }

		_delay_us(6);
		
		NES_CLOCK_HIGH();
	}
	last_read_bytes[0] = tmp;
}

static char ChangedNES(void)
{
	return memcmp(last_read_bytes, last_reported_bytes, 1);
}

static void buildReport(void) {
	int x, y;
	unsigned char lrcb;

	if (reportBuffer != NULL)
	{
		y = x = 0x80;
		lrcb = last_read_bytes[0];

		if (lrcb&0x1) { x = 0xff; }
		if (lrcb&0x2) { x = 0; }
		if (lrcb&0x4) { y = 0xff; }
		if (lrcb&0x8) { y = 0; }
		reportBuffer[0]=x;
		reportBuffer[1]=y;

		// swap buttons so they get reported in a more
		// logical order (A-B-Select-Start)
		reportBuffer[2]=(lrcb & 0x80) >> 7;
		reportBuffer[2]|=(lrcb & 0x40) >> 5;
		reportBuffer[2]|=(lrcb & 0x20) >> 3;
		reportBuffer[2]|=(lrcb & 0x10) >> 1;
	}
	memcpy(last_reported_bytes, last_read_bytes, 1);	
}

uchar usbFunctionSetup(uchar data[8]) {
	usbRequest_t *rq = (void *)data;

    usbMsgPtr = reportBuffer;
    if((rq->bmRequestType & USBRQ_TYPE_MASK) == USBRQ_TYPE_CLASS){    /* class request type */
        if(rq->bRequest == USBRQ_HID_GET_REPORT){  /* wValue: ReportType (highbyte), ReportID (lowbyte) */
            /* we only have one report type, so don't look at wValue */
            buildReport();
            return sizeof(reportBuffer);
        }else if(rq->bRequest == USBRQ_HID_GET_IDLE){
            usbMsgPtr = &idleRate;
            return 1;
        }else if(rq->bRequest == USBRQ_HID_SET_IDLE){
            idleRate = rq->wValue.bytes[1];
        }
    }else{
        /* no vendor specific requests implemented */
    }
	return 0;
}

void    usbEventResetReady(void)
{
    calibrateOscillator();
    eeprom_write_byte(0, OSCCAL);   /* store the calibrated value in EEPROM */
}


int main(void) {
	uchar   calibrationValue;

    	calibrationValue = eeprom_read_byte(0); /* calibration value from 	last time */
    	if(calibrationValue != 0xff){
        	OSCCAL = calibrationValue;
    	}

	odDebugInit();
    DDRB = (1 << USB_CFG_DMINUS_BIT) | (1 << USB_CFG_DPLUS_BIT);
    PORTB = 0; //indicate USB disconnect to host

	uchar i = 20;
    while(i){ //300 ms disconnect, also allows our oscillator to stabilize
        _delay_ms(15);
		i--;
    }
	
	//Init NES-Controller + Reset 
	DDRB = NES_LATCH | NES_CLOCK;

	wdt_enable(WDTO_1S);
    usbInit();
    sei();
    for(;;) { //main event loop
        wdt_reset();
        usbPoll();

		UpdateNES();
		        
		if(usbInterruptIsReady() && ChangedNES()){
            buildReport();
            usbSetInterrupt(reportBuffer, sizeof(reportBuffer));
        }
    }
    return 0;
}
