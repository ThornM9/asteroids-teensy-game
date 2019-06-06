#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <limits.h>
#include <lcd.h>
#include "lcd_model.h"
#include <graphics.h>

void draw_pixels(int left, int top, int width, int height, char bitmap[], bool space_is_transparent) {
    for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; i++) {
            if (bitmap[i + j * width] != ' ') {
                draw_pixel(left + i, top + j, FG_COLOUR);
            } else if (!space_is_transparent) {
                draw_pixel(left + i, top + j, FG_COLOUR);
            }
        }
    }
}

void draw_ship(int x, int y, int ship_width, int ship_height, char * spaceship) {
	draw_pixels(x, y, ship_width, ship_height, spaceship, true);
}

int get_turret_tip_x(int turret_base_x, double turret_angle) {
    // helper function to get x coord of tip of turret
	int turret_tip_x = turret_base_x + -4 * sin(M_PI * (turret_angle * -1) / 180);
	return turret_tip_x;
}

int get_turret_tip_y(int turret_base_y, double turret_angle) {
    // helper function to get y coord of tip of turret
	int turret_tip_y = turret_base_y + -4 * cos(M_PI * (turret_angle * -1) / 180);
	return turret_tip_y;
}

void draw_process_turret(int ship_left, int ship_width, double turret_angle) {
	int turret_base_x = ship_left + ((int)ship_width/2);
	int turret_base_y = 45;

	int turret_tip_x = get_turret_tip_x(turret_base_x, turret_angle);
	int turret_tip_y = get_turret_tip_y(turret_base_y, turret_angle);

	draw_line(turret_base_x, turret_base_y, turret_tip_x, turret_tip_y, FG_COLOUR);
}

void draw_shield() {
	for (int i = 0; i < LCD_X; i += 10) {
		draw_line(i, 39, i + 5, 39, FG_COLOUR);
	}
}

void draw_plasma(int plasma_num, double plasma_x[], double plasma_y[], char * plasma) {
	for (int i = 0; i < plasma_num; i++) {
		draw_pixels(plasma_x[i], plasma_y[i], 2, 3, plasma, true);
	}
}

void draw_asteroids(int asteroid_num, int asteroid_x[], double asteroid_y[], int ast_size, char * asteroid) {
	for (int i = 0; i < asteroid_num; i++) {
		draw_pixels(asteroid_x[i], asteroid_y[i], ast_size, ast_size, asteroid, true);
	}
}

void draw_boulders(int boulder_num, int boulder_x[], double boulder_y[], int bould_size, char * boulder) {
	for (int i = 0; i < boulder_num; i++) {
		draw_pixels(boulder_x[i], boulder_y[i], bould_size, bould_size, boulder, true);
	}
}

void draw_fragments(int fragment_num, int fragment_x[], double fragment_y[], int frag_size, char * fragment) {
	for (int i = 0; i < fragment_num; i++) {
		draw_pixels(fragment_x[i], fragment_y[i], frag_size, frag_size, fragment, true);
	}
}

void draw_everything(int ship_xc, int ship_yc, int ship_width, int ship_height, char * spaceship, 
            double shooter_angle, int plasma_num, double plasma_x[], double plasma_y[], char * plasma,
            int asteroid_num, int asteroid_x[], double asteroid_y[], int ast_size, char * asteroid,
            int boulder_num, int boulder_x[], double boulder_y[], int bould_size, char * boulder,
            int fragment_num, int fragment_x[], double fragment_y[], int frag_size, char * fragment) {
	clear_screen();
	draw_ship(ship_xc, ship_yc, ship_width, ship_height, spaceship);
	draw_process_turret(ship_xc, ship_width, shooter_angle);
	draw_shield();
	draw_plasma(plasma_num, plasma_x, plasma_y, plasma);
	draw_asteroids(asteroid_num, asteroid_x, asteroid_y, ast_size, asteroid);
	draw_boulders(boulder_num, boulder_x, boulder_y, bould_size, boulder);
	draw_fragments(fragment_num, fragment_x, fragment_y, frag_size, fragment);
	show_screen();
}