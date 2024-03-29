/**
 ******************************************************************************
 *
 * @file       OSD_Panels.ino
 * @author     Joerg-D. Rothfuchs
 * @brief      The OSD panels
 *
 *****************************************************************************/


#include "OSD_Config.h"
#include "PWMRead.h"
#include "TSRXTalk.h"


#define CRITICAL_PERCENT			75	// [%]	set this to a level that you think is critical for your environment

#define	SHOW_MINIMAL					// 	show minimalized information

#define PAN_WARN_X				4	//	x position of the warning
#define PAN_WARN_Y				3	//	y position of the warning

#define PAN_CHAN_STAT_X				4	//	x position of the channel status
#define PAN_CHAN_STAT_Y				4	//	y position of the channel status

#define PAN_RX_STAT_X				22	//	x position of the receiver status
#define PAN_RX_STAT_Y				9	//	y position of the receiver status


#define SWITCHING_TIME				1000	// [ms]	the time within the switching has to happen for screen switching

#define VIEW_CNT				2	//	number of implemented views

#define WARN_MAX				2	//	number of implemented warnings

#define WARN_ON_TIME				1000	// [ms]	time warnings showing
#define WARN_OFF_TIME				250	// [ms]	time warnings not showing
#define BLINK_TIME				250	// [ms]	blink time


static int data_view = 0;


/******* MAIN FUNCTIONS *******/


/******************************************************************/
// Panel  : writePanels
// Output : Write the panels
/******************************************************************/
void writePanels() {
	switchView();
	showView();
}


/******************************************************************/
// Panel  : switchView
// Needs  : nothing
// Output : check switch view RC channel
/******************************************************************/
void switchView(void) {
	static unsigned long switch_time = 0;
	static int switch_state = 0;
	int stick_position;
	
	stick_position = pwm_get();
	
	if (switch_state == 0 && PWM_CHECK(stick_position)) {
		switch_state = 1;
		switch_time = millis();
	}
	
	if (switch_state == 1 && !PWM_CHECK(stick_position)) {
		switch_state = 0;
		if (millis() - switch_time < SWITCHING_TIME) {
			data_view = (data_view >= VIEW_CNT) ? 0 : data_view + 1;
			osd.clear();
		}
	}
}


/******************************************************************/
// Panel  : showView
// Needs  : nothing
// Output : show the views
/******************************************************************/
void showView(void) {
	if (BadPacketsDelta || BadChannelDelta) {
		if (data_view > 1) data_view = 1;
	}
	
	switch (data_view) {
		case 0:
			panChannelStatus(PAN_CHAN_STAT_X, PAN_CHAN_STAT_Y);
			panRxStatus(PAN_RX_STAT_X, PAN_RX_STAT_Y);
		break;
		case 1:
			panRxStatus(PAN_RX_STAT_X, PAN_RX_STAT_Y);
		break;
		case 2:
		break;
	}
	
	panWarn(PAN_WARN_X, PAN_WARN_Y);
}


/******* PANELS *******/


/******************************************************************/
// Panel  : panBoot
// Needs  : X, Y locations
// Output : Booting up text and empty bar after that
/******************************************************************/
void panBoot(int first_col, int first_line) {
#if 0
	osd.setPanel(first_col, first_line);
	osd.openPanel();
	osd.printf_P(PSTR("booting up:\xed\xf2\xf2\xf2\xf2\xf2\xf2\xf2\xf3")); 
	osd.closePanel();
#endif
}


/******************************************************************/
// Panel  : panLogo
// Needs  : 
// Output : Startup OSD LOGO
/******************************************************************/
void panLogo() {
	osd.setPanel(5, 0);
	osd.openPanel();
	osd.printf_P(PSTR("minrxosd 0.3.0  beta"));
	osd.closePanel();
}


/******************************************************************/
// Panel  : panWarn
// Needs  : X, Y locations
// Output : Warnings if there are any
//
// Layout :
//		|BAD PACKETSxxxxx  abc%|  with debug channel feature
//		|!CRITICAL! xxxxx  abc%|  with debug channel feature
//		|FAILSAFE xxxx     abc%|  with debug channel feature
//
//		|  BAD PACKETS xxxxxx  |  without debug channel feature
//		|     FAILSAFE xxx     |  without debug channel feature
//
/******************************************************************/
void panWarn(int first_col, int first_line) {
    static char warning_string[22];
    static uint8_t last_warning_type = 1;
    static uint8_t warning_type = 1;
    static unsigned long warn_text_timer = 0;
    static unsigned long blink_timer = 0;
    static uint8_t blink_cnt = 0;
    int cycle;

    if (millis() > warn_text_timer) {				// if the text or blank text has been shown for a while
        if (warning_type) {					// there was a warning, so we now blank it out for a while
            last_warning_type = warning_type;			// save the warning type for cycling
            warning_type = 0;					// set clear warning time
	    warn_text_timer = millis() + WARN_OFF_TIME;
        } else {
            cycle = last_warning_type;				// start the warning checks cycle where we left it last time
            do {				                // cycle through the warning checks
                if (++cycle > WARN_MAX) cycle = 1;
                switch (cycle) {
			case 1:					// BAD PACKETS
				if (BadChannelDelta || BadPacketsDelta) {
					warning_type = cycle;
				}
			break;
			case 2:					// FAILSAFE
				if (FailsafesDelta) {
					warning_type = cycle;
				}
			break;
                }
            } while (!warning_type && cycle != last_warning_type);
	    if (warning_type) {					// if there is a warning
		warn_text_timer = millis() + WARN_ON_TIME;	// set show warning time
	    }
        }
    }

    switch (warning_type) {
	case 0:		// blank the warning
		if (DEBUG_CHAN_ACTIVE) {
			sprintf(warning_string, "                 ");
		} else {
			sprintf(warning_string, "                      ");
		}
	break;
	case 1:		// BAD PACKETS or !CRITICAL!
		if (DEBUG_CHAN_ACTIVE) {
			if (packet_window_percent() >= CRITICAL_PERCENT) {
				sprintf(warning_string, "bad packets%5u ", BadChannelDelta);
			} else {
				sprintf(warning_string, "%ccritical%c %5u ", 0x9F, 0x9F, BadChannelDelta);
			}
		} else {
			if (BadChannelDelta) {
				sprintf(warning_string, "  bad packets %6u  ", BadChannelDelta);
			}
			if (BadPacketsDelta) {
				sprintf(warning_string, "  bad packets %6u  ", BadPacketsDelta);
			}
		}
	break;
	case 2:		// FAILSAFE
		if (DEBUG_CHAN_ACTIVE) {
			sprintf(warning_string, "failsafe %4u    ", Failsafes);
		} else {
			sprintf(warning_string, "     failsafe %3u     ", Failsafes);
		}
	break;
    }

    osd.setPanel(first_col, first_line);
    osd.openPanel();
    if (DEBUG_CHAN_ACTIVE) {
	uint8_t percent = packet_window_percent();
	if (BadChannelDelta == 0 && percent == 100) {
		osd.printf("%s     ", warning_string);
		blink_cnt = 0;
	} else {
		if (millis() > blink_timer + BLINK_TIME) {
			blink_cnt++;
			blink_timer = millis();
		}
		if (percent == 100)
			osd.printf("%s %3u%c", warning_string, percent, 0xE3);
		else
			osd.printf("%s %c%2u%c", warning_string, 0xE0 + (blink_cnt % 2), percent, 0xE2 + (blink_cnt % 2));
	}
    } else {
	osd.printf("%s", warning_string);
    }
    osd.closePanel();
}


/******************************************************************/
// Panel  : panChannelStatus
// Needs  : X, Y locations
// Output : Shows the channel status
/******************************************************************/
void panChannelStatus(int first_col, int first_line) {
	static uint8_t show = 0;
	int x, y;
	int div = CHANNEL_DIV;
	int digit;
	int sub;
	char c;
	
	if (BadChannelDelta) {
		show = 1;
	} else {
		show = 0;
	}
	
	osd.setPanel(first_col, first_line);
	osd.openPanel();
    
	for (y = 0; y < CHANNEL_STATUS_ROWS; y++) {
		for (x = 0; x < CHANNEL_MAX; x++) {
			if (!show) {
				c = ' ';
			} else if (ChannelFails[x] > CHANNEL_ERROR_SHOW_MAX) {
				c = '*';
			} else {
				digit = (int)(ChannelFails[x] / div);
				if (digit > 9) {
					sub = (int)(digit / 10) * 10;
				} else {
					sub = 0;
				}
				digit -= sub;
				if (digit == 0 && ChannelFails[x] < div) {
					c = ' ';
				} else {
					c = digit + '0';
				}
			}
			osd.printf("%c", c);
		}
		osd.printf("|");
		div /= 10;
	}
    
	osd.closePanel();
}


/******************************************************************/
// Panel  : panRxStatus
// Needs  : X, Y locations
// Output : Shows the RX status
/******************************************************************/
void panRxStatus(int first_col, int first_line) {
	osd.setPanel(first_col, first_line);
	osd.openPanel();
#ifdef SHOW_MINIMAL
	if (get_tsrx_version() == TSRX_IDLE_OLDER) {
		if (BadChannel) osd.printf("%6u%c", BadChannel, 0x9D);
	} else {
		if (DEBUG_CHAN_ACTIVE) {
			char rotary = 0x90 + ChannelCount % 12;
			if (Failsafes > 9 || BadChannel > 999) {
				osd.printf("%5u%c%c", BadChannel, 0x9D, rotary);
				if (Failsafes) osd.printf("|%5u%c", Failsafes, 0x9C);
			} else if (Failsafes) {
				osd.printf("%1u%c%3u%c%c", Failsafes, 0x9C, BadChannel, 0x9D, rotary);
			} else if (BadChannel) {
				osd.printf("%5u%c%c", BadChannel, 0x9D, rotary);
			} else {
				osd.printf("      %c", rotary);
			}
		} else {
			if (scan_value_percent() < 100) osd.printf(" %c %3u%c|", 0xE1, scan_value_percent(), '%');
			else osd.printf("|");
			if (BadPackets) osd.printf("%6u%c|", BadPackets, 0x9D);
			else osd.printf("|");
			if (Failsafes) osd.printf("%6u%c", Failsafes, 0x9C);
		}
	}
#else
	if (get_tsrx_version() == TSRX_IDLE_OLDER) {
		osd.printf("%6u%c", BadChannel, 0x9D);
	} else {
		if (DEBUG_CHAN_ACTIVE) {
			//osd.printf(" %c %3u%c|", 0xE1, packet_window_percent(), '%');
			osd.printf("%6lu%c|", ChannelCount, 0x90 + ChannelCount % 12);
			osd.printf("%6u%c|", BadChannel, 0x9D);
		} else {
			osd.printf(" %c %3u%c|", 0xE1, scan_value_percent(), '%');
			//osd.printf("%6lu%c|", GoodPackets, 0x9E);
			osd.printf("%6u%c|", BadPackets, 0x9D);
		}
		osd.printf("%6u%c", Failsafes, 0x9C);
	}
#endif
	osd.closePanel();
}
