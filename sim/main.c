#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include <util/delay.h>
#include <cpu_speed.h>
#include <macros.h>
#include <graphics.h>
#include <lcd.h>
#include "lcd_model.h"
#include "cab202_adc.h"
#include <avr/interrupt.h>
#include "bool.h"
#include "usb_serial.h"
#include "drawing.h"


#define BIT(x) (1 << (x))

char * spaceship = 
"               "
"               "
"               "
"               "
"oo   ooooo   oo"
"ooooooooooooooo"
"oo   ooooo   oo"
;
int ship_width = 15; int ship_height = 7;

char * asteroid = 
"   o   "
"  ooo  "
" ooooo "
"ooooooo"
" ooooo "
"  ooo  "
"   o   "
;
int ast_size = 7;

char * boulder = 
"  o  "
" ooo "
"ooooo"
" ooo "
"  o  "
;
int bould_size = 5;

char * fragment = 
" o "
"ooo"
" o "
;
int frag_size = 3;

char * plasma = 
"oo"
"oo"
"oo"
;
int plas_width = 2; int plas_height = 3;

int ship_xc = (LCD_X / 2) - (15 / 2);
int ship_yc = 41;

double shooter_angle = 0;
int fighter_velocity = 1;

bool paused = true;
bool game_finished = false;
bool left_flashing = false;
bool right_flashing = false;

int start_flashing_counter;

double plasma_x[20]; double plasma_y[20]; double plasma_angles[20];
int plasma_num = 0;

int asteroid_x[3]; double asteroid_y[3]; int boulder_x[6]; double boulder_y[6]; int fragment_x[12]; double fragment_y[12]; 
int asteroid_num; int boulder_num; int fragment_num;

double game_velocity;

int life;
int player_score = 0;

int lives = 5;

int aim_cheat_counter = 0;
int velocity_cheat_counter = 0;

volatile uint32_t game_overflow_counter = 0;
volatile uint32_t plasma_fire_overflow_counter = 0;

/* timer 3 interupt handler */
ISR(TIMER3_OVF_vect) {
	if (!paused) {
		game_overflow_counter ++;
		plasma_fire_overflow_counter ++;
	}
}

/* HELPERS */
bool is_opaque(int left, int top, int width, int height, char pixel, int i, int j) {
    if (i >= left && i < left + width && j >= top && j < top + height && pixel != ' ') {
        return true;
    }
    return false;
}

bool pixel_collision(int x0, int y0, int w0, int h0, char pixels0[], int x1, int y1, int w1, int h1, char pixels1[]) {
    for (int j = y0; j < y0 + h0; j++) {
        for (int i = x0; i < x0 + w0; i++) {
            if (is_opaque(x0, y0, w0, h0, pixels0[(i - x0) + (j - y0) * w0], i, j)
                && is_opaque(x1, y1, w1, h1, pixels1[(i - x1) + (j - y1) * w1], i, j)) {
                return true;
            }
        }
    } 
    return false;
}

void off_backlight() {	
	// (a)	Set bits 8 and 9 of Output Compare Register 4A.
	TC4H = 0 >> 8;

	// (b)	Set bits 0..7 of Output Compare Register 4A.
	OCR4A = 0 & 0xff;
}

void on_backlight() {	
	// (a)	Set bits 8 and 9 of Output Compare Register 4A.
	TC4H = 1023 >> 8;

	// (b)	Set bits 0..7 of Output Compare Register 4A.
	OCR4A = 1023 & 0xff;
}

void del_plasma(int index) {
	// shuffle all plasma arrays back by 1
	for (int i = index; i < plasma_num - 1; i++) {
		plasma_x[i] = plasma_x[i + 1];
		plasma_y[i] = plasma_y[i + 1];
		plasma_angles[i] = plasma_angles[i + 1];
	}
	plasma_num -= 1;
}

void del_asteroid(int index) {
	// shuffle all asteroid arrays back by 1
	for (int i = index; i < asteroid_num - 1; i++) {
		asteroid_x[i] = asteroid_x[i + 1];
		asteroid_y[i] = asteroid_y[i + 1];
	}
	asteroid_num -= 1;
}

void del_boulder(int index) {
	// shuffle all boulder arrays back by 1
	for (int i = index; i < boulder_num - 1; i++) {
		boulder_x[i] = boulder_x[i + 1];
		boulder_y[i] = boulder_y[i + 1];
	}
	boulder_num -= 1;
}

void del_fragment(int index) {
	// shuffle all fragment arrays back by 1
	for (int i = index; i < fragment_num - 1; i++) {
		fragment_x[i] = fragment_x[i + 1];
		fragment_y[i] = fragment_y[i + 1];
	}
	fragment_num -= 1;
}

bool is_offscreen(int left, int top, int right, int bottom) {
	// helper function to check if object is off the screen
	return (left < 0 || top < 0 || right > LCD_X || bottom > LCD_Y);
}

double get_elapsed_time(int overflow_counter) {
	// get time elapsed using timer overflows
    double time = ( overflow_counter * 65536.0 + TCNT3 ) * 8.0  / 8000000.0;
    return time;
}

int get_random_int(int lower, int upper) {
	// get random integer between a lower and upper bound
	int random = (rand() % (upper - lower + 1)) + lower;
	return random;
}

/* END HELPERS */


/* USB SERIAL */

void usb_serial_send(char * message) {
	// Cast to avoid "error: pointer targets in passing argument 1 
	//	of 'usb_serial_write' differ in signedness"
	usb_serial_write((uint8_t *) message, strlen(message));
}

int wait_int(char * wait_message) {
	// implementation of the get_int function
	usb_serial_send(wait_message);
	bool finished = false;
	int final_int = 0;
	while (!finished) {
		char buffer[10];
		int char_code = usb_serial_getchar();
		if (char_code >= 0) {
			if (!isdigit(char_code)) {
				usb_serial_send("\r\n");
				break;
			}
			final_int = final_int * 10 + (char_code - '0');
			snprintf(buffer, sizeof(buffer), "%c", char_code);
			usb_serial_send(buffer);
		} 
	}
	return final_int;
}

/* END USB SERIAL */

/* GAME STATUS */
void time_status(bool send) {
	// function that takes a bool and either displays the time status or sends it to computer
	char time_status[20];
    double diff = get_elapsed_time(game_overflow_counter);
    int minutes = floor(diff / 60);
    int seconds = (int) diff % 60;
	// format the time string
    if (minutes > 10 && seconds > 10) {
        sprintf(time_status, "Time: %d:%d\r\n", minutes, seconds);
    } else if (minutes > 10) {
        sprintf(time_status, "Time: %d:0%d\r\n", minutes, seconds);
    } else if (minutes < 10 && seconds < 10) {
        sprintf(time_status, "Time: 0%d:0%d\r\n", minutes, seconds);
    } else if (minutes < 10) {
        sprintf(time_status, "Time: 0%d:%d\r\n", minutes, seconds);
    } else if (seconds < 10) {
        sprintf(time_status, "Time: %d:0%d\r\n", minutes, seconds);
    } else if (minutes == 0) {
        sprintf(time_status, "Time: 00:%d\r\n", seconds);
    } else {
        sprintf(time_status, "Time: %d:%d\r\n", minutes, seconds);
    }
	if (send) {
		usb_serial_send(time_status);
	} else {
		draw_string(0, 1, time_status, FG_COLOUR);
	}
}

void send_game_status() {
	// time
	time_status(true);

	// lives
	char lives_status[15];
    sprintf(lives_status, "Lives: %d\r\n", lives);
	usb_serial_send(lives_status);

	// score
	char score_status[15];
    sprintf(score_status, "Score: %d\r\n", player_score);
	usb_serial_send(score_status);

	// asteroid_num
	char ast_status[30];
    sprintf(ast_status, "Asteroid Number: %d\r\n", asteroid_num);
	usb_serial_send(ast_status);

	// boulder_num
	char bould_status[30];
    sprintf(bould_status, "Boulder Number: %d\r\n", boulder_num);
	usb_serial_send(bould_status);

	// fragment_num
	char frag_status[30];
    sprintf(frag_status, "Fragment Number: %d\r\n", fragment_num);
	usb_serial_send(frag_status);

	// plasma_num
	char plas_status[30];
    sprintf(plas_status, "Plasma Number: %d\r\n", plasma_num);
	usb_serial_send(plas_status);

	// shooter_angle
	char angle_status[30];
    sprintf(angle_status, "Turret Angle: %.2f\r\n", shooter_angle);
	usb_serial_send(angle_status);

	// game_velocity
	char velocity_status[30];
    sprintf(velocity_status, "Game Velocity: %.2f\r\n\n", game_velocity);
	usb_serial_send(velocity_status);
}

void display_game_status() {
	// lives
	char lives_status[20];
    sprintf(lives_status, "Lives: %d\r\n", lives);

	// score
	char score_status[20];
    sprintf(score_status, "Score: %d\r\n", player_score);
	while (true) {
		// if unpaused
		if (BIT_IS_SET(PINB, 0) || usb_serial_getchar() == 'p') {
			paused = false;
			break;
		}

		// draw game status on teensy screen
		clear_screen();
		time_status(false);
		draw_string(0, 10, lives_status, FG_COLOUR);
		draw_string(0, 20, score_status, FG_COLOUR);
		show_screen();
		_delay_ms(100);
	}
}
/* END GAME STATUS */

/* SCREENS */

void intro(){
	// function for intro screen and animation
    char * ship =
    "ooooooo                  "
	"    ooo                  "
	"oooooooooooooooo         "
	"    ooooooooooooooooooooo"
	"oooooooooooooooo         "
	"    ooo                  "
	"ooooooo                  "
	;

    int counter = 0;

	int ship_x[4] = {-25, 0, 25, 50};
    while(counter < 1000){
        clear_screen();

        //if left button or 'r' pressed skip intro
        if(BIT_IS_SET(PINF, 6) || usb_serial_getchar() == 'r'){
            return;
        }

		// game name and student number
		draw_string(0, 0, "ASTRDZ", FG_COLOUR);
        draw_string(0, 10, "n9576321", FG_COLOUR);

		// handle animating spaceships
        for(int i = 0; i < 4; i++){
			int y;
			if (i % 2 == 0) {
				y = 20;
			} else {
				y = 35;
			}
            draw_pixels(ship_x[i], y, 25, 7, ship, true);
            ship_x[i]++;
            if(ship_x[i] > LCD_X){
                ship_x[i] = -25;
            }
        }
        show_screen();
        counter++;
        _delay_ms(100);
    }
}

void quit_screen() {
	// invert display
	LCD_CMD(lcd_set_display_mode, lcd_display_inverse);

	// show student number
	while (true) {
		clear_screen();
		draw_string(10, 10, "n9576321", FG_COLOUR);
		show_screen();
	}
}

/* END SCREENS */

void draw_all() {
	// call draw_everything from drawing.c
	draw_everything(ship_xc, ship_yc, ship_width, ship_height, spaceship, shooter_angle, plasma_num, plasma_x,
		plasma_y, plasma, asteroid_num, asteroid_x, asteroid_y, ast_size, asteroid, boulder_num, boulder_x,
		boulder_y, bould_size, boulder, fragment_num, fragment_x, fragment_y, frag_size, fragment);
}

void set_init_values() {
    paused = true;
	left_flashing = false;
	right_flashing = false;
    shooter_angle = 0;
	
	// choose random starting velocity
	if (get_random_int(0, 100) > 50) {
		fighter_velocity = 1;
	} else {
		fighter_velocity = -1;
	}
    ship_xc = (LCD_X / 2) - (ship_width / 2);
	ship_yc = 41;
    game_overflow_counter = 0;
	plasma_fire_overflow_counter = 0;
	aim_cheat_counter = 0;
	velocity_cheat_counter = 0;
	
	asteroid_num = 0;
	boulder_num = 0;
	fragment_num = 0;
	plasma_num = 0;
	player_score = 0;
	lives = 5;
	draw_all();

	// wait for unpause for game to start
	while (paused) {
		if (BIT_IS_SET(PINB, 0) || usb_serial_getchar() == 'p') {
			paused = false;
			usb_serial_send("Let the games commence!\r\n\n");
			send_game_status();
			break;
		}
		_delay_ms(100);
	}
};

void setup_usb_serial(void) {
	usb_init();

	while (!usb_configured()) {
		// Block until USB is ready.
	}
}

void pwm_init() {
	// Enable PWM
	TC4H = 1023 >> 8;
	OCR4C = 1023 & 0xff;
	TCCR4A = BIT(COM4A1) | BIT(PWM4A);
	SET_BIT(DDRC, 7);
	TCCR4B = BIT(CS42) | BIT(CS41) | BIT(CS40);
	TCCR4D = 0;
}

void enable_inputs() {
	//	Enable input from the Center, Left, Right, Up, and Down switches of the joystick.
	CLEAR_BIT(DDRB, 0);
    CLEAR_BIT(DDRB, 1);
    CLEAR_BIT(DDRD, 0);
    CLEAR_BIT(DDRD, 1);
    CLEAR_BIT(DDRB, 7);

	// left and right buttons
	CLEAR_BIT(DDRF, 5);
	CLEAR_BIT(DDRF, 6);

	// Enable input from the left thumb wheel
	adc_init();

	pwm_init();
}

void setup_timer() {
	// Initialise Timer 3
	TCCR3A = 0;
	TCCR3B = 2;

	// Enable timer overflow
    TIMSK3 = 1;

	// Turn on interrupts.
    sei();
}

bool x_overlap(int x1, int x2) {
	// check if the x positions of two asteroids overlap
	return ((x1 > x2 && x1 < x2 + 7) ||
						(x1 + 7 > x2 && x1 + 7 < x2 + 7));
}

void setup_asteroids() {
	int middle = LCD_X / 2;
	int left_count = 0;
	int right_count = 0;
	for (int i = 0; i < 3; i++) {
		int x = get_random_int(0, LCD_X - 8);
		
		// collision detection
		if (i != 0) {
			for (int j = 0; j < asteroid_num; j++) {
				int ast_x = asteroid_x[j];
				while (true) {
					if (x_overlap(x, ast_x)) {
						x = get_random_int(0, LCD_X - 8);
					} else {
						break;
					}
				}
			}
		}

		double y = -10;
		
		asteroid_x[i] = x;
		asteroid_y[i] = y;

		asteroid_num += 1;

		// count which side an asteroid is on
		if (x + 4 < middle) {
			left_count ++;
		} else if (x + 4 > middle) {
			right_count ++;
		}
	}
	// flash led of whichever side more asteroids are on
	if (left_count > right_count) {
		left_flashing = true;
	} else {
		right_flashing = true;
	}
	start_flashing_counter = game_overflow_counter;
}


void setup(void) {
    set_clock_speed(CPU_8MHz);
	enable_inputs();
	setup_timer();
	setup_usb_serial();
    lcd_init(LCD_DEFAULT_CONTRAST);
	on_backlight();
}

void fire_plasma() {
	// only fire if there are less than 20 bolts on screen and it has been more than 0.2 seconds since last shot
	if (plasma_num < 20 && get_elapsed_time(plasma_fire_overflow_counter) > 0.2) {
		int x = get_turret_tip_x(ship_xc + ((int)ship_width/2), shooter_angle);
		int y = get_turret_tip_y(45, shooter_angle);

		plasma_x[plasma_num] = x;
		plasma_y[plasma_num] = y;
		plasma_angles[plasma_num] = shooter_angle;
		plasma_num += 1;

		plasma_fire_overflow_counter = 0;
	}
}

void process_potentiometers() {
	// left wheel
	if (get_elapsed_time(game_overflow_counter) - get_elapsed_time(aim_cheat_counter) > 1) {
		long left_adc = adc_read(0);

		// (V × R2 ÷ R1) + (M2 - M1)
		shooter_angle = (left_adc * 120 / 1024) - 60;
	}

	// right wheel
	if (get_elapsed_time(game_overflow_counter) - get_elapsed_time(velocity_cheat_counter) > 1) {
		long right_adc = adc_read(1);

    	game_velocity = right_adc/1023.0;
	}
}

void move_spaceship_left() {
	if (ship_xc > 0 && fighter_velocity > -1) {
		fighter_velocity -= 1;
	}
}

void move_spaceship_right() {
	if (ship_xc + ship_width < LCD_X && fighter_velocity < 1) {
		fighter_velocity += 1;
	}
}

void send_display_game_status() {
	// send info to the computer
	send_game_status();
	if (paused) {
		// display info on teensy if game is paused
		display_game_status();
	}
}

void reset() {
	set_init_values();
	setup_asteroids();
}

void game_over() {
	// function for displaying game over screen
    int start_time = game_overflow_counter;
	off_backlight();

	// turn on leds
    SET_BIT(PORTB, 2);
    SET_BIT(PORTB, 3);

	usb_serial_send("Game over man!\r\n");
	send_game_status();

    while (true) {	
		clear_screen();
		draw_string(10, 10, "GAME OVER", FG_COLOUR);
        int diff = get_elapsed_time(game_overflow_counter) - get_elapsed_time(start_time);

        //turn off leds after 2 seconds
        if(diff >= 2){
            CLEAR_BIT(PORTB, 2);
            CLEAR_BIT(PORTB, 3);
			on_backlight();
			draw_string(5, 20, "RESTART OR QUIT", FG_COLOUR);
        }

		uint16_t char_code = usb_serial_getchar();

        // reset if left button pressed or 'r'
        if(BIT_IS_SET(PINF, 6) || char_code == 'r'){
            reset();
			game_finished = false;
            return;

        // quit if right button pressed or 'q'
        }else if (BIT_IS_SET(PINF, 5) || char_code == 'q') {
            game_finished = true;
			quit_screen();
            return;
        }
		show_screen();
        _delay_ms(100);
    }
}

void handle_exit() {
	game_finished = true;
	quit_screen();
}

void set_turret_aim() {
	// cheat for setting turret aim
	int angle = wait_int("New turret angle: ");

	if (angle < 0) {
        angle = 0;
    } else if (angle > 1023) {
        angle = 1023;
    }

    shooter_angle = ((double)angle * 120/1024) - 60;
    aim_cheat_counter = game_overflow_counter;
}

void set_game_speed() {
	// cheat for setting game speed
	int vel = wait_int("Velocity: ");

    if (vel < 0) {
        vel = 0;
    } else if (vel > 1023) {
        vel = 1023;
    }

    game_velocity = vel/1023;
    velocity_cheat_counter = game_overflow_counter;
}

void set_shield_lives() {
	// cheat for setting lives of shield
	lives = wait_int("Lives: ");
}

void set_score() {
	// cheat for setting game score
	player_score = wait_int("Score: ");
}

void show_help_screen_computer() {
	// function for displaying game controls
	usb_serial_send("\n");
    usb_serial_send("Teensy:\r\n");
    usb_serial_send("left: move left\r\n");
    usb_serial_send("right: move right\r\n");
    usb_serial_send("up: fire\r\n");
    usb_serial_send("down: send + display game status\r\n");
    usb_serial_send("centre: pause\r\n");
    usb_serial_send("left button: start/reset\r\n");
    usb_serial_send("right button: quit\r\n");
    usb_serial_send("left wheel: aim turret\r\n");
    usb_serial_send("right wheel: set speed\r\n\n");

    usb_serial_send("Computer:\r\n");
    usb_serial_send("a: move left\r\n");
    usb_serial_send("d: move right\r\n");
    usb_serial_send("w: fire \r\n");
    usb_serial_send("s: send + display game status\r\n");
    usb_serial_send("r: start/reset\r\n");
    usb_serial_send("p: pause \r\n");
    usb_serial_send("q: quit\r\n");
	usb_serial_send("o: aim turret\r\n");
    usb_serial_send("m: set speed\r\n");
    usb_serial_send("l: set lives\r\n");
    usb_serial_send("g: set score\r\n");
    usb_serial_send("?: print controls\r\n");
    usb_serial_send("h: move ship\r\n");
    usb_serial_send("j: place asteroid\r\n");
    usb_serial_send("k: place boulder\r\n");
    usb_serial_send("i: place fragment\r\n");
}

void move_spaceship() {
	// cheat for moving spaceship to x pos
	ship_xc = wait_int("X pos: ");
	if (ship_xc < 0) {
		ship_xc = 0;
	} else if (ship_xc + ship_width > LCD_X) {
		ship_xc = LCD_X - ship_width;
	}
}

void add_asteroid(int x, int y) {
	// if there are more than 3 asteroids, do nothing
	if (asteroid_num >= 3) return;
	asteroid_x[asteroid_num] = x;
	asteroid_y[asteroid_num] = y;
	asteroid_num += 1;
}

void add_boulder(int x, int y) {
	// if there are more than 6 boulders, do nothing
	if (boulder_num >= 6) return;
	boulder_x[boulder_num] = x;
	boulder_y[boulder_num] = y;
	boulder_num += 1;
}

void add_fragment(int x, int y) {
	// if there are more than 12 fragments, do nothing
	if (fragment_num >= 12) return;
	fragment_x[fragment_num] = x;
	fragment_y[fragment_num] = y;
	fragment_num += 1;
}

void cheat_add_asteroid() {
	// cheat for adding asteroid
	int x = wait_int("X pos: ");
	int y = wait_int("Y pos: ");

	// verify x is on screen
	if (x < 0) {
		x = 0;
	} else if (x > LCD_X) {
		x = LCD_X;
	}

	// verify y is on screen
	if (y < 0) {
		y = 0;
	} else if (y > LCD_Y) {
		y = LCD_Y;
	}
	add_asteroid(x, y);
}

void cheat_add_boulder() {
	// cheat for adding boulder
	int x = wait_int("X pos: ");
	int y = wait_int("Y pos: ");

	// verify x is on screen
	if (x < 0) {
		x = 0;
	} else if (x > LCD_X) {
		x = LCD_X;
	}

	// verify y is on screen
	if (y < 0) {
		y = 0;
	} else if (y > LCD_Y) {
		y = LCD_Y;
	}
	add_boulder(x, y);
}

void cheat_add_fragment() {
	// cheat for adding fragment
	int x = wait_int("X pos: ");
	int y = wait_int("Y pos: ");

	// verify x is on screen
	if (x < 0) {
		x = 0;
	} else if (x > LCD_X) {
		x = LCD_X;
	}

	// verify y is on screen
	if (y < 0) {
		y = 0;
	} else if (y > LCD_Y) {
		y = LCD_Y;
	}
	add_fragment(x, y);
}

void handle_keyboard_input(int16_t char_code) {
	if (char_code == 'a') {
		move_spaceship_left();
	} else if (char_code == 'd') {
		move_spaceship_right();
	} else if (char_code == 'w') {
		fire_plasma();
	} else if (char_code == 's') {
		send_display_game_status();
	} else if (char_code == 'r') {
		reset();
	} else if (char_code == 'p') {
		paused = true;
	} else if (char_code == 'q') {
		handle_exit();
	} else if (char_code == 'o') {
		set_turret_aim();
	} else if (char_code == 'm') {
		set_game_speed();
	} else if (char_code == 'l') {
		set_shield_lives();
	} else if (char_code == 'g') {
		set_score();
	} else if (char_code == '?') {
		show_help_screen_computer();
	} else if (char_code == 'h') {
		move_spaceship();
	} else if (char_code == 'j') {
		cheat_add_asteroid();
	} else if (char_code == 'k') {
		cheat_add_boulder();
	} else if (char_code == 'i') {
		cheat_add_fragment();
	}
}

void handle_left_led() {
	// if we want led to flash
	if (left_flashing) {
		double time = get_elapsed_time(game_overflow_counter);
		// flash the led based on elapsed time
		if (fmod(time,1) < 0.5) {
			SET_BIT(PORTB, 2);
		} else {
			CLEAR_BIT(PORTB, 2);
		}
		if (time - get_elapsed_time(start_flashing_counter) > 2) {
			left_flashing = false;
		}
	} else {
		CLEAR_BIT(PORTB, 2);
	}
}

void handle_right_led() {
	// if we want led to flash
	if (right_flashing) {
		double time = get_elapsed_time(game_overflow_counter);
		// flash the led based on elapsed time
		if (fmod(time, 1) < 0.5) {
			SET_BIT(PORTB, 3);
		} else {
			CLEAR_BIT(PORTB, 3);
		}
		if (time - get_elapsed_time(start_flashing_counter) > 2) {
			right_flashing = false;
		}
	} else {
		CLEAR_BIT(PORTB, 3);
	}
}

void process_teensy_inputs() {
	 // detect joystick up, down, left right
    if (BIT_IS_SET(PIND, 1)) {
        fire_plasma();
    } else if (BIT_IS_SET(PINB, 7)) {
        send_display_game_status();
    } else if (BIT_IS_SET(PINB, 1)) {
		move_spaceship_left();
    } else if (BIT_IS_SET(PIND, 0)) {
		move_spaceship_right();
    } else if (BIT_IS_SET(PINB, 0)) {
        paused = !paused;
    } else if (BIT_IS_SET(PINF, 6)) {
		reset();
	} else if (BIT_IS_SET(PINF, 5)) {
		game_finished = true;
		quit_screen();
	}

	handle_left_led();

	handle_right_led();

	process_potentiometers();
}

void process_serial_inputs() {
	int16_t char_code = usb_serial_getchar();
	if (char_code >= 0) {
		handle_keyboard_input(char_code);
	}
}

void process_ship() {
	// add the current velocity to the x position if the ship isn't over the screen width
	if (ship_xc + fighter_velocity + ship_width < LCD_X &&
		ship_xc + fighter_velocity > 0) {
			ship_xc += fighter_velocity;
	} else {
		fighter_velocity = 0;
	}
}


void process_asteroids() {
	for (int i = 0; i < asteroid_num; i++) {
		// move asteroids down
		asteroid_y[i] = asteroid_y[i] + game_velocity;

		// if deflector shield is hit
		if (asteroid_y[i] + ast_size >= 40){
            del_asteroid(i);
            lives -= 1;
        }
	}
}

void process_boulders() {
	for (int i = 0; i < boulder_num; i++) {
		// move boulders down
		boulder_y[i] = boulder_y[i] + game_velocity;

		// if deflector shield is hit
		if (boulder_y[i] + bould_size >= 40){
            del_boulder(i);
            lives -= 1;
        }
	}
}

void process_fragments() {
	for (int i = 0; i < fragment_num; i++) {
		// move asteroids down
		fragment_y[i] = fragment_y[i] + game_velocity;

		// if deflector shield is hit
		if (fragment_y[i] + frag_size >= 40){
            del_fragment(i);
            lives -= 1;
        }
	}
}

void process_plasma() {
	for (int i = 0; i < plasma_num; i++) {
		// move plasma in its current direction
		double x1 = plasma_x[i];
		double y1 = plasma_y[i];
		double plasma_angle = plasma_angles[i];

		double x2 = x1 - 1 * sin(M_PI * (plasma_angle * -1) / 180);
		double y2 = y1 -1 * cos(M_PI * (plasma_angle * -1) / 180);

		// if plasma is offscreen, delete it
		if (is_offscreen(x2, y2, x2 + plas_width, y2 + plas_height)) {
			del_plasma(i);
		} else {
			plasma_x[i] = x2;
			plasma_y[i] = y2;
		}
	}
}

void check_asteroid_collisions() {
	// function to check if any plasma has hit any asteroid
	for (int i = 0; i < plasma_num; i++) {
		for (int j = 0; j < asteroid_num; j++) {
			if (pixel_collision(plasma_x[i], plasma_y[i], plas_width, plas_height, plasma, asteroid_x[j], asteroid_y[j], ast_size, ast_size, asteroid)) {
				int init_x = asteroid_x[j];
				if (init_x + 14 > LCD_X) {
					init_x = LCD_X - 14;
				}
				// spawn two boulders
				add_boulder(init_x, asteroid_y[j]);
				add_boulder(init_x + 6, asteroid_y[j]);

				// delete asteroid and plasma
				del_asteroid(j);
				del_plasma(i);

				i--;
                j--;

				player_score += 1;
			}
		}
	}
}

void check_boulder_collisions() {
	// function to check if any plasma has hit any boulder
	for (int i = 0; i < plasma_num; i++) {
		for (int j = 0; j < boulder_num; j++) {
			if (pixel_collision(plasma_x[i], plasma_y[i], plas_width, plas_height, plasma, boulder_x[j], boulder_y[j], bould_size, bould_size, boulder)) {
				int init_x = boulder_x[j];
				if (init_x + 10 > LCD_X) {
					init_x = LCD_X - 10;
				}
				// spawn two fragments
				add_fragment(init_x, boulder_y[j]);
				add_fragment(init_x + 4, boulder_y[j]);

				// delete boulder and plasma
				del_boulder(j);
				del_plasma(i);

				i--;
                j--;

				player_score += 2;
			}
		}
	}
}

void check_fragment_collisions() {
	// function to check if any plasma has hit any asteroid
	for (int i = 0; i < plasma_num; i++) {
		for (int j = 0; j < fragment_num; j++) {
			if (pixel_collision(plasma_x[i], plasma_y[i], plas_width, plas_height, plasma, fragment_x[j], fragment_y[j], frag_size, frag_size, fragment)) {
				// don't spawn anything, just delete fragment and plasma
				del_fragment(j);
				del_plasma(i);

				i--;
                j--;

				player_score += 4;
			}
		}
	}
}

void check_collisions() {
	check_asteroid_collisions();
	check_boulder_collisions();
	check_fragment_collisions();
}

void process_screen_stuff() {
	// responsible for processing anything on screen
	process_ship();
	process_asteroids();
	process_boulders();
	process_fragments();
	process_plasma();
	check_collisions();

	draw_all();
}

void process(void) {	
	// responsible for processing everything
	process_screen_stuff();
	if (lives <= 0) {
		game_finished = true;
		game_over();
	}

	if (asteroid_num == 0 && boulder_num == 0 && fragment_num == 0) {
		setup_asteroids();
	}
}

void manage_loop() {
	// responsible for managing the event loop
	process_serial_inputs();
	process_teensy_inputs();
	if (paused) {
		return;
	}
	process();
}

int main(void) {
	srand(0);
	setup();
	intro();
	draw_all();
	set_init_values();
	setup_asteroids();
	while (!game_finished) {
		manage_loop();
		_delay_ms(100);
	}
	clear_screen();

	return 0;
}