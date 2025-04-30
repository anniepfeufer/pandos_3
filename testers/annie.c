/* prints a string to the terminal */

#include "h/localLibumps.h"
#include "h/tconst.h"
#include "h/print.h"


void main() {
	int status;
    int status1;
    int i;
	char buf[15];
    char buf1[15];
    char buf2[15];
	
	print(WRITETERMINAL, "Annie's Test starts\n");
	print(WRITETERMINAL, "Enter a string: ");
    print(WRITETERMINAL, "\n");
		
	status = SYSCALL(READTERMINAL, (int)&buf[0], 0, 0);
	buf[status] = EOS;

    print(WRITETERMINAL, "Enter another string of equal length: ");
		
	status1 = SYSCALL(READTERMINAL, (int)&buf1[0], 0, 0);
	buf1[status] = EOS;

    print(WRITETERMINAL, "\n");

    if (status!=status1){
        print(WRITETERMINAL, "string not of equal length");
        SYSCALL(TERMINATE, 0, 0, 0);
    }

    i=0;
    for( i = 0; i < status-1; i++ )
	{
        if ((((int)buf[i]+ (int)buf1[i]) - 0x61 ) < 0x7E)
		{
            buf2[i] = (char)(((int)buf[i]+ (int)buf1[i]) - 0x61 );
        }else
        {
            buf2[i]= (char)0x52;
        }
        
	}

	print(WRITETERMINAL, &buf[0]);
    print(WRITETERMINAL, "plus\n");
    print(WRITETERMINAL, &buf1[0]);
    print(WRITETERMINAL, "equals\n");
    print(WRITETERMINAL, &buf2[0]);


	
	print(WRITETERMINAL, "\n\nAnnie's test concluded\n");

		
	/* Terminate normally */	
	SYSCALL(TERMINATE, 0, 0, 0);
}