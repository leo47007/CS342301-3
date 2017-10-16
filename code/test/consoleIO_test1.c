#include "syscall.h"

int
main()
{
	int n;
	for (n=9;n>-15;n--) {
		PrintInt(n);
	}
        Halt();
}

