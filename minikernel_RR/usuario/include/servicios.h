/*
 *  usuario/include/servicios.h
 *
 *  Minikernel. Versi�n 1.0
 *
 *  Fernando P�rez Costoya
 *
 */

/*
 *
 * Fichero de cabecera que contiene los prototipos de funciones de
 * biblioteca que proporcionan la interfaz de llamadas al sistema.
 *
 *      SE DEBE MODIFICAR AL INCLUIR NUEVAS LLAMADAS
 *
 */

#ifndef SERVICIOS_H
#define SERVICIOS_H

/* Evita el uso del printf de la bilioteca est�ndar */
#define printf escribirf

/* Funcion de biblioteca */
int escribirf(const char *formato, ...);

/* Llamadas al sistema proporcionadas */
int crear_proceso(char *prog);
int terminar_proceso();
int escribir(char *texto, unsigned int longi);
/*I. Funcion que devuelve el identificador de un proceso */
int obtener_id_pr();
/*I. Funcion que duerme el proceso */
int dormir(unsigned int segundos);

/* Funciones del mutex: */
#define NO_RECURSIVO 0
#define RECURSIVO 1

/*I. Funcion que crea un mutex pasandole el nombre y el tipo */
int crear_mutex(char *nombre, int tipo);
/*I. Funcion que abre un mutex pasandole el nombre que lo identifica */
int abrir_mutex(char *nombre);
/*I. Funcion que bloquea el proceso pasandole el id del mutex */
int lock(unsigned int mutexid);
/*I. Funcion que desbloquea el proceso pasandole el id del mutex */
int unlock(unsigned int mutexid);
/*I. Funcion que cierra el mutex pasandole el id del mutex */
int cerrar_mutex(unsigned int mutexid);


#endif /* SERVICIOS_H */

