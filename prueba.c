#include "stdio.h"
char *a = "hola";

static int sizeNombre(char* nombre){
	int tam=0;
	char*temp=nombre;
	while(temp!="\0"){
        tam++;
        temp=temp+sizeof(char);
    }
    return tam;
}


int main(){
    printf("%d\n",sizeNombre(a));
    return 0;
}
