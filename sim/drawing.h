#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <limits.h>
#include <lcd.h>
#include "lcd_model.h"

void draw_pixels(int left, int top, int width, int height, char bitmap[], bool space_is_transparent);

int get_turret_tip_x(int turret_base_x, double turret_angle);

int get_turret_tip_y(int turret_base_y, double turret_angle);

void draw_everything(int ship_xc, int ship_yc, int ship_width, int ship_height, char * spaceship, 
            double shooter_angle, int plasma_num, double plasma_x[], double plasma_y[], char * plasma,
            int asteroid_num, int asteroid_x[], double asteroid_y[], int ast_size, char * asteroid,
            int boulder_num, int boulder_x[], double boulder_y[], int bould_size, char * boulder,
            int fragment_num, int fragment_x[], double fragment_y[], int frag_size, char * fragment);