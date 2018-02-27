#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <threads.h>
#include <hardware.h>
#include <terminals.h>

/* Constants */
#define ECHO_BUF_SIZE 1024 
#define INPUT_BUF_SIZE 1024
#define OUTPUT_BUF_SIZE 1024 

/* Terminal Init Status */
#define ACTIVE   1
#define CLOSED   0

/* ECHO STATUS*/
#define BUSY    1
#define IDLE    0


/* Condition variables to lock */
static cond_id_t writers[NUM_TERMINALS];
static cond_id_t busy_write[NUM_TERMINALS];
static cond_id_t readers[NUM_TERMINALS];
static cond_id_t readable[NUM_TERMINALS];

/* Terminals */
char input_buf[NUM_TERMINALS][INPUT_BUF_SIZE];
char echo_buf[NUM_TERMINALS][ECHO_BUF_SIZE]; // Must be at least 3
char output_buf[NUM_TERMINALS][OUTPUT_BUF_SIZE];
int term_state[NUM_TERMINALS]; // ACTIVE or CLOSED
struct termstat statistics[NUM_TERMINALS];

/* Read */
int num_readers[NUM_TERMINALS];
int num_readable[NUM_TERMINALS]; // number of the input that are readable
int input_buf_write_index[NUM_TERMINALS];
int input_buf_read_index[NUM_TERMINALS];
int input_buf_count[NUM_TERMINALS]; // number of chars in input_buf 

/* Write */
int num_writers[NUM_TERMINALS];
int output_buf_read_index[NUM_TERMINALS];
int output_buf_write_index[NUM_TERMINALS];
int output_buf_counter[NUM_TERMINALS];
int output_buf_len[NUM_TERMINALS]; //length of the output_buffer
bool is_newline[NUM_TERMINALS];

/* echo */
static cond_id_t busy_echo[NUM_TERMINALS];
int echo_buf_count[NUM_TERMINALS];  // number of chars have been echoed
int screen_len[NUM_TERMINALS];      // number of the visible number of chars on screen 
int echo_buf_write_index[NUM_TERMINALS];
int echo_buf_read_index[NUM_TERMINALS];
int  echo_status[NUM_TERMINALS];    // BUSY or IDLE
bool is_first_backspace[NUM_TERMINALS];


/**********************************/
/********* Helper Methods *********/
/**********************************/
extern
void begin_write(int term)
{
	/* 
	 * Check if other user thread is writing */
	if (num_writers[term] > 0) {
		CondWait(writers[term]);
	}
	num_writers[term]++;

	/* Wait until echo is done */
	if (echo_status[term] == BUSY)
		CondWait(busy_echo[term]);

	return;
}

extern
void end_write(int term)
{
	/* Wait until one char writing is done */
	CondWait(busy_write[term]);
	num_writers[term]--;
	CondSignal(writers[term]);

    return;
}

extern
void begin_read(int term)
{
	/* 
	 * Check other user thread is reading on the same terminal.
	 */
	if (num_readers[term] > 0) {
		CondWait(readers[term]);
	}
	num_readers[term]++;

	/* Wait until there exists something to read */
	if (num_readable[term] == 0)
		CondWait(readable[term]);
	num_readable[term]--;
    
    return;
}

extern
void end_read(int term)
{
	num_readers[term]--;
	CondSignal(readers[term]);

	return;
}

extern
void copy_to_output_buf(int term, char *buf, int buflen)
{
	 /* Write to out_put buf, may excceed, reuse to chunk the chars use mod */
	 if(output_buf_write_index[term]+buflen > OUTPUT_BUF_SIZE)
	 {
	 	 int write_available =OUTPUT_BUF_SIZE - output_buf_len[term]%OUTPUT_BUF_SIZE;
	 	 if(output_buf_len[term] == OUTPUT_BUF_SIZE)  // output_buf_len = SIZE or 0 have same value;
	 	 {
	 	 	write_available = 0;
	 	 }
	 	 memcpy(output_buf[term]+ output_buf_len[term]%OUTPUT_BUF_SIZE, buf, write_available);
	 	 int remaining = buflen - write_available;
	 	 memcpy(output_buf[term], buf+write_available, remaining);
	 }
	 else
	 {
	 	memcpy(output_buf[term]+ output_buf_len[term]%OUTPUT_BUF_SIZE, buf, buflen);
	 }
	 output_buf_len[term] += buflen;  // have to store all output char
	 output_buf_counter[term] += buflen;  //have to store all output cha
     output_buf_write_index[term] = output_buf_len[term] % OUTPUT_BUF_SIZE;
}


extern
void write_firstchar_to_terminal(int term)
{
	// begin with write the first char
	char begin_char = output_buf[term][output_buf_read_index[term]% OUTPUT_BUF_SIZE];
	if(begin_char == '\n')
	{
		statistics[term].user_in--;  // invalid beginning to write
	    WriteDataRegister(term,'\r');  // write the first '\r'
	    output_buf_counter[term]++;
	    is_newline[term] = false;  // write the second '\n';
	}
	else
	{
	    WriteDataRegister(term, begin_char);
	    output_buf_read_index[term]= (output_buf_read_index[term]+1) % OUTPUT_BUF_SIZE;
	    //printf("next output_read_intex is %d \n", output_buf_read_index[term]);  // debug
		is_newline[term] = true;
	}

	return;
}



extern
void input_buf_char_processing(int term, char c)
{
	/* update input buffer */
	 c = (c== '\r') ? '\n' : c;  // change the char in input_buf
     if(c == '\b' || c == '\177')
   	 {
   	 	// if not empty() || not the first new letter
   	 	if( input_buf_write_index[term] >0 && (input_buf[term][(input_buf_write_index[term]-1 + INPUT_BUF_SIZE)%INPUT_BUF_SIZE] != '\n'))  
 		{
 			// delete one char
 			input_buf_write_index[term] = (input_buf_write_index[term]-1 + INPUT_BUF_SIZE) % INPUT_BUF_SIZE;
 			input_buf_count[term]--;		
	   	}
	  }
	  else
	  {
	  	  // have vacant space, otherwise, run the bell 
	  	 if((INPUT_BUF_SIZE - input_buf_count[term]) > 0)
 		 {
 			input_buf[term][input_buf_write_index[term]] = c;
 			input_buf_write_index[term] = (input_buf_write_index[term] + 1) % INPUT_BUF_SIZE;
			input_buf_count[term]++;
			if (c == '\n') {
			 	num_readable[term]++;
			 	CondSignal(readable[term]);
			 }
	   	 }
	   	 else 
	   	 {	   	
	   	    // try to make use of pre_echo_indexious read position 	
	   	 	if(input_buf_read_index[term]>0 && input_buf_read_index[term] > 1)
	   	 	{
	   	 		// can take use of already read position
	   	 		input_buf[term][0] = c;  
	   	 		input_buf_write_index[term] = 1; 	 		
	   	 		input_buf_count[term]++;  
	   	 	}
	   	 	else
	   	 	{
	   	 		 // echo to run the bell
		   	 	if ((ECHO_BUF_SIZE - echo_buf_count[term]) > 0) { // Beep
					echo_buf[term][echo_buf_write_index[term]] = '\7';
					echo_buf_write_index[term] = (echo_buf_write_index[term] + 1) % ECHO_BUF_SIZE;
					echo_buf_count[term]++;
				}
			    fprintf(stderr, "Error, not enough space for input_buf in terminal %d",
                  term);

	   	 	}	   	 	
 	
	   	 }
	   	
	  }     
	  return;
}

extern
void check_to_start_echo(int term)
{
	 /* echo start at the first letter */
	  if(echo_status[term] == IDLE && echo_buf_count[term] > 0)
	  {
	  		if(num_writers[term] == 0)
	  		{
	  			WriteDataRegister(term, echo_buf[term][echo_buf_read_index[term]]);  
	  			echo_buf_read_index[term]++;
	  			echo_buf_count[term]--;
	  			echo_status[term] = BUSY;		
	  		}
	  }  
}
extern
void echo_write_char_processing(int term, char c)
{
	/* Echo buf update */
	if (('\b' == c) || ('\177' == c)) {

		if(screen_len[term] == 0)
   	 		return;
		/* write the ‘b b’ 3 sequence char */
		if((ECHO_BUF_SIZE - echo_buf_count[term]) > 3) {
			echo_buf[term][echo_buf_write_index[term]] = '\b';
			echo_buf_write_index[term] = (echo_buf_write_index[term] + 1) % ECHO_BUF_SIZE;
			echo_buf[term][echo_buf_write_index[term]] = ' ';
			echo_buf_write_index[term] = (echo_buf_write_index[term] + 1) % ECHO_BUF_SIZE;
			echo_buf[term][echo_buf_write_index[term]] = '\b';
			echo_buf_write_index[term] = (echo_buf_write_index[term] + 1) % ECHO_BUF_SIZE;		
			echo_buf_count[term] = echo_buf_count[term] + 3;
			screen_len[term]--;
			//printf("echoing the 2nd ' ' letter \n");  //debug
		} //printf("remove the cursor one place to the left\n");  //debug
		else
		{
             // Run the bell
			 echo_buf[term][echo_buf_write_index[term]] = '\7';
			 echo_buf_write_index[term] = (echo_buf_write_index[term] + 1) % ECHO_BUF_SIZE;
			 echo_buf_count[term]++;
		}

	} 
	else if (('\b' != c) && ('\177' != c)) {

		/* Echo only if there's empty space, else drop the character. */
		if (echo_buf_write_index[term] < ECHO_BUF_SIZE-1) { // check the empty space room for beep
			echo_buf[term][echo_buf_write_index[term]] = c;
			echo_buf_write_index[term] = (echo_buf_write_index[term] + 1) % ECHO_BUF_SIZE;
			echo_buf_count[term]++;
			screen_len[term]++;
		} else if (echo_buf_write_index[term] == ECHO_BUF_SIZE-1) { // Beep
			echo_buf[term][echo_buf_write_index[term]] = '\7';
			echo_buf_write_index[term] = (echo_buf_write_index[term] + 1) % ECHO_BUF_SIZE;
			echo_buf_count[term]++;
		}
	}
	return;
}

extern
void echo_read_char_processing(int term)
{
 
    /* last one been echoed to check special delete conditions */
	int pre_echo_index = (echo_buf_read_index[term] - 1 + ECHO_BUF_SIZE) % ECHO_BUF_SIZE;

    /* write the '\n' after '\r'*/
	if ('\r' == echo_buf[term][pre_echo_index]) 
	{ 
		WriteDataRegister(term, '\n');
		echo_buf[term][pre_echo_index] = '\n';
		echo_status[term] = BUSY;
		screen_len[term] = 0;
	}
	else{   // echo normal char
		WriteDataRegister(term, echo_buf[term][echo_buf_read_index[term]]);
		echo_buf_read_index[term] = (echo_buf_read_index[term] + 1) % ECHO_BUF_SIZE;
		echo_buf_count[term]--;
		echo_status[term] = BUSY;
	}

   return;
}
extern
void output_buf_char_processing(int term)
{

	output_buf_counter[term]--;
	/* Keep busy_write as long as there is something to write */
	if (output_buf_counter[term] > 0) {
			
		int i = output_buf_read_index[term];
	    char c = output_buf[term][i];
				
		if ( c == '\n' && is_newline[term]) {
			WriteDataRegister(term, '\r');
			output_buf_counter[term]++;
			statistics[term].user_in--;
			is_newline[term] = false;
		} 
		else if ( c == '\n') {
			WriteDataRegister(term, c);
			screen_len[term] = 0;
			is_newline[term] = true;
			output_buf_read_index[term] = (output_buf_read_index[term]+1) % OUTPUT_BUF_SIZE;
		} 
		else
		{
			if(c == '\b' || c == '\177')
			{
				printf("write the '~b' letter at index %d with char %c \n", i,c);  //debug
			}
			WriteDataRegister(term, c);
			screen_len[term]++;
			is_newline[term] = true;
			output_buf_read_index[term] = (output_buf_read_index[term]+1) % OUTPUT_BUF_SIZE;
		}
    }

	return;
}

/***********************************************/
/*******Device Driver API for User Thread ******/
/***********************************************/

extern
int WriteTerminal(int term, char *buf, int buflen)
{
	Declare_Monitor_Entry_Procedure();

	/* Error Handling */
	if ((buflen < 1) || (term_state[term] < 0) ||
		(term < 0) || (term > NUM_TERMINALS - 1))
		return -1;

	begin_write(term);
	copy_to_output_buf(term, buf, buflen);
    write_firstchar_to_terminal(term);
    end_write(term);
	
	return buflen;
}


extern
int ReadTerminal(int term, char *buf, int buflen)
{
	Declare_Monitor_Entry_Procedure();
	char c;
	int count;

	/* Error Handling */
	if ((buflen < 1) || (buflen > INPUT_BUF_SIZE) || (term_state[term] < 0) || 
		(term < 0) || (term > NUM_TERMINALS - 1))
		return -1;
   
   begin_read(term);
	
	/* Read from input buffer */
	int i;
	for (i = 0; i < buflen;i++) 
	{
		c = input_buf[term][input_buf_read_index[term]];
		input_buf_read_index[term] = (input_buf_read_index[term] + 1) % INPUT_BUF_SIZE;
        /* append a char */		
		int len = strlen(buf);
		*(buf+len) = c;
		*(buf+len+1)= '\0';
		/* end append a char */		
		statistics[term].user_out++;  // ReadTerminal char # counted
		count++;
		input_buf_count[term]--;
		if (c == '\n')
			break;
	}
	if (c != '\n')
		num_readable[term]++;
	
	/* end reading */
	end_read(term);

	return count;
}

/*
 * Called by the hardware once each character typed on the keyboard is ready.
 * It will concatnate the character into the buffer, increment the count
 * and start busy_write to the terminal.
 */
extern
void ReceiveInterrupt(int term)
{
	Declare_Monitor_Entry_Procedure();

	/* Read from register */
	char c = ReadDataRegister(term);
	statistics[term].tty_in++;
    
    echo_write_char_processing(term,c);

	input_buf_char_processing(term,c);

	check_to_start_echo(term);
	    
}


extern
void TransmitInterrupt(int term)
{
	Declare_Monitor_Entry_Procedure();

	/* Statistics */
	statistics[term].tty_out++;
     
    /* First to process echoing chars */
	if(echo_buf_count[term] > 0)
	{
		echo_read_char_processing(term);
	}
	/* WriteTerminal stuff */
	else if(output_buf_counter[term] > 0) 
	{
		
		echo_status[term] = IDLE;  //Echo job is done
		statistics[term].user_in++;
    	output_buf_char_processing(term);   
	}
	else if(output_buf_counter == 0)
	{
		/* Else the output is done */
		CondSignal(busy_write[term]);
	}
	else 
	{
		/* Echo job is done */
		CondSignal(busy_echo[term]);
		echo_status[term] = IDLE;
	}

	return;
}

/********************************/
/******* Init Methods  **********/
/********************************/
extern
int InitTerminal(int term)
{
	Declare_Monitor_Entry_Procedure();

	/* Error Handling */
	if ((term < 0) || (term > NUM_TERMINALS - 1) || (term_state[term] == 0))
		return -1;

	/* Try initializing hardware */
	term_state[term] = InitHardware(term);

	if(term_state[term] == 0) {

		// Statistics
		statistics[term].tty_in = 0;
		statistics[term].tty_out = 0;
		statistics[term].user_in = 0;
		statistics[term].user_out = 0;

		// Condition variables
		writers[term] = CondCreate();
		busy_write[term] = CondCreate();
		busy_echo[term] = CondCreate();
		readers[term] = CondCreate();
		readable[term] = CondCreate();

		// WriteTerminal
		num_writers[term] = 0;
		output_buf_read_index[term] = 0;
		output_buf_write_index[term] = 0;
		output_buf_counter[term] = 0;
		output_buf_len[term] = 0;
		is_newline[term] = true;

		// ReadTerminal
		num_readers[term] = 0;
		num_readable[term] = 0;
		input_buf_write_index[term] = 0;
		input_buf_read_index[term] = 0;

		// Echo
		echo_buf_count[term] = 0;
		input_buf_count[term] = 0;
		screen_len[term] = 0;
		echo_buf_write_index[term] = 0;
		echo_buf_read_index[term] = 0;
		echo_status[term] = IDLE;
		is_first_backspace[term] = true;		
	}

	return term_state[term];
}

/*
 * Initialization needed for the whole terminals;
 */
extern
int InitTerminalDriver()
{
	Declare_Monitor_Entry_Procedure();
	int i;
	for (i = 0; i < NUM_TERMINALS; i++) {
		term_state[i] = -1;
		statistics[i].tty_in = -1;
		statistics[i].tty_out = -1;
		statistics[i].user_in = -1;
		statistics[i].user_out = -1;
	}

	return 0;
}


/* Snapshot of the I/O statistics for all terminals */
extern
int TerminalDriverStatistics(struct termstat *stats)
{
	Declare_Monitor_Entry_Procedure();
	int i;

	for (i = 0; i < NUM_TERMINALS; i++) {
		stats[i].tty_in = statistics[i].tty_in;
		stats[i].tty_out = statistics[i].tty_out;
		stats[i].user_in = statistics[i].user_in;
		stats[i].user_out = statistics[i].user_out;
	}

	return 0;
}


















