
int NUM_TERMINAL= 10;

char *hw_records;
char *user_records;

int tyy_in = 0; //a successful ReadDataRegister call for some terminal,  add 1
int tyy_out= 0; // a successful WriteDataRegister call for some terminal
 
int user_in = 0;  //user thread does a WriteTerminal call for some terminal, add by length of chars

int user_out = 0; //user thread does a successful ReadTerminal call for some terminal, increase by the # of chars returned by request.

int[] io_statistics = new int[NUM_TERMINAL]{-1,-1,-1,-1,......}; 
// after initialize: io_statistics[i] = 0 ; // status changed;

