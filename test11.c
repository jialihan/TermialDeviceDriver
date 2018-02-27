/* 
 * Tests 
 * 1. error initiate a terminal with -1
 * 2. write to a terminal that never exists
 * 3. read the statistics of terminal i/o info
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <threads.h>
#include <hardware.h>
#include <terminals.h>

void writer(void *);
void getIOstatistics(void *);

char string1[] = "aaaaaaaaaaaaaaaaaaaaa\n";
int length1 = sizeof(string1) - 1;

struct termstat stats[NUM_TERMINALS];

int
main(int argc, char **argv)
{
    InitTerminalDriver();
    int a = InitTerminal(0);
    int b = InitTerminal(1);
    int c = InitTerminal(2);
    int d = InitTerminal(3);
    int error = InitTerminal(-1);

    printf("%d %d %d %d error terminal: %d \n", a, b, c, d, error);
    fflush(stdout);


    if (argc > 1) HardwareOutputSpeed(1, atoi(argv[1]));
    if (argc > 2) HardwareInputSpeed(1, atoi(argv[2]));


    ThreadCreate(writer, NULL);

    ThreadWaitAll();

    exit(0);
}

void
getStatistics(void *arg)
{
    int i;
    while (1) {
        sleep(1);
        TerminalDriverStatistics(stats);
        printf("Open terminal statistics: \n");
        fflush(stdout);
        for (i = 0; i < 4; i++) {  
            // record the opening 4 terminals
            printf("term %d = {tty_in: %d, tty_out: %d, user_in: %d, user_out: %d}\n", i, stats[i].tty_in, stats[i].tty_out ,stats[i].user_in, stats[i].user_out);
            fflush(stdout);
        }
    }

}


void
writer(void *arg)  //write to a terminal that never exists
{
    int w_status;
    w_status = WriteTerminal(50, string1, length1);

    
    printf("write_status is %d \n", w_status);
    fflush(stdout);
}


