#include "servicios.h"

int main(){
    printf("pruebasID: comienza\n");
    int i;
    for (i=1; i<=2; i++){
        if (crear_proceso("yosoy")<0)                  
			printf("Error creando yosoy\n");
		else 
			printf("PROCCESS ID: %d\n",obtener_id_pr());
	}
	printf("pruebasID: termina\n");
    return 0;
}