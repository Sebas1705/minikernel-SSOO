/*
 *  minikernel/include/kernel.h
 *
 *  Minikernel. Versi�n 1.0
 *
 *  Fernando P�rez Costoya
 *
 */

/*
 *
 * Fichero de cabecera que contiene definiciones usadas por kernel.c
 *
 *      SE DEBE MODIFICAR PARA INCLUIR NUEVA FUNCIONALIDAD
 *
 */

#ifndef _KERNEL_H
#define _KERNEL_H

#include "const.h"
#include "HAL.h"
#include "llamsis.h"

/*
 *
 * Definicion del tipo que corresponde con el BCP.
 * Se va a modificar al incluir la funcionalidad pedida.
 *
 */
typedef struct BCP_t *BCPptr;

typedef struct BCP_t {
    int id;									/* ident. del proceso */
    int estado;								/* TERMINADO|LISTO|EJECUCION|BLOQUEADO*/
    contexto_t contexto_regs;				/* copia de regs. de UCP */
    void * pila;							/* dir. inicial de la pila */
	BCPptr siguiente;						/* puntero a otro BCP */
	void *info_mem;							/* descriptor del mapa de memoria */
	unsigned int ticksDormido;  			/* número de ticks que tienes que estar dormido */
	int descriptores_mutex[NUM_MUT_PROC];	/* array de descriptores de cada proceso */

	//Round-Robin:
	unsigned int rodaja;					/* tiempo de ejecucion que le queda al proceso o rodaja */
	
} BCP;



/*
 *
 * Definicion del tipo que corresponde con la cabecera de una lista
 * de BCPs. Este tipo se puede usar para diversas listas (procesos listos,
 * procesos bloqueados en sem�foro, etc.).
 *
 */

typedef struct{
	BCP *primero;
	BCP *ultimo;
} lista_BCPs;

typedef struct MUTEX_t *MUTEXptr;

typedef struct MUTEX_t { 
	char nombre[MAX_NOM_MUT];
	int tipo;
	int estado; /* 0-No bloqueado, 1-Bloqueado, 1o> - Bloqueado mas de una vez*/
	int creado; /* 0-No creado, 1-Creado*/
	int abierto;/* 0-Cerrado, 1-Abierto*/
	int id_proc_propietario;
	lista_BCPs procesos_bloqueados_lock;
} MUTEX;


/*
 * Variable global que cuenta el numero de mutex en la lista
 */
int n_mutexs=0;

/*
 * Variable global que identifica el proceso actual
 */

BCP * p_proc_actual=NULL;

/*
 * Variable global que representa la tabla de procesos
 */

BCP tabla_procs[MAX_PROC];

/*
 * I. Variable global que representa la tabla de procesos
 */

MUTEX tabla_mutexs[NUM_MUT];

/*
 * Variable global que representa la cola de procesos listos
 */
lista_BCPs lista_listos= {NULL, NULL};

/*
 * I. Variable global que representa la cola de procesos dormido
 */
lista_BCPs lista_dormidos= {NULL, NULL};

/*
 * I. Variable global que represental la cola de procesos bloqueados por mutex
 */
lista_BCPs lista_bloqueados= {NULL, NULL};

/*
 *
 * Definici�n del tipo que corresponde con una entrada en la tabla de
 * llamadas al sistema.
 *
 */
typedef struct{
	int (*fservicio)();
} servicio;


/*
 * Prototipos de las rutinas que realizan cada llamada al sistema
 */
int sis_crear_proceso();
int sis_terminar_proceso();
int sis_escribir();
/*I. Funcion que devuelve el identificador de un proceso */
int sis_obtener_id_pr();
/*I. Funcion que duerme el proceso */
int sis_dormir();
/*I. Funcion que crea un mutex */
int sis_crear_mutex();
/*I. Funcion que abre un mutex */
int sis_abrir_mutex();
/*I. Funcion que bloquea el proceso */
int sis_lock();
/*I. Funcion que desbloquea el proceso pasandole el id del mutex */
int sis_unlock();
/*I. Funcion que cierra el mutex pasandole el id del mutex */
int sis_cerrar_mutex();

/*
 * Variable global que contiene las rutinas que realizan cada llamada
 */
servicio tabla_servicios[NSERVICIOS]={	
					{sis_crear_proceso},
					{sis_terminar_proceso},
					{sis_escribir},
					{sis_obtener_id_pr},
					{sis_dormir},
					{sis_crear_mutex},
					{sis_abrir_mutex},
					{sis_lock},
					{sis_unlock},
					{sis_cerrar_mutex}};

#endif /* _KERNEL_H */

