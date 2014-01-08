#include <stdlib.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <unistd.h>
#include <ncurses.h>

#define PI 3.1415926
#define SHIP_CH ACS_BLOCK
#define FRAMETIME .1
#define SHIP_ACCEL .1		// acceleration in chars/frame^2
#define MISSILE_VEL 1.0		// velocity in chars/frame
#define MISSILE_RANGE 100.0 // maximum range in chars

typedef struct _Ship {
	double y_pos, x_pos;
	double y_vel, x_vel;
} Ship;

typedef struct _Missile {
	double y_pos, x_pos, range;
} Missile, *Missiles;

typedef enum { TORUS, SPHERE, PROJECTIVE, KLEIN } mode;

int get_y(double y, double x, int y_max, int x_max, mode m) {
	int y_mod = ((int) y) % y_max;
	if (m == TORUS || m == KLEIN) {
		return y_mod;
	}
}

int get_x(double y, double x, int y_max, int x_max, mode m) {
	int x_mod = ((int) x) % x_max;
	int y_overflow = ((int) y) / y_max;
	if (m == TORUS) {
		return x_mod;
	} else if (m == KLEIN) {
		if (y_overflow % 2 == 0) {
			return x_mod;
		} else { 
			if (x_mod == x_max) {
				return 0;
			} else {
				return x_max - x_mod - 1;
			}
		}
	}
}

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

	s->y_pos += s->y_vel * FRAMETIME;
	s->x_pos += s->x_vel * FRAMETIME;

	mvaddch(get_y(s->y_pos, s->x_pos, y_max, x_max, m), get_x(s->y_pos, s->x_pos, y_max, x_max, m), SHIP_CH);
}

int main(int argc, char **argv)
{	
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

	Ship *s = malloc(sizeof(Ship));
	s->y_pos = 0, s->x_pos = 0, s->y_vel = 0, s->x_vel = 0;

	initscr();
	cbreak();
	halfdelay(1);
	noecho();
	curs_set(0);
	char input;
	for (int i=0 ;; i++) {
		input = getch();
		clear();
		refresh();
		move_ship(s, input, w.ws_row, w.ws_col, KLEIN);
		refresh();
	}
	endwin();

	return 0;
}
