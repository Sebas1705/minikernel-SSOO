/*
 *  kernel/kernel.c
 *
 *  Minikernel. Versi�n 1.0
 *
 *  Fernando P�rez Costoya
 *
 */

/*
 *
 * Fichero que contiene la funcionalidad del sistema operativo
 *
 */

#include "kernel.h"	/* Contiene defs. usadas por este modulo */
#include "string.h"
#include <stdlib.h>
/*
 *
 * Funciones relacionadas con la tabla de procesos:
 *	iniciar_tabla_proc buscar_BCP_libre
 *
 */

/*
 * Funci�n que inicia la tabla de procesos
 */
static void iniciar_tabla_proc(){
	int i;

	for (i=0; i<MAX_PROC; i++)
		tabla_procs[i].estado=NO_USADA;
}

/*
 * Funci�n que busca una entrada libre en la tabla de procesos
 */
static int buscar_BCP_libre(){
	int i;

	for (i=0; i<MAX_PROC; i++)
		if (tabla_procs[i].estado==NO_USADA)
			return i;
	return -1;
}

/*
 *
 * Funciones que facilitan el manejo de las listas de BCPs
 *	insertar_ultimo eliminar_primero eliminar_elem
 *
 * NOTA: PRIMERO SE DEBE LLAMAR A eliminar Y LUEGO A insertar
 */

/*
 * Inserta un BCP al final de la lista.
 */
static void insertar_ultimo(lista_BCPs *lista, BCP * proc){
	if (lista->primero==NULL)
		lista->primero= proc;
	else
		lista->ultimo->siguiente=proc;
	lista->ultimo= proc;
	proc->siguiente=NULL;
}

/*
 * Elimina el primer BCP de la lista.
 */
static void eliminar_primero(lista_BCPs *lista){

	if (lista->ultimo==lista->primero)
		lista->ultimo=NULL;
	lista->primero=lista->primero->siguiente;
}

/*
 * Elimina un determinado BCP de la lista.
 */
static void eliminar_elem(lista_BCPs *lista, BCP * proc){
	BCP *paux=lista->primero;

	if (paux==proc)
		eliminar_primero(lista);
	else {
		for ( ; ((paux) && (paux->siguiente!=proc));
			paux=paux->siguiente);
		if (paux) {
			if (lista->ultimo==paux->siguiente)
				lista->ultimo=paux;
			paux->siguiente=paux->siguiente->siguiente;
		}
	}
}

/*
 *
 * Funciones relacionadas con la planificacion
 *	espera_int planificador
 */

/*
 * Espera a que se produzca una interrupcion
 */
static void espera_int(){
	int nivel;

	// printk("-> NO HAY LISTOS. ESPERA INT\n");

	/* Baja al m�nimo el nivel de interrupci�n mientras espera */
	nivel=fijar_nivel_int(NIVEL_1);
	halt();
	fijar_nivel_int(nivel);
}

/*
 * Funci�n de planificacion que implementa un algoritmo FIFO.
 */
static BCP * planificador(){
	while (lista_listos.primero==NULL)
		espera_int();		/* No hay nada que hacer */
	return lista_listos.primero;
}

/*
 *
 * Funcion auxiliar que termina proceso actual liberando sus recursos.
 * Usada por llamada terminar_proceso y por rutinas que tratan excepciones
 *
 */
static void liberar_proceso(){
	BCP * p_proc_anterior;

	liberar_imagen(p_proc_actual->info_mem); /* liberar mapa */

	p_proc_actual->estado=TERMINADO;
	eliminar_primero(&lista_listos); /* proc. fuera de listos */
	/* Realizar cambio de contexto */
	p_proc_anterior=p_proc_actual;
	p_proc_actual=planificador();

	printk("\x1b[32m""-> C.CONTEXTO POR FIN: de %d a %d\n""\x1b[0m",
			p_proc_anterior->id, p_proc_actual->id);
	

	liberar_pila(p_proc_anterior->pila);
	cambio_contexto(NULL, &(p_proc_actual->contexto_regs));
	
        return; /* no deber�a llegar aqui */
}

/*
 *
 * Funciones relacionadas con el tratamiento de interrupciones
 *	excepciones: exc_arit exc_mem
 *	interrupciones de reloj: int_reloj
 *	interrupciones del terminal: int_terminal
 *	llamadas al sistemas: llam_sis
 *	interrupciones SW: int_sw
 *
 */

/*
 * Tratamiento de excepciones aritmeticas
 */
static void exc_arit(){

	if (!viene_de_modo_usuario())
		panico("excepcion aritmetica cuando estaba dentro del kernel");


	printk("\x1b[32m""-> EXCEPCION ARITMETICA EN PROC %d\n""\x1b[0m", p_proc_actual->id);
	liberar_proceso();

        return; /* no deber�a llegar aqui */
}

/*
 * Tratamiento de excepciones en el acceso a memoria
 */
static void exc_mem(){

	if (!viene_de_modo_usuario())
		panico("excepcion de memoria cuando estaba dentro del kernel");


	printk("\x1b[32m""-> EXCEPCION DE MEMORIA EN PROC %d\n""\x1b[0m", p_proc_actual->id);
	liberar_proceso();

        return; /* no deber�a llegar aqui */
}

/*
 * Tratamiento de interrupciones de terminal
 */
static void int_terminal(){
	char car;

	car = leer_puerto(DIR_TERMINAL);
	printk("\x1b[32m""-> TRATANDO INT. DE TERMINAL %c\n""\x1b[0m", car);

        return;
}

/*
 * Tratamiento de interrupciones de reloj
 */
static void int_reloj(){
	
	//Apuntamos al primer elemento de la lista de bloqueados:
	BCP *procARevisar = lista_dormidos.primero;

	//Recorremos la lista de procesos bloqueados:
	while(procARevisar!=NULL){

		//Decrementamos un tick el proceso a revisar:
		procARevisar->ticksDormido--;
		//Apuntamos al siguiente proceso:
		BCP *siguiente=procARevisar->siguiente;

		//Si el los ticks estan a cero lo pasamos de la lista de bloqueados a la de listos:
		if(procARevisar->ticksDormido==0){
			//Elevamos el nivel de int para inhibir otra int_reloj:
			int nivel=fijar_nivel_int(NIVEL_3);
			//Eliminamos del la lista de dormidos:
			eliminar_elem(&lista_dormidos,procARevisar);
			//Lo insertamos al final de la lista de listos:
			insertar_ultimo(&lista_listos,procARevisar);
			//Volvemos al nivel original 
			fijar_nivel_int(nivel);
		}
		//Cambiamos al siguiente proceso:
		procARevisar=siguiente;
	}
}

/*
 * Tratamiento de llamadas al sistema
 */
static void tratar_llamsis(){
	int nserv, res;

	nserv=leer_registro(0);
	if (nserv<NSERVICIOS)
		res=(tabla_servicios[nserv].fservicio)();
	else
		res=-1;		/* servicio no existente */
	escribir_registro(0,res);
	return;
}

/*
 * Tratamiento de interrupciuones software
 */
static void int_sw(){

	printk("\x1b[32m""-> TRATANDO INT. SW\n""\x1b[0m");

	return;
}

/*
 *
 * Funcion auxiliar que crea un proceso reservando sus recursos.
 * Usada por llamada crear_proceso.
 *
 */
static int crear_tarea(char *prog){
	void * imagen, *pc_inicial;
	int error=0;
	int proc;
	BCP *p_proc;

	proc=buscar_BCP_libre();
	if (proc==-1)
		return -1;	/* no hay entrada libre */

	/* A rellenar el BCP ... */
	p_proc=&(tabla_procs[proc]);

	/* crea la imagen de memoria leyendo ejecutable */
	imagen=crear_imagen(prog, &pc_inicial);
	if (imagen)
	{
		p_proc->info_mem=imagen;
		p_proc->pila=crear_pila(TAM_PILA);
		fijar_contexto_ini(p_proc->info_mem, p_proc->pila, TAM_PILA,
			pc_inicial,
			&(p_proc->contexto_regs));
		p_proc->id=proc;
		p_proc->estado=LISTO;

		/* Bucle para inicializar los descriptores */
		for(int i=0; i<NUM_MUT_PROC; i++){
			p_proc->descriptores_mutex[i]=-1;
		}

		/* lo inserta al final de cola de listos */
		int level = fijar_nivel_int(NIVEL_3);
		insertar_ultimo(&lista_listos, p_proc);
		fijar_nivel_int(level);
		error= 0;
	}
	else
		error= -1; /* fallo al crear imagen */

	return error;
}

/*I. Iniciar tabla de mutexs*/
static void iniciar_tabla_mutexs(){
	int i;

	for (i=0; i<NUM_MUT; i++)
		tabla_mutexs[i].creado=0;
}

/*I. Funcion que busca un descriptor libre en el proceso actual*/
static int desLibre(){
	for(int i=0;i<NUM_MUT_PROC;i++){
		if(p_proc_actual->descriptores_mutex[i]==-1)return i;
	}
	return -1;
}

/*I. Funcion que comprueba si ya hay un mutex con el mismo nombre*/
static int existeNombre(char* nombre){
	for(int i=0;i<n_mutexs;i++){
		if(strcmp(nombre,tabla_mutexs[i].nombre)==0)return i;
	}
	return -1;
}

/*I. Funcion igual que existe nombre pero en la tabla de descriptores*/
static int existeNombreDes(char* nombre){
	for(int i=0;i<NUM_MUT_PROC;i++){
		int des=p_proc_actual->descriptores_mutex[i];
		if(des!=-1&&strcmp(tabla_mutexs[des].nombre,nombre)==0)return i;
	}
	return -1;
}

/*I. Funcion que devuelve la primera posicion de un mutex libre*/
static int buscarMutexLibre(){
	for(int i=0;i<NUM_MUT;i++){
		if(tabla_mutexs[i].creado==0)return i;
	}
	return -1;
}

static int cerrarMutex(unsigned int des, unsigned int posDes){
	//Una vez encontrado, se libera:
	p_proc_actual->descriptores_mutex[posDes]=-1;
	//Si esta bloqueado se desbloquea:
	if(tabla_mutexs[des].id_proc_propietario==p_proc_actual->id){
		tabla_mutexs[des].estado=0;
		printk("\x1b[33m""#>\t""\x1b[0m""Unlock %s: des->%d, proc_id->%d (B:%d)\n",tabla_mutexs[des].nombre,des,p_proc_actual->id,tabla_mutexs[des].estado);

		//Si hay procesos bloqueados se desbloquean todos:
		while(tabla_mutexs[des].procesos_bloqueados_lock.primero!=NULL) {
			//Guardamos y elevamos el nivel de interrupcion:
			int nivel_int = fijar_nivel_int(NIVEL_3);

			//Ponemos a listo el proceso que esta esperando:
			BCP* proc_aux = tabla_mutexs[des].procesos_bloqueados_lock.primero;
			proc_aux->estado = LISTO;

			//Lo pasamos de la lista de bloqueados a la de listos:
			eliminar_primero(&(tabla_mutexs[des].procesos_bloqueados_lock)); 
			insertar_ultimo(&lista_listos,proc_aux);

			//Volvemos al nivel de interrupcion:
			fijar_nivel_int(nivel_int);
		}
	
	}
	tabla_mutexs[des].abierto--;
	char *nombre=(char*)malloc(sizeof(char)*MAX_NOM_MUT);
	//Si no hay otros procesos que hayan abierto el mutex, este desaparece
	if(tabla_mutexs[des].abierto==0) {
		tabla_mutexs[des].creado=0;
		strcpy(nombre,tabla_mutexs[des].nombre);
		strcpy(tabla_mutexs[des].nombre,"       ");
		n_mutexs--;
		//Desbloqueamos todos lo procesos que hayan sido bloqueados por el maximo de mutexs:
		//Ya que tiene que comprobar de nuevo si pueden crear un mutex.
		while(lista_bloqueados.primero!=NULL) {
			//Guardamos y elevamos el nivel de interrupcion:
			int nivel_int = fijar_nivel_int(NIVEL_3);

			//Ponemos a listo el proceso que esta esperando:
			BCP* proc_aux = lista_bloqueados.primero;
			proc_aux->estado = LISTO;

			//Lo pasamos de la lista de bloqueados a la de listos:
			eliminar_primero(&lista_bloqueados); 
			insertar_ultimo(&lista_listos,proc_aux);

			//Volvemos al nivel de interrupcion:
			fijar_nivel_int(nivel_int);
		}
	}
	printk("\x1b[33m""#>\t""\x1b[0m""Cerrado %s: des->%d, proc_id->%d (A:%d)(N:%d)\n",nombre,des,p_proc_actual->id,tabla_mutexs[des].abierto,n_mutexs);
	free(nombre);
	return 0;
}

/*
 *
 * Rutinas que llevan a cabo las llamadas al sistema
 *	sis_crear_proceso sis_escribir
 *
 */

/*
 * Tratamiento de llamada al sistema crear_proceso. Llama a la
 * funcion auxiliar crear_tarea sis_terminar_proceso
 */
int sis_crear_proceso(){
	char *prog;
	int res;

	printk("\x1b[32m""-> PROC %d: CREAR PROCESO\n""\x1b[0m", p_proc_actual->id);
	prog=(char *)leer_registro(1);
	printk("PROG: %s\n", prog);
	res=crear_tarea(prog);
	return res;
}

/*
 * Tratamiento de llamada al sistema escribir. Llama simplemente a la
 * funcion de apoyo escribir_ker
 */
int sis_escribir()
{
	char *texto;
	unsigned int longi;

	texto=(char *)leer_registro(1);
	longi=(unsigned int)leer_registro(2);

	escribir_ker(texto, longi);
	return 0;
}

/*
 * Tratamiento de llamada al sistema terminar_proceso. Llama a la
 * funcion auxiliar liberar_proceso
 */
int sis_terminar_proceso(){

	//Cerramos todos los mutex:
	for (int i=0;i<NUM_MUT_PROC;i++){
		if (p_proc_actual->descriptores_mutex[i]!=-1){
			cerrarMutex(p_proc_actual->descriptores_mutex[i],i);
		}
	}
		
	printk("\x1b[32m""-> FIN PROCESO %d\n""\x1b[0m", p_proc_actual->id);

	liberar_proceso();

        return 0; /* no deber�a llegar aqui */
}

/*I. Funcion que devuelve el identificador de un proceso */
int sis_obtener_id_pr(){
	return p_proc_actual->id;
}

/*I. Funcion que duerme el proceso */
int sis_dormir(){

	//Leer de los registros los segundos:
	unsigned int segundos=(unsigned int)leer_registro(1); 

	//Elevar nivel interrupcion y guardar actual:
	int nivel=fijar_nivel_int(NIVEL_3);
	//Cambiar estado a bloqueado:
	p_proc_actual->estado=BLOQUEADO;
	//Cambiar los ticksDormidos a los introducidos:
	p_proc_actual->ticksDormido=segundos*TICK;
	//Guardamos el proceso:
	BCP* p_proc_dormido = p_proc_actual; 
	
	//Eliminamos de la lista de listos:
	eliminar_primero(&lista_listos);
	//Lo insertamos en la lista de dormidos:
	insertar_ultimo(&lista_dormidos, p_proc_dormido);

	//Llamamos al planificador para el nuevo proceso actual:
	p_proc_actual=planificador();
	
	//Hacemos un cambio de contexto, guardando el contexto del proceso dormido:
	printk("\x1b[32m""-> C.CONTEXTO POR DORMIR: de %d a %d\n""\x1b[0m", p_proc_dormido->id, p_proc_actual->id);
	cambio_contexto(&(p_proc_dormido->contexto_regs), &(p_proc_actual->contexto_regs));

	//Volvemos al nivel de int anterior:
	fijar_nivel_int(nivel);

	return 0;
}

/*I. Funcion que crea un mutex */
/**
 * ERRORES:
 * -1: El nombre excede del limite de longitud.
 * -2: No hay descriptor libre en el proceso.
 * -3: Ya existe el nombre.
*/
int sis_crear_mutex(){
	
	//1.Comprobamos que el tamaño del nombre es menor al maximo;
	char* nombre=(char *)leer_registro(1);
	int charSize=strlen(nombre);
	if(charSize>=MAX_NOM_MUT){
		printk("\x1b[31m""[SIS_CREAR_MUTEX] - Nombre demasiado largo (%d)\n""\x1b[0m",charSize);
		return -1;
	} 
	
	//2.Buscamos el descriptor libre:
	int posLibre=desLibre();
	if(posLibre==-1){
		printk("\x1b[31m""[SIS_CREAR_MUTEX] - No hay descriptores libres en el proceso %d\n""\x1b[0m",p_proc_actual->id);
		return -2;
	} 

	//3.Comprobamos que no existe un mutex con el mismo nombre:
	if(existeNombre(nombre)!=-1){
		printk("\x1b[31m""[SIS_CREAR_MUTEX] - Nombre ya existe (%s)\n""\x1b[0m",nombre);
		return -3;
	} 

	//Comprobamos que se puede crear un mutex si no lo bloqueamos:
	int mutexLibre = buscarMutexLibre();
	while(mutexLibre==-1){
		printk("\x1b[31m""[SIS_CREAR_MUTEX] - No hay mutex libre, bloqueando el proceso %d\n""\x1b[0m",p_proc_actual->id);
		//Elevar nivel interrupcion y guardar actual:
		int nivel=fijar_nivel_int(NIVEL_3);
		//Cambiar estado a bloqueado:
		p_proc_actual->estado=BLOQUEADO;
		//Guardamos el proceso:
		BCP* p_proc_bloqueado = p_proc_actual; 
						
		//Eliminamos de la lista de listos:
		eliminar_primero(&lista_listos);
		//Lo insertamos en la lista de bloqueado:
		insertar_ultimo(&lista_bloqueados, p_proc_bloqueado);

		//Llamamos al planificador para el nuevo proceso actual:
		p_proc_actual=planificador();
						
		//Hacemos un cambio de contexto, guardando el contexto del proceso dormido:
		printk("\x1b[32m""-> C.CONTEXTO POR MUTEX NO LIBRE: de %d a %d\n""\x1b[0m",p_proc_bloqueado->id,p_proc_actual->id);
		cambio_contexto(&(p_proc_bloqueado->contexto_regs), &(p_proc_actual->contexto_regs));

		//Volvemos al nivel de int anterior:
		fijar_nivel_int(nivel);
		mutexLibre=buscarMutexLibre();
	}

	//Inicializamos todos los aspectos del mutex:
	MUTEX m;
	strcpy(m.nombre,nombre);
	m.tipo=(int)leer_registro(2);
	m.creado=1;/*Creado*/
	m.procesos_bloqueados_lock.primero=NULL;
	m.procesos_bloqueados_lock.ultimo=NULL;
	m.estado=0;/*No bloqueado*/
	m.abierto=1;/*Abierto*/
	m.id_proc_propietario=-1;

	//Guardamos el descriptor en el array:
	p_proc_actual->descriptores_mutex[posLibre]=mutexLibre;

	//Guardamos el mutex en la lista:
	tabla_mutexs[mutexLibre]=m;
	n_mutexs++;

	printk("\x1b[33m""#>\t""\x1b[0m""Creado %s: des->%d, proc_id->%d (A:%d)(N:%d)\n",tabla_mutexs[mutexLibre].nombre,mutexLibre,p_proc_actual->id,tabla_mutexs[mutexLibre].abierto,n_mutexs);
	return mutexLibre;
}
/*I. Funcion que abre un mutex */
/**
 * ERRORES:
 * -1: El nombre no existe.
 * -2: No hay descriptor libre en el proceso.
*/
int sis_abrir_mutex(){
	
	//1.Comprobamos que existe el mutex:
	char* nombre=(char *)leer_registro(1);
	int des=existeNombre(nombre);
	if(des==-1){
		printk("\x1b[31m""[SIS_ABRIR_MUTEX] - Nombre no existe (%s)\n""\x1b[0m", nombre);
		return -1;
	}

	//Existe pero comprobamos si esta en el array de descriptores:
	int posDes=existeNombreDes(nombre);
	if(posDes==-1){
		//Existe pero no esta dentro del array de descriptores:
		int posLibre=desLibre();
		//2.Comprobamos si hay descriptores libres:
		if(posLibre==-1){
			printk("\x1b[31m""[SIS_ABRIR_MUTEX] - No hay descriptores libres en el proceso %d\n""\x1b[0m",p_proc_actual->id);
			return -2;
		}
		//Si hay hueco pasamos el descriptor:
		p_proc_actual->descriptores_mutex[posLibre]=des;
	}

	//Abrimos el mutex:
	tabla_mutexs[des].abierto++;
	printk("\x1b[33m""#>\t""\x1b[0m""Abierto %s: des->%d, proc_id->%d (A:%d)\n",tabla_mutexs[des].nombre,des,p_proc_actual->id,tabla_mutexs[des].abierto);
	return des;
}
/*I. Funcion que bloquea el proceso */
/**
 * ERRORES:
 * -1: El descriptor se sale del rango posible.
 * -2: El mutex no esta abierto.
 * -3: El mutex no recursivo ya fue bloqueado por el mismo proceso.
*/
int sis_lock(){

	//1.Comprobamos que el descritor esta dentro del rango:
	unsigned int des=(unsigned int)leer_registro(1);
	if(des>=NUM_MUT){
		printk("\x1b[31m""[SIS_LOCK] - El descriptor no existe dentro del rango 0-%d\n""\x1b[0m",NUM_MUT);
		return -1;
	}

	//2.Comprobamos que el descriptor pertenece a un mutex abierto:
	if(tabla_mutexs[des].abierto==0){
		printk("\x1b[31m""[SIS_LOCK] - El descriptor apunta a un mutex que no esta abierto\n""\x1b[0m",NUM_MUT);
		return -2;
	}

	int proc_esperando=1;
	while (proc_esperando){
	    //Si ya esta bloqueado:
		if(tabla_mutexs[des].estado>0){
			//Si es recursivo:
			if(tabla_mutexs[des].tipo==1){
				//Si ha sido bloqueado por el mismo proceso:
				if(tabla_mutexs[des].id_proc_propietario==p_proc_actual->id) {
					//El proceso se puede volver a bloquear:
					tabla_mutexs[des].estado++;
					proc_esperando=0;
				}
				//Si no es el mismo proceso:
				else{
					printk("\x1b[33m""#>\t""\x1b[0m""Block %s: des->%d, proc_id->%d (B:%d)\n",tabla_mutexs[des].nombre,des,p_proc_actual->id,tabla_mutexs[des].estado);
					//Elevar nivel interrupcion y guardar actual:
					int nivel=fijar_nivel_int(NIVEL_3);
					//Cambiar estado a bloqueado:
					p_proc_actual->estado=BLOQUEADO;
							
					//Eliminamos de la lista de listos:
					eliminar_primero(&lista_listos);
					//Lo insertamos en la lista de bloqueado:
					insertar_ultimo(&(tabla_mutexs[des].procesos_bloqueados_lock), p_proc_actual);
					
					//Guardamos el proceso:
					BCP* p_proc_bloqueado = p_proc_actual; 
					//Llamamos al planificador para el nuevo proceso actual:
					p_proc_actual=planificador();
									
					//Hacemos un cambio de contexto, guardando el contexto del proceso dormido:
					printk("\x1b[32m""-> C.CONTEXTO POR BLOQUEO: de %d a %d\n""\x1b[0m",p_proc_bloqueado->id,p_proc_actual->id);
					cambio_contexto(&(p_proc_bloqueado->contexto_regs),&(p_proc_actual->contexto_regs));
		
					//Volvemos al nivel de int anterior:
					fijar_nivel_int(nivel);
					
				}
			}
			//Si no es recursivo: 
			else{
				//Si el mutex fue bloqueado por este proceso:
				//3.Comprobar si ha sido bloqueado por el proceso actual
				if(tabla_mutexs[des].id_proc_propietario==p_proc_actual->id){				
					//El proceso actual ha bloqueado el mutex antes -> interbloqueo
					printk("\x1b[31m""[SIS_LOCK] - El proceso ya ha sido bloqueado por este mutex no recursivo\n""\x1b[0m");
					return -3;
				}
				//Si el mutex no fue bloqueado por este proceso:
				else{
					printk("\x1b[33m""#>\t""\x1b[0m""Block %s: des->%d, proc_id->%d (B:%d)\n",tabla_mutexs[des].nombre,des,p_proc_actual->id,tabla_mutexs[des].estado);
					//Elevar nivel interrupcion y guardar actual:
					int nivel=fijar_nivel_int(NIVEL_3);
					//Cambiar estado a bloqueado:
					p_proc_actual->estado=BLOQUEADO;
					//Guardamos el proceso:
					BCP* p_proc_bloqueado = p_proc_actual; 
									
					//Eliminamos de la lista de listos:
					eliminar_primero(&lista_listos);
					//Lo insertamos en la lista de bloqueado:
					insertar_ultimo(&(tabla_mutexs[des].procesos_bloqueados_lock), p_proc_bloqueado);

					//Llamamos al planificador para el nuevo proceso actual:
					p_proc_actual=planificador();
									
					//Hacemos un cambio de contexto, guardando el contexto del proceso dormido:
					printk("\x1b[32m""-> C.CONTEXTO POR BLOQUEO: de %d a %d\n""\x1b[0m",p_proc_bloqueado->id,p_proc_actual->id);
					cambio_contexto(&(p_proc_bloqueado->contexto_regs), &(p_proc_actual->contexto_regs));

					//Volvemos al nivel de int anterior:
					fijar_nivel_int(nivel);
				}
			}
		}
		//Si no esta bloqueado:
		else {
			tabla_mutexs[des].estado++;
			proc_esperando=0;
		} 
	}

	//Se asigna el propietario a este proceso:
	tabla_mutexs[des].id_proc_propietario=p_proc_actual->id;
	printk("\x1b[33m""#>\t""\x1b[0m""Lock %s: des->%d, proc_id->%d (B:%d)\n",tabla_mutexs[des].nombre,des,p_proc_actual->id,tabla_mutexs[des].estado);

	return 0;
}

/*I. Funcion que desbloquea el proceso pasandole el id del mutex */
/**
 * ERRORES:
 * -1: El descriptor se sale del rango posible.
 * -2: El mutex no esta abierto.
 * -3: Proceso no propietario intenta desbloquear.
 * -4: Mutex no esta bloqueado.
*/
int sis_unlock(){

	//1.Comprobamos que el descritor esta dentro del rango:
	unsigned int des=(unsigned int)leer_registro(1);
	if(des>=NUM_MUT){
		printk("\x1b[31m""[SIS_UNLOCK] - El descriptor no existe dentro del rango 0-%d\n""\x1b[0m",NUM_MUT);
		return -1;
	}

	//2.Comprobamos que el descriptor pertenece a un mutex abierto:
	if(tabla_mutexs[des].abierto==0){
		printk("\x1b[31m""[SIS_UNLOCK] - El descriptor apunta a un mutex que no esta abierto\n""\x1b[0m",NUM_MUT);
		return -2;
	}

	//Si esta bloqueado:
	if(tabla_mutexs[des].estado>0){
		//Si es recursivo:
		if(tabla_mutexs[des].tipo==1){
			//Si ha sido bloqueado por el proceso actual:
			if(tabla_mutexs[des].id_proc_propietario==p_proc_actual->id){
				//Desbloqueamos:
				tabla_mutexs[des].estado--;
				//Comprobamos si ya no esta bloqueado:
				if(tabla_mutexs[des].estado==0){
					//Si hay algun proceso esperando el mutex por lock, se le desbloquea
					if(tabla_mutexs[des].procesos_bloqueados_lock.primero!=NULL){
						//Guardamos y elevamos el nivel de interrupcion:
						int nivel_int = fijar_nivel_int(NIVEL_3);

						//Ponemos a listo el proceso que esta esperando:
						BCP* proc_aux = tabla_mutexs[des].procesos_bloqueados_lock.primero;
						proc_aux->estado = LISTO;

						//Lo pasamos de la lista de bloqueados a la de listos:
						eliminar_primero(&tabla_mutexs[des].procesos_bloqueados_lock); 
						insertar_ultimo(&lista_listos,proc_aux);

						//Volvemos al nivel de interrupcion:
						fijar_nivel_int(nivel_int);
						printk("\x1b[33m""#>\t""\x1b[0m""Unblock %s: des->%d, proc_id->%d (B:%d)\n",tabla_mutexs[des].nombre,des,p_proc_actual->id,tabla_mutexs[des].estado);
					}
					//Eliminamos al propietario del mutex:
					tabla_mutexs[des].id_proc_propietario=-1;
				}
			}
			//3.Si este proceso no es el propietario, no puede desbloquear:
			else{
				printk("\x1b[31m""[SIS_UNLOCK] - Proceso no propietario intenta desbloquear\n""\x1b[0m",NUM_MUT);
				return -3;
			} 
		} 
		//Si no es recursivo:
		else {
			//Si es propietario el proceso actual:
			if(tabla_mutexs[des].id_proc_propietario==p_proc_actual->id){
				//Desbloquear:
				tabla_mutexs[des].estado--;
				//Eliminar al propietario:
				tabla_mutexs[des].id_proc_propietario=-1;
				//Si hay procesos bloqueados:
				if(tabla_mutexs[des].procesos_bloqueados_lock.primero!=NULL){
					//Guardamos y elevamos el nivel de interrupcion:
					int nivel_int = fijar_nivel_int(NIVEL_3);

					//Ponemos a listo el proceso que esta esperando:
					BCP* proc_aux = tabla_mutexs[des].procesos_bloqueados_lock.primero;
					proc_aux->estado = LISTO;

					//Lo pasamos de la lista de bloqueados a la de listos:
					eliminar_primero(&tabla_mutexs[des].procesos_bloqueados_lock); 
					insertar_ultimo(&lista_listos,proc_aux);

					//Volvemos al nivel de interrupcion:
					fijar_nivel_int(nivel_int);
					printk("\x1b[33m""#>\t""\x1b[0m""Unblock %s: des->%d, proc_id->%d (B:%d)\n",tabla_mutexs[des].nombre,des,p_proc_actual->id,tabla_mutexs[des].estado);
				}
			}
			//Si no es propietario:
			else{
				printk("\x1b[31m""[SIS_UNLOCK] - Proceso no propietario intenta desbloquear\n""\x1b[0m",NUM_MUT);
				return -3;
			}
		}
	}
	//Si no esta bloqueado:
	else {
		printk("\x1b[31m""[SIS_UNLOCK] - Mutex no esta bloqueado\n""\x1b[0m",NUM_MUT);
		return -4;
	}

	printk("\x1b[33m""#>\t""\x1b[0m""Unlock %s: des->%d, proc_id->%d (B:%d)\n",tabla_mutexs[des].nombre,des,p_proc_actual->id,tabla_mutexs[des].estado);
	return 0;
}
/*I. Funcion que cierra el mutex pasandole el id del mutex */
/**
 * ERRORES:
 * -1: El descriptor se sale del rango posible.
 * -2: El mutex no esta abierto.
 * -3: Proceso no propietario intenta desbloquear.
 * -4: Mutex no esta bloqueado.
*/
int sis_cerrar_mutex(){
	
	//1.Comprobamos que el descritor esta dentro del rango:
	unsigned int des=(unsigned int)leer_registro(1);
	if(des>=NUM_MUT){
		printk("\x1b[31m""[SIS_UNLOCK] - El descriptor no existe dentro del rango 0-%d\n""\x1b[0m",NUM_MUT);
		return -1;
	}

	//2.Comprobamos que el descriptor pertenece a un mutex abierto:
	if(tabla_mutexs[des].abierto==0){
		printk("\x1b[31m""[SIS_UNLOCK] - El descriptor apunta a un mutex que no esta abierto\n""\x1b[0m",NUM_MUT);
		return -2;
	}

	//Se busca en los descriptores del proceso:
    int posDes=existeNombreDes(tabla_mutexs[des].nombre);
	return cerrarMutex(des,posDes);
}

/*
 *
 * Rutina de inicializaci�n invocada en arranque
 *
 */
int main(){
	/* se llega con las interrupciones prohibidas */

	instal_man_int(EXC_ARITM, exc_arit); 
	instal_man_int(EXC_MEM, exc_mem); 
	instal_man_int(INT_RELOJ, int_reloj); 
	instal_man_int(INT_TERMINAL, int_terminal); 
	instal_man_int(LLAM_SIS, tratar_llamsis); 
	instal_man_int(INT_SW, int_sw); 

	iniciar_cont_int();		/* inicia cont. interr. */
	iniciar_cont_reloj(TICK);	/* fija frecuencia del reloj */
	iniciar_cont_teclado();		/* inici cont. teclado */

	iniciar_tabla_proc();		/* inicia BCPs de tabla de procesos */
	iniciar_tabla_mutexs();     /* I. inciar tabla de mutexs*/

	/* crea proceso inicial */
	if (crear_tarea((void *)"init")<0)
		panico("no encontrado el proceso inicial");
	
	/* activa proceso inicial */
	p_proc_actual=planificador();
	cambio_contexto(NULL, &(p_proc_actual->contexto_regs));
	panico("S.O. reactivado inesperadamente");
	return 0;
}
