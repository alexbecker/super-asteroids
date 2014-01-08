#include <stdlib.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <unistd.h>
#include <ncurses.h>
#include <math.h>

#define PI 3.1415926
#define SHIP_CH ACS_BLOCK
#define MISSILE_CH ACS_BULLET
#define SHIP_ACCEL .1		// acceleration in chars/frame^2
#define MISSILE_VEL 1		// velocity in chars/frame
#define MISSILE_RANGE 250	// maximum range in chars
#define MAX_MISSILES 256

typedef struct _Ship {
	double y_pos, x_pos;
	double y_vel, x_vel;
} Ship;

typedef struct _Missile {
	double y_pos, x_pos;
	double y_vel, x_vel;
	double range;
} Missile, *Missiles;

typedef enum { TORUS, KLEIN, SPHERE, PROJECTIVE } mode;

void move_ship(Ship *s, char input, int y_max, int x_max, mode m) {
	if (input == 'w') {
		s->y_vel -= SHIP_ACCEL;
	} else if (input == 'd') {
		s->x_vel += SHIP_ACCEL;
	} else if (input == 's') {
		s->y_vel += SHIP_ACCEL;
	} else if (input == 'a') {
		s->x_vel -= SHIP_ACCEL;
	}

	int crossed_y, crossed_x;
	crossed_y = (int) floor((s->y_pos + s->y_vel) / y_max);
	crossed_x = (int) floor((s->x_pos + s->x_vel) / x_max);

	s->y_pos = fmod(s->y_pos + s->y_vel, y_max);
	if (s->y_pos < 0) {
		s->y_pos += y_max;
	}
	s->x_pos = fmod(s->x_pos + s->x_vel, x_max);
	if (s->x_pos < 0) {
		s->x_pos += x_max;
	}

	// in KLEIN mode, crossing the y axis flips the object across the x axis
	if (m == KLEIN && crossed_y) {
		s->x_pos = x_max - s->x_pos;
	}

	mvaddch((int) s->y_pos, (int) s->x_pos, SHIP_CH);
}

void move_missiles(Ship *s, Missiles cur_missiles, char input, int y_max, int x_max, mode m) {
	// add new missile if user has pressed u, k, j or h
	int missile_ind = 0;
	if (input == 'u' || input == 'k' || input == 'j' || input == 'h') {
		while (cur_missiles[missile_ind].range > 0) {
			missile_ind++;
		}
		cur_missiles[missile_ind].y_pos = s->y_pos;
		cur_missiles[missile_ind].x_pos = s->x_pos;
		cur_missiles[missile_ind].range = MISSILE_RANGE;
	}
	if (input == 'u') {
		cur_missiles[missile_ind].y_vel = s->y_vel - MISSILE_VEL;
		cur_missiles[missile_ind].x_vel = s->x_vel;
	} else if (input == 'k') {
		cur_missiles[missile_ind].y_vel = s->y_vel;
		cur_missiles[missile_ind].x_vel = s->x_vel + MISSILE_VEL;
	} else if (input == 'j') {
		cur_missiles[missile_ind].y_vel = s->y_vel + MISSILE_VEL;
		cur_missiles[missile_ind].x_vel = s->x_vel;
	} else if (input == 'h') {
		cur_missiles[missile_ind].y_vel = s->y_vel;
		cur_missiles[missile_ind].x_vel = s->x_vel - MISSILE_VEL;
	}

	// move and print missiles
	int crossed_y, crossed_x;
	for (int i=0; i<MAX_MISSILES; i++) {
		if (cur_missiles[i].range <= 0) {
			continue;
		}

		// test for crossing the axes
		crossed_y = (int) floor((cur_missiles[i].y_pos + cur_missiles[i].y_vel) / y_max);
		crossed_x = (int) floor((cur_missiles[i].x_pos + cur_missiles[i].x_vel) / x_max);

		// move missiles
		cur_missiles[i].y_pos = fmod(cur_missiles[i].y_pos + cur_missiles[i].y_vel, y_max);
		if (cur_missiles[i].y_pos < 0) {
			cur_missiles[i].y_pos += y_max;
		}
		cur_missiles[i].x_pos = fmod(cur_missiles[i].x_pos + cur_missiles[i].x_vel, x_max);
		if (cur_missiles[i].x_pos < 0) {
			cur_missiles[i].x_pos += x_max;
		}
	
		// in KLEIN mode, crossing the y axis flips the object across the x axis
		if (m == KLEIN && crossed_y) {
			cur_missiles[i].x_pos = x_max - cur_missiles[i].x_pos;
		}

		// decrease remaining range
		cur_missiles[i].range -= MISSILE_VEL;

		// update frame
		mvaddch(cur_missiles[i].y_pos, cur_missiles[i].x_pos, MISSILE_CH);
	}
}

int main(int argc, char **argv)
{	
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

	Ship *s = malloc(sizeof(Ship));
	s->y_pos = 0, s->x_pos = 0, s->y_vel = 0, s->x_vel = 0;

	Missiles cur_missiles = malloc(MAX_MISSILES * sizeof(Missile));
	for (int i=0; i<MAX_MISSILES; i++) {
		cur_missiles[i].range = 0;
	}

	mode m = TORUS;

	initscr();
	cbreak();
	halfdelay(1);
	noecho();
	curs_set(0);
	char input;
	while (1) {
		input = getch();

		// check for mode change
		if (input == ' ') {
			m = (m + 1) % 2;
		}

		// draw next frame	
		clear();
		refresh();
		move_ship(s, input, w.ws_row, w.ws_col, m);
		move_missiles(s, cur_missiles, input, w.ws_row, w.ws_col, m);
		refresh();
	}
	endwin();

	return 0;
}
