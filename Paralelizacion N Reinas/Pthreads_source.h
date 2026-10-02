#ifndef PTHREADS_SOURCE_H
#define PTHREADS_SOURCE_H

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
/*---------------------------------------------------------------------------
 * Constantes
 *--------------------------------------------------------------------------*/
#define MAXSIZE 18
/*---------------------------------------------------------------------------
 * Tipos de tarea
 * TIPO_BT1: primera reina en esquina (col 0), usa Backtrack1
 * TIPO_BT2: primera reina en columna interior,  usa Backtrack2
 *--------------------------------------------------------------------------*/
#define TIPO_BT1 1
#define TIPO_BT2 2

/*---------------------------------------------------------------------------
 * Descriptor de tarea (segundo nivel de fila ya propagado)
 *
 * Cada tarea fija las dos primeras reinas y arranca el backtracking
 * desde la fila y_inicio con las amenazas left/down/right ya calculadas.
 * De esta forma el pool tiene muchas tareas pequeñas y el balance
 * de carga mejora respecto a fijar solo la primera reina.
 *
 * Campos de simetría (solo para TIPO_BT2):
 *   bound1, bound2  : límites del bucle exterior de NQueens
 *   lastmask        : máscara de columnas prohibidas para la última reina
 *   endbit          : bit esperado en BOARD[SIZEE] para simetría 180°
 *   sidemask        : máscara de columnas borde (TOPBIT | 1)
 *--------------------------------------------------------------------------*/
typedef struct {
    int tipo;           /* TIPO_BT1 o TIPO_BT2                              */
    int board0;         /* posición (bitmask) de la reina en fila 0         */
    int board1;         /* posición (bitmask) de la reina en fila 1         */
    int y_inicio;       /* fila desde la que arranca el backtracking (2 o 3)*/
    int left;           /* amenaza diagonal izquierda acumulada hasta fila 1 */
    int down;           /* columnas ocupadas acumuladas hasta fila 1         */
    int right;          /* amenaza diagonal derecha acumulada hasta fila 1   */
    /* parámetros de simetría necesarios dentro de Backtrack2 */
    int bound1;
    int bound2;
    int lastmask;
    int endbit;
    int sidemask;
} Tarea;

/*---------------------------------------------------------------------------
 * Pool de tareas compartido entre los hilos de un proceso MPI
 *--------------------------------------------------------------------------*/
typedef struct {
    Tarea   *tareas;        /* arreglo de tareas generadas antes del launch  */
    int      total;         /* cantidad total de tareas en el pool            */
    int      siguiente;     /* índice de la próxima tarea disponible          */
    pthread_mutex_t mutex;  /* protege el acceso a 'siguiente'                */
} Pool;

/*
 * lanzar_hilos:
 *   Crea num_hilos hilos Pthreads, cada uno ejecuta funcion_hilo.
 *   Espera a que todos terminen (pthread_join) y acumula los contadores
 *   parciales en count8_out, count4_out, count2_out.
 *
 *   Parámetros:
 *     size        : N del problema
 *     num_hilos   : cantidad de hilos a crear
 *     pool        : pool de tareas compartido
 *     hilos       : puntero a arreglo de pthread_t para almacenar los hilos
 *     count8_out  : puntero donde se devuelve la suma de COUNT8 de todos los hilos
 *     count4_out  : puntero donde se devuelve la suma de COUNT4
 *     count2_out  : puntero donde se devuelve la suma de COUNT2
 */
extern void lanzar_hilos(int size, int num_hilos, Pool *pool,
                 pthread_t *hilos, 
                 double *tiempo,
                 long int *id,
                 long int *count8,
                 long int *count4,
                 long int *count2);

// Función para medir el tiempo (dwalltime)
extern double dwalltime(void);
#endif /* PTHREADS_SOURCE_H */