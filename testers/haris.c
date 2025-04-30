/* prints a number to the terminal */

#include "h/localLibumps.h"
#include "h/tconst.h"
#include "h/print.h"

void main()
{
    char buf1[10], buf2[10], resultBuf[20];
    int status1, status2;
    int num1, num2, sum, i, j, k, l;

    print(WRITETERMINAL, "Adder Test Begins\n");
    print(WRITETERMINAL, "Enter first number: ");

    status1 = SYSCALL(READTERMINAL, (int)&buf1[0], 0, 0);
    buf1[status1 - 1] = EOS;

    print(WRITETERMINAL, "Enter second number: ");

    status2 = SYSCALL(READTERMINAL, (int)&buf2[0], 0, 0);
    buf2[status2 - 1] = EOS;

    num1 = 0;
    num2 = 0;

    for (i = 0; buf1[i] != EOS; i++)
    {
        num1 = num1 * 10 + (buf1[i] - '0');
    }

    for (j = 0; buf2[j] != EOS; j++)
    {
        num2 = num2 * 10 + (buf2[j] - '0');
    }

    sum = num1 + num2;

    k = 0;
    int temp = sum;
    do
    {
        resultBuf[k++] = (temp % 10) + '0';
        temp /= 10;
    } while (temp > 0);
    resultBuf[k] = EOS;

    for (l = 0; l < k / 2; l++)
    {
        char t = resultBuf[l];
        resultBuf[l] = resultBuf[k - l - 1];
        resultBuf[k - l - 1] = t;
    }

    print(WRITETERMINAL, "Sum is: ");
    print(WRITETERMINAL, resultBuf);
    print(WRITETERMINAL, "\nAdder Test Completed\n");

    SYSCALL(TERMINATE, 0, 0, 0);
}
