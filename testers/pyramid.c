
/* prints out a pyramid of numbers */

#include "h/localLibumps.h"
#include "h/tconst.h"
#include "h/print.h"

int main()
{
    
    int status, n, i;
    char buf[20];
    
    print(WRITETERMINAL, "enter a number for the pyramid: \n");

    status = SYSCALL(READTERMINAL, (int)&buf[0], 0, 0);
    buf[status] = EOS;
    cin>> n;
    for(int i=1; i<=n; i++)
    {
        int space= n-i;
        for(int j=1; j<=i; j++)
        {
            if (j<space)
            cout<<" ";
            else
            cout<<i;
        };
        cout<<endl;
    };
    return 0;
}
