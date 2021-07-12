#include "TicTacToe.h"

int CheckForWinner(unsigned int _xSelections, unsigned int _oSelections)
{
  int x;
  if ((_oSelections | _xSelections) == FULL_BOARD)
  {
    return 2;
  }
  for (x = 0; x< NUM_WIN_PATTERNS; x++)
  {
    if ((winPatterns[x] & _xSelections) == winPatterns[x])
	{
	  return 0;
	}
    else if ((winPatterns[x] & _oSelections) == winPatterns[x])
	{
	  return 1;
	}
  }
  return -1;
}
