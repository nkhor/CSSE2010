/*
 * project.c
 *
 * Main file for the Snake Project.
 *
 * Author: Peter Sutton. Modified by <Nicole Khor>
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <stdio.h>
#include <stdlib.h>		// For random()

uint8_t seven_seg_conversion[10] = { 63,6,91,79,102,109,125,7,127,111};


#include "ledmatrix.h"
#include "scrolling_char_display.h"
#include "buttons.h"
#include "serialio.h"
#include "terminalio.h"
#include "score.h"
#include "timer0.h"
#include "game.h"
#include "snake.h"
#include "food.h"
// Define the CPU clock speed so we can use library delay functions
#define F_CPU 8000000L
#include <util/delay.h>

//superfood
#define SUPER_COLOUR		COLOUR_LIGHT_ORANGE
PosnType super_posn;
int32_t spawn_time;
int32_t removal_time;
int super_status = 0;
//RATS
int32_t rat_time;
// Function prototypes - these are defined below (after main()) in the order
// given here
void initialise_hardware(void);
void splash_screen(void);
void new_game(void);
void play_game(void);
void handle_game_over(void);
void handle_new_lap(void);
void get_seven_seg(void);
void display(uint8_t number, uint8_t place);

// ASCII code for Escape character
#define ESCAPE_CHAR 27

//Initialise counter
uint32_t resume_time = 0;
uint32_t game_time = 0;
uint32_t buffer_time = 0;
uint32_t scorekeeper;

//JOYSTICK
uint16_t joyst;
uint8_t x_or_y = 0;
	
void display(uint8_t number, uint8_t place) {
		//PORTA = place;
		if (place == 1) {
			PORTA |= (1<<7);
		} else if (place == 0) {
			PORTA &= (0<<7);
		}
		PORTC = seven_seg_conversion[number];	// We assume digit is in range 0 to 9
}

/////////////////////////////// main //////////////////////////////////
int main(void) {
	// Setup hardware and call backs. This will turn on 
	// interrupts.
	initialise_hardware();
	
	// Show the splash screen message. Returns when display
	// is complete
	splash_screen();
	
	while(1) {
		new_game();
		play_game();
		handle_game_over();
	}
}

// initialise_hardware()
//
// Sets up all of the hardware devices and then turns on global interrupts
void initialise_hardware(void) {
	// Set up SPI communication with LED matrix
	ledmatrix_setup();
	
	// Set up pin change interrupts on the push-buttons
	init_button_interrupts();
	
	// Setup serial port for 19200 baud communication with no echo
	// of incoming characters
	init_serial_stdio(19200, 0);
	
	// Set up our main timer to give us an interrupt every millisecond
	init_timer0();
	
	// Turn on global interrupts
	sei();
}

void splash_screen(void) {
	// Reset display attributes and clear terminal screen then output a message
	set_display_attribute(TERM_RESET);
	clear_terminal();
	
	hide_cursor();	// We don't need to see the cursor when we're just doing output
	move_cursor(3,3);
	printf_P(PSTR("Snake"));
	
	move_cursor(3,5);
	set_display_attribute(FG_GREEN);	// Make the text green
	// Modify the following line
	printf_P(PSTR("CSSE2010/7201 Snake Project by Nicole Khor"));	
	set_display_attribute(FG_WHITE);	// Return to default colour (White)
	
	// Output the scrolling message to the LED matrix
	// and wait for a push button to be pushed.
	ledmatrix_clear();
	
	// Red message the first time through
	PixelColour colour = COLOUR_RED;
	while(1) {
		set_scrolling_display_text("Nicole Khor 43953970", colour);
		// Scroll the message until it has scrolled off the 
		// display or a button is pushed. We pause for 130ms between each scroll.
		while(scroll_display()) {
			_delay_ms(130);
			if(button_pushed() != -1) {
				// A button has been pushed
				return;
			}
		}
		// Message has scrolled off the display. Change colour
		// to a random colour and scroll again.
		switch(random()%4) {
			case 0: colour = COLOUR_LIGHT_ORANGE; break;
			case 1: colour = COLOUR_RED; break;
			case 2: colour = COLOUR_YELLOW; break;
			case 3: colour = COLOUR_GREEN; break;
		}
	}
}

void new_game(void) {
	// Clear the serial terminal
	clear_terminal();
	
	// Initialise the game and display
	init_game();
		
	// Initialise the score
	init_score();

	// Delete any pending button pushes or serial input
	empty_button_queue();
	clear_serial_input_buffer();
	
	
	/* Set port C (all pins) to be outputs */
	DDRC = 0xFF;

	/* Set port A, pin 7 to be an output */
	DDRA = (1<<DDRA7);

	/* Set up timer/counter 1 */
	TCCR1A = (0 << COM1A1) | (1 << COM1A0)  // Toggle OC1A on compare match
	| (0 << WGM11) | (0 << WGM10); // Least two significant WGM bits
	TCCR1B = (0 << WGM13) | (1 << WGM12) // Two most significant WGM bits
	| (0 << CS12) | (1 << CS11) | (1 <<CS10); // Divide clock by 64
	
	DDRD = (1<<5);
	

	OCR1A = 14;
	
}

void play_game(void) {
	/*joystick*/
	ADMUX = (1<<REFS0);
	ADCSRA = (1<<ADEN)|(1<<ADPS2)|(1<<ADPS1);

	uint32_t last_move_time;
	int8_t button;
	char serial_input, escape_sequence_char;
	uint8_t characters_into_escape_sequence = 0;
	
	// Record the last time the snake moved as the current time -
	// this ensures we don't move the snake immediately.
	last_move_time = get_clock_ticks();
	game_time = get_clock_ticks();
	
	// We play the game forever. If the game is over, we will break out of
	// this loop. The loop checks for events (button pushes, serial input etc.)
	// and on a regular basis will move the snake forward.
	uint8_t place = 0;
	while(1) {
		
		/*SEVEN SEG DISPLAY*/
		uint8_t value; //actual value to place and display

			//printf("\%u", digit);
			/* Change the digit flag for next time. if 0 becomes 1, if 1 becomes 0. */
			if (place == 0) {
				value = get_snake_length() % 10;
				display(value, place);
				place = 1;

			} else if (place == 1) {
				value = (get_snake_length() / 10) % 10;
					if (value != 0){
					display(value, place);
					}
				place = 0;
			//	printf("changed digit to \n");
			}
	
			while ((TIFR1 & (1 << OCF1A)) == 0) {
		 		; 
			}
			TIFR1 &= (1 << OCF1A);
		
		/*SERIAL INPUTS*/
			// Check for input - which could be a button push or serial input.
			// Serial input may be part of an escape sequence, e.g. ESC [ D
			// is a left cursor key press. We will be processing each character
			// independently and can't do anything until we get the third character.
			// At most one of the following three variables will be set to a value 
			// other than -1 if input is available.
			// (We don't initalise button to -1 since button_pushed() will return -1
			// if no button pushes are waiting to be returned.)
			// Button pushes take priority over serial input. If there are both then
			// we'll retrieve the serial input the next time through this loop
			serial_input = -1;
			escape_sequence_char = -1;
			button = button_pushed();
		
			move_cursor(3,8);
			if (scorekeeper != get_score()){
				set_display_attribute(FG_WHITE); printf("Score: [%8lu]\n", get_score()); //%d warning
			}
			scorekeeper = get_score();
		
			if(button == -1) {
				// No push button was pushed, see if there is any serial input
				if(serial_input_available()) {
					// Serial data was available - read the data from standard input
					serial_input = fgetc(stdin);
					// Check if the character is part of an escape sequence
					if(characters_into_escape_sequence == 0 && serial_input == ESCAPE_CHAR) {
						// We've hit the first character in an escape sequence (escape)
						characters_into_escape_sequence++;
						serial_input = -1; // Don't further process this character
					} else if(characters_into_escape_sequence == 1 && serial_input == '[') {
						// We've hit the second character in an escape sequence
						characters_into_escape_sequence++;
						serial_input = -1; // Don't further process this character
					} else if(characters_into_escape_sequence == 2) {
						// Third (and last) character in the escape sequence
						escape_sequence_char = serial_input;
						serial_input = -1;  // Don't further process this character - we
											// deal with it as part of the escape sequence
						characters_into_escape_sequence = 0;
					} else {
						// Character was not part of an escape sequence (or we received
						// an invalid second character in the sequence). We'll process 
						// the data in the serial_input variable.
						characters_into_escape_sequence = 0;
					}
				}
			}
		
			// Process the input. 
		if(400<joyst && joyst <600) {
			if(button==0 || escape_sequence_char=='C') {
				// Set next direction to be moved to be right.
				if (!get_paused()) {set_snake_dirn(SNAKE_RIGHT);}
			} else  if (button==2 || escape_sequence_char == 'A') {
				// Set next direction to be moved to be up
				if (!get_paused()) {set_snake_dirn(SNAKE_UP);}
			} else if(button==3 || escape_sequence_char=='D') {
				// Set next direction to be moved to be left
				if (!get_paused()) {set_snake_dirn(SNAKE_LEFT);}
			} else if (button==1 || escape_sequence_char == 'B') {
				// Set next direction to be moved to be down
				if (!get_paused()) {set_snake_dirn(SNAKE_DOWN);}
			} else if (serial_input == 'n' || serial_input == 'N') { //new game
				new_game(); 
			} else if(serial_input == 'p' || serial_input == 'P') {
				pause_time();
			}
		}
				if(x_or_y == 0) {
					ADMUX &= ~1;
				} else {
					ADMUX |= 1;
				}
			// Check for timer related events here - THIS PAUSES THE GAME
				if(get_clock_ticks() >= (last_move_time + 600 - get_speedkeeper())) {
					// 600ms (0.6 second) has passed since the last time we moved the snake,
					// so move it now
					// Start the ADC conversion
					ADCSRA |= (1<<ADSC);
					if(x_or_y == 0) {
						ADMUX &= ~1;
						} else {
						ADMUX |= 1;
					}
					while(ADCSRA & (1<<ADSC)) {
						; /* Wait until conversion finished */
					}
					joyst = ADC; // read the value
					//value = read;
					if (x_or_y ==0) {//x
						if (joyst <200) {
							set_snake_dirn(SNAKE_DOWN);
						} else if (joyst > 900) {
							set_snake_dirn(SNAKE_UP);
						} 
					//	printf("\n X: %4d", joyst);
					} else if (x_or_y == 1) { //y
						if (joyst < 200) {
							set_snake_dirn(SNAKE_RIGHT);
						} else if (joyst > 900) {
							set_snake_dirn(SNAKE_LEFT);
						}
					//	printf("\n \t \tY: %4d", joyst);
					}
					
					// Next time through the loop, do the other direction
					x_or_y ^= 1;
					if(!attempt_to_move_snake_forward()) {
						// Move attempt failed - game over
						break;
					}
					last_move_time = get_clock_ticks();
				}
				if((get_clock_ticks() >= removal_time + 15000) && (super_status == 0)) {
					super_posn = add_food_item();
					store_super_posn(super_posn);
						
					if(is_position_valid(super_posn)) {
						ledmatrix_update_pixel(x_position(super_posn), y_position(super_posn), SUPER_COLOUR);
						store_super_posn(super_posn);
						spawn_time = get_clock_ticks();
					//	printf("addition of super");
						super_status = 1;
					}
				}
				if (get_snake_head_position() == return_stored_super()) {
						super_status =0; //if snake was eaten, need to toggle super status
						removal_time = get_clock_ticks();
						store_super_posn(0);
				} else if (get_clock_ticks() >= spawn_time + 5000 && super_status == 1) {
					if (is_food_at(super_posn)) {
						remove_food(food_at(super_posn));
						ledmatrix_update_pixel(x_position(super_posn), y_position(super_posn), COLOUR_BLACK);
					//	printf("removal of super");
						removal_time = get_clock_ticks();
						store_super_posn(0);
						super_status = 0;
					}	
				}
				if (get_clock_ticks() > rat_time + 1500) {
					move_rat();
					rat_time = get_clock_ticks();
				}

		}
}	// If we get here the game is over. 

void handle_game_over() {
	move_cursor(10,14);
	// Print a message to the terminal. 
	printf_P(PSTR("GAME OVER"));
	move_cursor(10,15);
	printf_P(PSTR("Press a button to start again"));
	display(0,0);
	reset_speedkeeper();
	while(button_pushed() == -1) {
		; // wait until a button has been pushed
	}
}

