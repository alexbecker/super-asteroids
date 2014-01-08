#include <stdlib.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <unistd.h>
#include <ncurses.h>

#define PI 3.1415926
#define SHIP_CH ACS_BLOCK
#define FRAMETIME .1
#define SHIP_ACCEL .1	// acceleration in chars/frame^2

typedef struct _Ship {
	double y_pos, x_pos;
	double y_vel, x_vel;
} Ship;

typedef enum { TORUS, SPHERE, PROJECTIVE, KLEIN } mode;

int get_y(double y, int y_max, mode m) {
	if (m == TORUS) {
		return ((int) y) % y_max;
	}
}

int get_x(double x, int x_max, mode m) {
	if (m == TORUS) {
		return ((int) x) % x_max;
	}
}

void move_ship(Ship *s, char input, mode m) {
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

    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

	mvaddch(get_y(s->y_pos, w.ws_row, m), get_x(s->x_pos, w.ws_col, m), SHIP_CH);
}

int main(int argc, char **argv)
{	
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
		move_ship(s, input, TORUS);
		refresh();
	}
	endwin();

	return 0;
}
