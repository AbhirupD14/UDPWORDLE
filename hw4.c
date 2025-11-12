#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define MAXBUFFER 128

extern int game_token;
extern int total_wins;
extern int total_losses;
extern char **words;

volatile sig_atomic_t shutdown_flag = 0;
int sd = -1;

typedef struct {
    int token;
    short guesses_left;
    char * hidden_word;
    char valid_guess;
    int active;
    // char * result;
}Game;

void terminate(int sig)
{
    shutdown_flag = 1;
    if( sd != -1 )
    {
        close( sd );
    }
    printf("SIGUSR1 received; Wordle server shutting down...\n");
}

void downcase(char *str) {
    for (char *p = str; *p; p++) {
        *p = tolower(*p);
    }
}

void uppercase(char * str)
{
    for (char *p = str; *p; p++) {
        *p = toupper(*p);
    }
}

// Cleanup dynamic memory in each game. Result field is freed in validate guess
void cleanup(Game *all_games)
{
    for (int i = 0; i < game_token; i++) {
        if( (all_games+i)->active == 1 )
        {
        
            printf("GAME %d: Game over; word was %s!\n", (all_games+i)->token, (all_games+i)->hidden_word);
            
        }
            free((all_games+i)->hidden_word);
    }
    free(all_games);    
}

int word_in_file(char *path, char *guess) {
    char *word = calloc(6, sizeof(char));  // 5 letters + newline
    if (word == NULL) {
        fprintf(stderr, "ERROR: Memory allocation failed\n");
        return 0;
    }

    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "ERROR: open() failed\n");
        free(word);
        return 0;
    }

    ssize_t n;
    while ((n = read(fd, word, 6)) == 6) {
        if (*(word + 5) != '\n') {
            continue;  // skip malformed line
        }

        // Compare 5 characters using pointers
        char *a = word;
        char *b = guess;
        int match = 1;
        while (a < word + 5) {
            if (*a != *b) {
                match = 0;
                break;
            }
            a++;
            b++;
        }

        if (match) {
            close(fd);
            free(word);
            return 1;
        }
    }

    close(fd);
    free(word);
    return 0;
}


char * validate_guess( Game * game, char * guess, char * path )
{
    /*
        Response in form: game_token | valid_guess | guesses_left | result
    */    

    char * response = calloc( 12, sizeof( char ) );
    char valid_guess = 'N';
    char * result = calloc( 6, sizeof( char ) );

    // Invalid guess
    if( !word_in_file(path, guess) )
    {
        char *  temp = "?????";
        strncpy( result, temp, 5 );
        if( game->guesses_left != 1) printf("GAME %d: Invalid guess; sending reply: %s (%d guesses left)\n", game->token, temp, game->guesses_left);
        else printf("GAME %d: Invalid guess; sending reply: %s (%d guess left)\n", game->token, temp, game->guesses_left);
        int token_n = htonl( game-> token );
        short guesses_left = htons( game -> guesses_left );
        memcpy( response, &token_n, 4 );
        *(response+4) = valid_guess;
        memcpy( response+5, &guesses_left, 2 );
        memcpy( response+7, result, 5 );
        free(result);
        return response;
    }

    // Valid guess
    else
    {
        game->guesses_left--;
        valid_guess = 'Y';
        
        // Check if the guess is the correct word
        // This should handle getting the word correct on your last try
        if( strcmp( guess, game->hidden_word ) == 0)
        {
            strncpy( result, game->hidden_word, 5);
            uppercase(result);
            if( game->guesses_left != 1) printf("GAME %d: Sending reply: %s (%d guesses left)\n", game->token, result, game->guesses_left);
            else printf("GAME %d: Sending reply: %s (%d guess left)\n", game->token, result, game->guesses_left);
            printf("GAME %d: Game over; word was %s!\n", game->token, game->hidden_word);
            total_wins++;
            game->active=0;
            int token_n = htonl( game-> token );
            short guesses_left = htons( game -> guesses_left );
            memcpy( response, &token_n, 4 );
            *(response+4) = valid_guess;
            memcpy( response+5, &guesses_left, 2 );
            memcpy( response+7, result, 5 );
            free(result);
            return response;
            // game->token = -1;
        }

        // Logic for matching letters to positions
        else
        {
            int * used = calloc( 5, sizeof(int) );
            // Start off with all letters as incorrect/not in word letters
            strcpy(result, "-----");

            for (int i = 0; i < 5; ++i) {
                char a = *(guess + i);
                char b = *(game->hidden_word + i);
        
                if (a == b) {
                    *(result + i) = toupper(a);
                    *(used+i) = 1;
                }
            }

            for (int i = 0; i < 5; ++i) {
                if (isupper(*(result + i))) continue; // already matched

                char a = *(guess + i);

                for (int j = 0; j < 5; ++j) {
                    if (*(used+j)) continue;

                    char b = *(game->hidden_word + j);
                    if (a == b) {
                        *(result + i) = a; // lowercase
                        *(used+j) = 1;
                        break;
                    }
                }
            }
        }

    }

    // Construct the server response
    if( game->token > 0)
    {
         if(game->guesses_left != 1) printf("GAME %d: Sending reply: %s (%d guesses left)\n", game->token, result, game->guesses_left);
         else printf("GAME %d: Sending reply: %s (%d guess left)\n", game->token, result, game->guesses_left);

         // Handles printing for the case where you run out of guesses and the game is over
         if( game->guesses_left == 0)
         {
             printf("GAME %d: Game over; word was %s!\n", game->token, game->hidden_word);
             game->active=0;
             total_losses++;
         }
    }

    int token_n = htonl( game-> token );
    short guesses_left = htons( game -> guesses_left );
    memcpy( response, &token_n, 4 );
    *(response+4) = valid_guess;
    memcpy( response+5, &guesses_left, 2 );
    memcpy( response+7, result, 5 );
    free(result);
    return response;
}

int wordle_server(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    signal(SIGINT, SIG_IGN);   // Ignore Ctrl+C
    signal(SIGTERM, SIG_IGN);  // Ignore kill <pid>
    signal(SIGUSR2, SIG_IGN);  // Ignore custom user signal 2
    signal(SIGUSR1, terminate);

    if (argc != 5)
    {
        fprintf(stderr, "ERROR: Invalid argument(s)\nUSAGE: ./hw4.out <UDP-server-port> <word-file-path> <num-words> <seed>\n");
        return EXIT_FAILURE;
    }

    int port = atoi(*(argv + 1));
    char * word_path = *(argv+2);
    int num_words = atoi(*(argv+3));
    int seed = atoi(*(argv+4));

    if (port < 0 || num_words < 0 || seed < 0)
    {
        fprintf(stderr, "ERROR: One or more arguments were negative!\n");
        return EXIT_FAILURE;
    }

    srand(seed);

    int fd = open(word_path, O_RDONLY);
    if (fd == -1)
    {
        fprintf(stderr, "ERROR: open() failed");
        return EXIT_FAILURE;
    }
    printf("Opened %s; read %d valid words\n", word_path, num_words);
    off_t file_size = lseek(fd, 0, SEEK_END);

    // Reset file pointer for future reads
    lseek(fd, 0, SEEK_SET); 

    if ( (file_size/6) != num_words )
    {
        fprintf(stderr, "ERROR: Number of words in file does not match number of words provided!\n");
        close(fd);
        return EXIT_FAILURE;
    }

    /*------------------------
    |    SERVER SETUP         |
    ------------------------*/


    sd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sd == -1)
    {
        fprintf(stderr, "socket() failed");
        return EXIT_FAILURE;
    }

    struct sockaddr_in udp_server;
    int length = sizeof(udp_server);

    udp_server.sin_family = AF_INET;
    udp_server.sin_addr.s_addr = htonl(INADDR_ANY);
    udp_server.sin_port = htons(port);

    if (bind(sd, (struct sockaddr *)&udp_server, length) == -1)
    {
        fprintf(stderr, "bind() failed");
        close(sd);
        return EXIT_FAILURE;
    }

    if (getsockname(sd, (struct sockaddr *)&udp_server, (socklen_t *)&length) == -1)
    {
        fprintf(stderr, "getsockname() failed");
        close(sd);
        return EXIT_FAILURE;
    }

    printf("Wordle UDP server started\n");

    Game * all_games = calloc(1, sizeof( Game ) );
    // int total_guesses = 0;
    /*------------------------
    |  APPLICATION PROTOCOL   |
    ------------------------*/
   
    while (!shutdown_flag)
    {
        char * guess = calloc(MAXBUFFER, sizeof( char ) );
        
        if (!guess)
        {
            fprintf(stderr, "ERROR: calloc() failed");
            return EXIT_FAILURE;
        }

        struct sockaddr_in remote_client;
        socklen_t addrlen = sizeof(remote_client);

        int n = recvfrom(sd, guess, MAXBUFFER, 0, (struct sockaddr *)&remote_client, &addrlen);

        // Null-terminate the received guess part (after game token)
        *(guess+n) = '\0';
        
        /*------------------------
        |     GAME LOGIC         |
        ------------------------*/

        if( n == 3 && strcmp(guess, "NEW") == 0 )
        {
            game_token++;
            printf("New game request; assigned game token %d\n", game_token);

            int n = rand() % num_words;
            // printf("Random word index: %d\n", n);

            // File might have been closed previously 
            close( fd );
            fd = open(word_path, O_RDONLY);
            lseek(fd, (n * 6), SEEK_SET);
        
            char *hidden_word = calloc(6, sizeof(char));

            if (!hidden_word)
            {
                fprintf(stderr, "ERROR: calloc() failed");
                return EXIT_FAILURE;
            }
            

            if (read(fd, hidden_word, 6) == -1)
            {
                fprintf(stderr, "ERROR: read() failed");
                free(hidden_word);
                close(fd);
                return EXIT_FAILURE;
            }
        
            *(hidden_word + 5) = '\0'; // Null-terminate

            words = realloc(words, sizeof(char *) * (game_token + 1)); // Reallocate to be the size of the number of games plus one for the null entry at the end

            *(words+game_token-1) = calloc( 6, sizeof( char ) );
            strncpy(*(words + game_token-1), hidden_word, 5);
            *(*(words + game_token-1) + 5) = '\0';
            * (words + game_token) = NULL;

            Game new_game;
            new_game.token = game_token;
            new_game.guesses_left = 6;
            new_game.hidden_word = hidden_word;
            new_game.active=1;

            all_games = realloc(all_games, game_token * sizeof(Game));

            *(all_games+game_token-1) = new_game;

            int response = htonl(game_token);
            int sent = sendto(sd, &response, 4, 0, (struct sockaddr *)&remote_client, addrlen);
            
            if (sent == -1)
            {
                fprintf(stderr, "sendto() failed");
                return EXIT_FAILURE;
            }
        }   

        else if ( n == 9 )
        {
            int target_game = ntohl(*(int *)guess);
            // The next 5 bytes will be the guess, which is located after the token
            char * received_guess = guess+4;
            downcase( received_guess );
            printf("GAME %d: Received guess: %s\n", target_game, received_guess);
            if( target_game > game_token || target_game < 0) continue;
            Game * current_game = all_games + target_game - 1;
            
            if( current_game->token == -1 || target_game > game_token )
            {
                continue;
            }

            close(fd); // Closing this because I reopen it to check for valid words in validate_guess
            if( current_game->active == 0) 
            {
                fd = open(word_path, O_RDONLY); 
                continue;
            }

            char * response = validate_guess( current_game, received_guess, word_path );
            int sent = sendto(sd, response, 12, 0, (struct sockaddr *)&remote_client, addrlen);
            
            if (sent == -1)
            {
                fprintf(stderr, "sendto() failed");
                return EXIT_FAILURE;
            }

            free( response );
        }

        free(guess);
    }
    // cleanup logic
    close(sd);
    cleanup( all_games );
    close(fd);

    return EXIT_SUCCESS;
}