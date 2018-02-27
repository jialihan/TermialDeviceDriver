/*
 * Test for reading on single separate terminal  
 */

#include <stdio.h>
#include <stdlib.h>
#include <threads.h>
#include <terminals.h>

void writer1(void *);
void writer2(void *);


char string1[] = "jjjjjjjjhhhhhhhh\n";
int length1 = sizeof(string1) - 1;


char string2[] = "aaaaaaaabbbbbbb\n";
int length2 = sizeof(string2) - 1;


int
main(int argc, char **argv)
{
    InitTerminalDriver();
    int a = InitTerminal(0);
    int b = InitTerminal(1);
    int c = InitTerminal(2);
    int d = InitTerminal(3);

    printf("%d %d %d %d\n", a, b, c, d);
    fflush(stdout);


    if (argc > 1) HardwareOutputSpeed(1, atoi(argv[1]));
    if (argc > 2) HardwareInputSpeed(1, atoi(argv[2]));


    ThreadCreate(writer1, NULL);
    ThreadCreate(writer2, NULL);

    ThreadWaitAll();

    exit(0);
}

void
writer1(void *arg)  // write and read in same terminal 1
{
    int  r_status,w_status;
    char buf1[20];

    w_status = WriteTerminal( 1, string1, length1);
    r_status = ReadTerminal(1 , buf1, 5000);
    
    printf("write_status is %d reading status is %d \n", w_status,r_status);
    fflush(stdout);
}

void
writer2(void *arg)   // write and read in same terminal 2
{
    int  r_status,w_status;
    char buf1[20];

    w_status = WriteTerminal(3, string2, length2);
    r_status = ReadTerminal(3, buf1, 5000);
    
    printf("write_status is %d reading status is %d \n", w_status,r_status);
    fflush(stdout);
}
