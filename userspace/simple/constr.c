#include <stdlib.h>
#include <stdio.h>

__attribute__ ((constructor)) void foo(void)
{
	printf("foo is running and printf is available at this point\n");
}

int main(int argc, char* argv[])
{
	printf("%s: main is running with argc=%i\n", argv[0], argc);
    return 0;
}
