/* hw4-main.c */

/* bash$ gcc -Wall -Werror -o hw4.out hw4-main.c hw4.c */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* global variables */
int game_token;
int total_wins;
int total_losses;
char ** words;

/* write the wordle_server() function and place all of your code in hw4.c */
int wordle_server( int argc, char ** argv );

int main( int argc, char ** argv )
{
  game_token = total_wins = total_losses = 0;
  words = calloc( 1, sizeof( char * ) );
  if ( words == NULL ) { perror( "calloc() failed" ); return EXIT_FAILURE; }

  int rc = wordle_server( argc, argv );

  /* on Submitty, there will be more code here that validates the
   *  global variables when your wordle_server() function returns...
   */

  printf( "\ngames: %d\nwins: %d\nlosses: %d\n", game_token, total_wins, total_losses );
  printf( "\nWord(s) played:" );

  /* display and deallocate memory for the list of words played */
  for ( char ** ptr = words ; *ptr ; ptr++ )
  {
    printf( " - %s", *ptr );
    free( *ptr );
  }
  printf( "\n" );
  free( words );

  return rc;
}