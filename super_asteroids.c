#include <stdlib.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <unistd.h>
#include <ncurses.h>
#include <math.h>
#include <assert.h>

#define MAX(A, B) (A) > (B) ? (A) : (B)
#define MIN(A, B) (A) < (B) ? (A) : (B)

#define PI 3.1415926
#define MODE_CHANGE_CH ' '		// character signalling mode change
#define EXIT_CH	'\n'			// character signalling program exit
#define SHIP_CH ACS_BLOCK		// character representing ship
#define MISSILE_CH ACS_BULLET	// character representing bullet
#define ASTEROID_CH '*'			// character representing asteroid
#define SHIP_ACCEL .1			// ship acceleration in chars/frame^2
#define MISSILE_VEL 1			// missile velocity in chars/frame
#define MISSILE_RANGE 250		// missile maximum range in chars
#define MAX_MISSILES 256		// upper bound on number of live missiles
#define SAFE_ZONE_RADIUS 10		// radius (l_infinity) around ship in which no asteroids will appear
#define ASTEROID_FREQ 0.2		// probability of new asteroid appearing each frame
#define ASTEROID_VEL 1			// asteroid velocity in chars/frame
#define MAX_ASTEROIDS 512		// upper bound on number of asteroids on screen

typedef struct _Ship {
	double y_pos, x_pos;
	double y_vel, x_vel;
} Ship;

typedef struct _Missile {
	double y_pos, x_pos;
	double y_vel, x_vel;
	double range;	// missile is removed from screen when range <= 0
} Missile, *Missiles;

typedef struct _Asteroid {
	double y_pos, x_pos;
	double y_vel, x_vel;
	int intact;		// asteroid is removed from screen when intact == 0
} Asteroid, *Asteroids;

typedef enum { TORUS, KLEIN, SPHERE, PROJECTIVE } mode;

// move ship one frame
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
		s->x_vel = -s->x_vel;
	}

	mvaddch((int) s->y_pos, (int) s->x_pos, SHIP_CH);
}

// move missiles one frame and add new missile if user has indicated
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
		// ignore missiles with nonpositive range
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
			cur_missiles[i].x_vel = -cur_missiles[i].x_vel;
		}

		// decrease remaining range
		cur_missiles[i].range -= MISSILE_VEL;

		// update frame
		mvaddch(cur_missiles[i].y_pos, cur_missiles[i].x_pos, MISSILE_CH);
	}
}

// move asteroids and possibly create new one
void move_asteroids(Ship *s, Asteroids cur_asteroids, int y_max, int x_max, mode m) {
	// possibly add new asteroid
	int asteroid_ind = 0;
	double rand_y_pos, rand_x_pos, y_dist, x_dist, angle;
	if (rand() < RAND_MAX * ASTEROID_FREQ) {
		// find empty space in array of asteroids
		while (cur_asteroids[asteroid_ind].intact) {
			asteroid_ind++;
		}

		// get coordinates outside safe zone
		y_dist = 0, x_dist = 0;
		while (y_dist < SAFE_ZONE_RADIUS && x_dist < SAFE_ZONE_RADIUS) {
			rand_y_pos = rand() * (y_max / (double) RAND_MAX);
			rand_x_pos = rand() * (x_max / (double) RAND_MAX);
			y_dist = MIN(MIN(fabs(rand_y_pos - s->y_pos), rand_y_pos + y_max - s->y_pos), s->y_pos + y_max - rand_y_pos);
			x_dist = MIN(MIN(fabs(rand_x_pos - s->x_pos), rand_x_pos + x_max - s->x_pos), s->x_pos + x_max - rand_x_pos);
		}

		// get random initial angle
		angle = rand() * (2 * PI / RAND_MAX);

		// create asteroid
		cur_asteroids[asteroid_ind].y_pos = rand_y_pos;
		cur_asteroids[asteroid_ind].x_pos = rand_x_pos;
		cur_asteroids[asteroid_ind].y_vel = sin(angle) * ASTEROID_VEL;
		cur_asteroids[asteroid_ind].x_vel = cos(angle) * ASTEROID_VEL;
		cur_asteroids[asteroid_ind].intact = 1;
	}

	// move asteroids
	int crossed_y, crossed_x;
	for (int i=0; i<MAX_ASTEROIDS; i++) {
		// ignore destroyed asteroids
		if (!cur_asteroids[i].intact) {
			continue;
		}

		// test for crossing the axes
		crossed_y = (int) floor((cur_asteroids[i].y_pos + cur_asteroids[i].y_vel) / y_max);
		crossed_x = (int) floor((cur_asteroids[i].x_pos + cur_asteroids[i].x_vel) / x_max);

		// move asteroids
		cur_asteroids[i].y_pos = fmod(cur_asteroids[i].y_pos + cur_asteroids[i].y_vel, y_max);
		if (cur_asteroids[i].y_pos < 0) {
			cur_asteroids[i].y_pos += y_max;
		}
		cur_asteroids[i].x_pos = fmod(cur_asteroids[i].x_pos + cur_asteroids[i].x_vel, x_max);
		if (cur_asteroids[i].x_pos < 0) {
			cur_asteroids[i].x_pos += x_max;
		}
	
		// in KLEIN mode, crossing the y axis flips the object across the x axis
		if (m == KLEIN && crossed_y) {
			cur_asteroids[i].x_pos = x_max - cur_asteroids[i].x_pos;
			cur_asteroids[i].x_vel = -cur_asteroids[i].x_vel;
		}

		// update frame
		mvaddch(cur_asteroids[i].y_pos, cur_asteroids[i].x_pos, ASTEROID_CH);
	}
}

// detect missiles colliding with asteroids
// returns number of asteroids destroyed
int destroy_asteroids(Missiles cur_missiles, Asteroids cur_asteroids, int y_max, int x_max, int *hash_map) {
	int num_destroyed = 0;

	// create an array representing every character on screen, initialized to -1
	for (int i=0; i<y_max; i++) {
		for (int j=0; j<x_max; j++) {
			hash_map[x_max * i + j] = -1;
			assert(hash_map[x_max * i + j] != 255);
		}
	}

	// put indices of missiles in entry of hash_map corresponding to their location
	for (int i=0; i<MAX_MISSILES; i++) {
		if (cur_missiles[i].range > 0) {
			hash_map[x_max * ((int) cur_missiles[i].y_pos) + (int) cur_missiles[i].x_pos] = i;
		}
	}

	// check each asteroid for a collision
	int missile_ind;
	for (int i=0; i<MAX_ASTEROIDS; i++) {
		if (!cur_asteroids[i].intact) {
			continue;
		}
		missile_ind = hash_map[x_max * ((int) cur_asteroids[i].y_pos) + (int) cur_asteroids[i].x_pos];
		if (missile_ind > -1) {
			// collision detected, remove asteroid and corresponding missile
			cur_asteroids[i].intact = 0;
			cur_missiles[missile_ind].range = 0;
			num_destroyed++;
		}
	}

	return num_destroyed;
}

// detects whether user has lost
int game_over(Ship *s, Asteroids cur_asteroids) {
	for (int i=0; i<MAX_ASTEROIDS; i++) {
		if (cur_asteroids[i].intact && (int) cur_asteroids[i].y_pos == (int) s->y_pos && (int) cur_asteroids[i].x_pos == (int) s->x_pos) {
			return 1;
		}
	}

	return 0;
}

int main(int argc, char **argv)
{	
	// get terminal dimensions
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

	// create stationary ship in middle of screen
	Ship *s = malloc(sizeof(Ship));
	s->y_pos = w.ws_row / 2, s->x_pos = w.ws_col / 2, s->y_vel = 0, s->x_vel = 0;

	// initialize empty array of missiles
	Missiles cur_missiles = calloc(MAX_MISSILES, sizeof(Missile));

	// initialize empty array of asteroids
	Asteroids cur_asteroids = calloc(MAX_ASTEROIDS, sizeof(Asteroid));

	// initialize array used by destroy_asteroids
	int *hash_map = malloc(w.ws_row * w.ws_col * sizeof(int));

	// set default mode as TORUS
	mode m = TORUS;

	int score = 0;

	// initialize screen
	initscr();		// start screen
	cbreak();		// get keyboard input directly, except signal characters
	halfdelay(1);	// wait .1s for each key press
	noecho();		// do not echo input
	curs_set(0);	// make cursor invisible

	// play game
	char input;
	while (1) {
		input = getch();

		// check if user has quit
		if (input == EXIT_CH) {
			goto finish;
		}

		// check for mode change
		if (input == MODE_CHANGE_CH) {
			m = (m + 1) % 2;
		}

		// draw next frame	
		clear();
		refresh();
		move_ship(s, input, w.ws_row, w.ws_col, m);
		move_missiles(s, cur_missiles, input, w.ws_row, w.ws_col, m);
		move_asteroids(s, cur_asteroids, w.ws_row, w.ws_col, m);
		score += destroy_asteroids(cur_missiles, cur_asteroids, w.ws_row, w.ws_col, hash_map);
		refresh();

		// check for game over
		if (game_over(s, cur_asteroids)) {
			goto finish;
		}
	}

	// close window, print score and exit
	finish:
	endwin();
	printf("SCORE: %d\n", score);
	return 0;
}
