/*
 * score.c
 *
 * Written by Peter Sutton
 */

#include "score.h"

// Variable to keep track of the score. We declare it as static
// to ensure that it is only visible within this module - other
// modules should call the functions below to modify/access the
// variable.
static uint32_t score;
uint32_t speedkeeper;

void init_score(void) {
	score = 0;
}

uint32_t get_speedkeeper(void) {
	return speedkeeper;
}

void reset_speedkeeper(void) {
	speedkeeper = 0;
}

void add_to_score(uint16_t value) {
	score += value;
	if (speedkeeper < 450) {
		speedkeeper = speedkeeper + value*1.5;
	}
}

uint32_t get_score(void) {
	return score;
}
