#include<stdio.h>

int main(){
	int a = 0x12345678;
	int c = 0x87654321;
	int *p[] = {&a,&c};
	int **sp;
	sp = p;
	int ax = 0xaaaaaaaa;

	ax = *(char*)*sp++ = ax;

	printf("%x	%x	%x\n",a,c,ax);
	return 0;
}
