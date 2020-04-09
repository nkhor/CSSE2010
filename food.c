/*
** food.c
**
** Written by Peter Sutton
**
*/
#include <stdlib.h>
#include <stdio.h>
#include "position.h"
#include "food.h"
#include "snake.h"
#include "board.h"
#include "timer0.h"
#include "ledmatrix.h"

/*
** Global variables.
** foodPositions records the (x,y) positions of each food item.
** numFoodItems records the number of items. The index into the
** array is the food ID. Food IDs will range between 0 and 
** numFoodItems-1. If we delete a food item, we rearrange the 
** array.
**
*/
PosnType foodPositions[MAX_FOOD];
int8_t numFoodItems;
PosnType stored_super;		//position of superfood
//RATS
PosnType rat_posn;
int8_t rat_id;

/* 
** Initialise food details.
*/

void init_food(void) {
	numFoodItems = 0; 
}

/* Returns true if there is food at the given position, false (0) otherwise.
*/
uint8_t is_food_at(PosnType posn) {
	return (food_at(posn) != -1);
}

/* Returns a food ID if there is food at the given position,
** otherwise returns -1
*/
int8_t food_at(PosnType posn) {
    int8_t id;
	// Iterate over all the food items and see if the position matches
    for(id=0; id < numFoodItems; id++) {
        if(foodPositions[id] == posn) {
            // Food found at this position 
            return id;
        }
    }
    // No food found at the given position.
    return -1;
}

/* Add a food item and return the position - or 
** INVALID_POSITION if we can't.
*/
PosnType add_food_item(void) {
	if(numFoodItems >= MAX_FOOD) {
		// Can't fit any more food items in our list
		return INVALID_POSITION;
	}
	/* Generate "random" positions until we get one which
	** is not occupied by a snake or food.
	*/
	int8_t x, y, attempts, i;
	PosnType test_position;
	x = 0;
	y = 0;
	attempts = 0;
	i = 0;
	do {
		// Generate a new position - this is based on a sequence rather
		// then being random
		switch(random()%4) {
			case 0: i = get_clock_ticks()/5; break;
			case 1: i =	get_clock_ticks()/1; break;
			case 2: i = get_clock_ticks()/2; break;
			case 3: i = get_clock_ticks()/4; break;
		}
        x = (x+3+attempts+i)%BOARD_WIDTH;
        y = (y+5+i)%BOARD_HEIGHT;
		test_position = position(x,y);
        attempts++;
    } while(attempts < 100 && 
                (is_snake_at(test_position) || is_food_at(test_position)));
        
    if(attempts >= 100) {
        /* We tried 100 times to generate a position
        ** but they were all occupied.
        */
		printf("\n no room for more food \n");
        return INVALID_POSITION;
    }
	
	// If we get here, we've found an unoccupied position (test_position)
	// Add it to our list, display it, and return its ID.
	int8_t newFoodID = numFoodItems;
	foodPositions[newFoodID] = test_position;
	
	numFoodItems++;
	return test_position;
}

/* Return the position of the given food item. The ID is assumed
** to be valid.
*/
PosnType get_position_of_food(int8_t foodID) {
	return foodPositions[foodID];
}

/*
** Remove the food item from our list of food
*/
void remove_food(int8_t foodID) {
    int8_t i;
        
    if(foodID < 0 || foodID >= numFoodItems) {
        /* Invalid foodID */
        return;
    }
	     
    /* Shuffle our list of food items along so there are
	** no holes in our list 
	*/
    for(i=foodID+1; i <numFoodItems; i++) {
        foodPositions[i-1] = foodPositions[i];
    }
    numFoodItems--;
}

void store_super_posn(PosnType posn) {
	stored_super = posn;
}

PosnType return_stored_super(void) {
	return stored_super;
}

void store_rat_posn(PosnType posn) {
	rat_posn = posn;
}

PosnType return_rat_posn(void) {
	return rat_posn;
}

PosnType move_rat(void){
	PosnType prevPosition = foodPositions[food_at(rat_posn)];
	int8_t ratX= 1; int8_t ratY = 1; int8_t xdirn = 0; int8_t ydirn =0; int8_t attempts = 0;
	PosnType newRatPosn;
	//printf("tries to move rat");
	//Randomise
	do{
		switch (random()%4) {
			case 0: xdirn = 1; ydirn = 0;break;
			case 1: xdirn = -1; ydirn = 0; break;
			case 2: ydirn = 1; xdirn = 0; break;
			case 3: ydirn = -1; xdirn = 0; break;
		}
		
		if (y_position(rat_posn) == 0b000) {
			ratY = 0b001;
		} else if(y_position(rat_posn) == 0b111) {
			ratY = 0b110;
		} else{
			ratY = y_position(rat_posn) + ydirn;
		}
		if (x_position(rat_posn) == 0b1111) {
			ratX = 0b1110;
		} else if (x_position(rat_posn) == 0b0000) {
			ratX = 0b0001;
		} else {
			ratX = x_position(rat_posn) + xdirn;
		}
		newRatPosn = position(ratX,ratY);
		attempts++;
	} while (attempts < 100);// && !is_position_valid(newRatPosn));
	//if here, random position has been selected but may not be valid
	//checking boundaries
	//ledmatrix_update_pixel(0b1111, 0b111, 0x0F);//remove led rat
	if (!is_position_valid(newRatPosn)) {
		printf("invalid working");
		return prevPosition;
	}
	//if not taken up by snake or other food
	else {
		if (!is_food_at(newRatPosn) && !is_snake_at(newRatPosn) && newRatPosn != return_stored_super()) {
			foodPositions[food_at(rat_posn)] = newRatPosn; //replace the old position with the new position
			store_rat_posn(newRatPosn); //store new position
		
			if (get_snake_head_position() != prevPosition) { //if havent been eaten by snake
				ledmatrix_update_pixel(x_position(prevPosition), y_position(prevPosition), 0x00);//remove led rat
			}
			ledmatrix_update_pixel(x_position(newRatPosn), y_position(newRatPosn), 0x21);// update display
		return newRatPosn; //return new position 
		}
		return prevPosition;
	}
}