/******************************HEADER**************************/
#ifndef __TICTACTOE_H__
#define __TICTACTOE_H__

#define XTURN 0;
#define OTURN 1;

struct Point
{
  int y;
  int x;
};

static const int FULL_BOARD = 1<<0 | 1<<1 | 1<<2 | 1<<3 | 1<<4 | 
                  1<<5 | 1<<6 | 1<<7 | 1<<8;

static char turnChars[] = {'X','O'};

#define NUM_WIN_PATTERNS 8
static unsigned int winPatterns[] = {
                                 1<<0 | 1<<1 | 1 <<2, //top row
                                 1<<3 | 1<<4 | 1 <<5, //middle row
                                 1<<6 | 1<<7 | 1 <<8, //bottom row
								 
								 1<<0 | 1<<3 | 1<<6, //left column
								 1<<1 | 1<<4 | 1<<7, //center column
								 1<<2 | 1<<5 | 1<<8, //right column
								 
								 1<<0 | 1<<4 | 1<<8, //diagnol 
								 1<<6 | 1<<4 | 1<<2, //diagnol
							   };
								 


int CheckForWinner(unsigned int _xSelections, unsigned int _oSelections);
/******************************HEADER**************************/
#endif
