#include <inttypes.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#define __DELAY_BACKWARD_COMPATIBLE__
#include <util/delay.h>
#include <stdlib.h>

#include "main.h"

#include "lib/usart.h"
#include "lib/bughal.h"
#include "lib/util.h"
#include "lib/music.h"

//operartion modes
#define MODE0 0
#define MODE1 1
#define MODE2 2
#define MODE3 3
#define MODE4 4
#define NUM_MODES 5

uint8_t OpMode = MODE2; //Operation mode
bool ModeChanged = true;

// check if mode should be changed (one of the buttons long pressed)
void modeswitch_poll(void) {
	if (btn_state(BTNST_LUP, BTN_LEFT)) {
		//opmode -
		OpMode = (0 == OpMode) ? (NUM_MODES - 1) : (OpMode - 1);
		ModeChanged = true;
		button_clear(BTN_LEFT);
	};
	if (btn_state(BTNST_LUP, BTN_RIGHT)) {
		//opmode +
		ModeChanged = true;
		OpMode = ((NUM_MODES - 1) == OpMode) ? 0 : (OpMode + 1);
		button_clear(BTN_RIGHT);
	};
	return;
}
;

// MODE0 sound detection
void do_mode0(void) {
	static timer_t mytimer;
	static uint16_t maxval; //maximum of ADC values read during the las timer interval
	static uint16_t lastmaxval; //maximum of values during last timer interval
	uint16_t curval; //current value on D5 (one pin of the piezo,the other is on GND)
	static bool signaling; //are we currently signaling (beeping, blinking etc...)
	static bool sound_on;  //should sound be on when signaling
	static bool motor_on;  //should motor be on when signaling

	if (ModeChanged) { //init after mode change
		maxval = 0;
		lastmaxval = 000;
		ModeChanged = false;
		signaling = false;
		sound_on = true;
		motor_on = true;
		init_mic();
		timer_set(&mytimer, 10);
	};

	// single ADC measurement
	curval = ADCW; // read result
	maxval = (curval > maxval) ? curval : maxval;

	//check for Buttons
	if (btn_state(BTNST_SUP, BTN_LEFT)) {
		button_clear(BTN_LEFT);
		sound_on = !sound_on;
	};
	if (btn_state(BTNST_SUP, BTN_RIGHT)) {
		button_clear(BTN_RIGHT);
		motor_on = !motor_on;
	};
	if (timer_expired(&mytimer)) {
		if (signaling) {
			//turn off sound
			music_setNote(NOTE_PAUSE, 0); //mute
			set_motor(MOTOR_OFF);
			//re-init mic
			init_mic();
			led_off(LED_R | LED_L);
			timer_set(&mytimer, 1);
			signaling = false;
			lastmaxval = 10000;
		} else { //sound was off wer're in measuring mode
			if (maxval > lastmaxval) {
				USART0_put_uint16(maxval);
				USART0_crlf();
				led_on(LED_R | LED_L);
				init_buzzr(); //buzzr to output
				if (sound_on)
					music_setNote(NOTE_C, 5);
				if (motor_on)
					set_motor(MOTOR_ON);
				signaling = true;
				timer_set(&mytimer, 5); //sound duration
			} else {
				timer_set(&mytimer, 1);
			};
			lastmaxval = maxval;
			maxval = 0;
		}; //end if soundon

	}; //end if timer_expired

}
;
//end do_mode0

void do_mode1(void) {
	static uint16_t tone;
	if (ModeChanged) {
		ModeChanged = false;
		tone = 1000;
		music_setNote(tone, 0);
		led_off(LED_L | LED_R);
	};
	if (btn_state(BTNST_SUP, BTN_LEFT)) {
		button_clear(BTN_LEFT);
		tone += 10;
		music_setNote(tone, 0);
	};
	if (btn_state(BTNST_SUP, BTN_RIGHT)) {
		button_clear(BTN_RIGHT);
		tone -= 10;
		music_setNote(tone, 0);
	};
}
;

void do_mode2(void) {
	static timer_t mytimer;
	uint8_t max = 50;
	uint8_t min = 5;
	uint16_t maxfreq = 5000;
	uint16_t minfreq = 1000;

	if (ModeChanged) {
		ModeChanged = false;
		music_setNote(NOTE_PAUSE, 4); //mute
		timer_set(&mytimer, 10);
		led_off(LED_L | LED_R);
		set_motor(MOTOR_OFF);
	}

	if (timer_expired(&mytimer)) {
		set_motor(MOTOR_OFF);
		music_setNote(NOTE_PAUSE, 0); //mute
		// set random led
		switch (rand() % 4) {
		case 0:
			led_on(LED_L);
			break;
		case 1:
			led_on(LED_R);
			break;
		case 2:
			led_on(LED_L | LED_R);
			break;
		default:
			led_off(LED_L | LED_R);
			break;
		};
		// decide if to switch motor on (40% chance)
		if (rand() % 5 > 2)
			set_motor(MOTOR_ON);

		//decide if to play sound (70% chance)
		if (rand() % 10 > 2) {
			music_setNote((rand() % (maxfreq - minfreq)) + minfreq, 0);

		}

		timer_set(&mytimer, (rand() % (max - min)) + min);
	}; //end if timer_expired

}
;

void do_mode3(void) {
	static timer_t mytimer;
	static bool blink;
	if (ModeChanged) {
		ModeChanged = false;
		music_setNote(NOTE_PAUSE, 4); //mute
		set_motor(MOTOR_OFF);
		timer_set(&mytimer, 10);
		blink = false;
	};

	if (timer_expired(&mytimer)) {
		if (!blink) {
			//lets blink
			led_on(LED_L | LED_R);
			timer_set(&mytimer, 1);
			blink = true;
		} else {
			//stop blink
			led_off(LED_L | LED_R);
			timer_set(&mytimer, 123);
			blink = false;
		}

	} //end if timer_expired

	if (btn_state(BTNST_SUP, BTN_LEFT)) {
		button_clear(BTN_LEFT);
		set_motor(MOTOR_OFF);

	};
	if (btn_state(BTNST_SUP, BTN_RIGHT)) {
		button_clear(BTN_RIGHT);
		set_motor(MOTOR_ON);
	};

}
;

void do_mode4(void) {
	uint8_t max = 200;
	uint8_t min = 10;

	static timer_t mytimer;
	static bool blink;
	if (ModeChanged) {
		music_setNote(NOTE_PAUSE, 0);
		ModeChanged = false;
		timer_set(&mytimer, 10);
		blink = false;
	};
	if (timer_expired(&mytimer)) {
		if (!blink) {
			//lets blink
			int i = (rand() % 3);
			switch (i) {
			case 0:
				led_on(LED_L);
				break;
			case 1:
				led_on(LED_R);
				break;
			default:
				led_on(LED_L | LED_R);
				break;
			};
			if (rand() % 10 > 8)
				set_motor(MOTOR_ON);
			music_setNote(NOTE_C, 5);
			timer_set(&mytimer, 2);
			blink = true;
		} else {
			//stop blink
			led_off(LED_L | LED_R);
			set_motor(MOTOR_OFF);
			music_setNote(NOTE_PAUSE, 0);
			timer_set(&mytimer, (rand() % (max - min)) + min);

			blink = false;
		}

	} //end if timer_expired

}
;

void __attribute__((noreturn))
main(void) {
	/* hardware initialisation: */
	init_leds();
	init_buzzr();
	init_switch();
	USART0_Init();
	init_motor();
	/* software initialisation */
	timer_init();
	music_init();

	/* here the show begins:*/sei();

	for (;;) /* ever */{
		//do something
		//main polling loop;
		button_poll();
		modeswitch_poll();
		switch (OpMode) {
		case MODE1:
			do_mode1();
			break;
		case MODE2:
			do_mode2();
			break;
		case MODE3:
			do_mode3();
			break;
		case MODE4:
			do_mode4();
			break;
		default:
			do_mode0();
			break;
		};

	};

	/* never  return 0; */
}
;

