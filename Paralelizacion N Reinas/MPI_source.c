/*****************************************************************************
 *
 * Argumentos:
 *   argv[1]: N (tamaño del tablero, entre 14 y 18)
 *   argv[2]: número de hilos Pthreads por proceso MPI
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include "Pthreads_source.h"
#include <sys/time.h>


/*===========================================================================
 * recorrer_arbol — núcleo único de enumeración de tareas
 *
 * Replica los dos bucles de NQueens() secuencial (BT1 y BT2) para
 * enumerar las tareas que le corresponden a este proceso MPI según la
 * distribución round-robin (tarea i → proceso i % num_procs).
 *
 * Esta función tiene DOS MODOS de operación, controlados por el
 * parámetro buf:
 *
 *   buf == NULL  →  modo CONTEO: recorre el árbol pero no escribe nada,
 *                   solo cuenta cuántas tareas le corresponden a este
 *                   proceso. Es la base de contar_tareas().
 *
 *   buf != NULL  →  modo LLENADO: asume que buf ya tiene exactamente el
 *                   tamaño retornado por una llamada previa en modo
 *                   conteo, y completa cada Tarea en buf[0..total-1].
 *                   Es la base de llenar_tareas().
 *=========================================================================*/
static int recorrer_arbol(int size, int rank, int num_procs, Tarea *buf)
{
    /* Constantes del tablero, iguales que en NQueens() secuencial */
    int SIZEE  = size - 1;
    int TOPBIT = 1 << SIZEE;               /* bit de la columna N-1          */
    int MASK   = (1 << size) - 1;          /* N bits a 1                     */

    int total_global = 0;   /* contador global de tareas (antes del filtro) */
    int total_local  = 0;   /* tareas que le corresponden a este proceso    */

    int bit1, left2, down2, right2, bitmap2, bit_col2;

    /*-----------------------------------------------------------------------
     * BLOQUE BT1: primera reina en (fila 0, col 0), segunda en col BOUND1
     *
     * Replica el bucle:
     *   for (BOUND1=2; BOUND1<SIZEE; BOUND1++) { Backtrack1(2, ...); }
     * de NQueens() secuencial.
     *
     * Segundo nivel: para cada BOUND1, calcula las columnas libres en la
     * fila 2 y genera una tarea por cada columna libre. El backtracking
     * de cada tarea arranca en la fila 3 con las amenazas de las tres
     * primeras reinas ya propagadas.
     *----------------------------------------------------------------------*/
    for (int BOUND1 = 2; BOUND1 < SIZEE; BOUND1++) {

        /* bit1: bitmask de la segunda reina en la columna BOUND1 */
        bit1 = 1 << BOUND1;

        /* Amenazas acumuladas después de colocar reina 0 en col 0
         * y reina 1 en col BOUND1. Son exactamente los valores que
         * NQueens() pasa como (left, down, right) a Backtrack1(2,...). */
        left2  = (2 | bit1) << 1;   /* diagonales izquierdas de filas 0 y 1 */
        down2  =  1 | bit1;         /* columnas 0 y BOUND1 ocupadas          */
        right2 = bit1 >> 1;         /* diagonal derecha de fila 1            */

        /* Columnas libres en fila 2, respetando la restricción de BOUND1:
         * si y=2 < BOUND1, Backtrack1 prohíbe la columna 1 (bit=2).
         * Aplicamos aquí esa misma restricción para no generar tareas
         * inválidas que el backtracking luego descartaría. */
        bitmap2 = MASK & ~(left2 | down2 | right2);
        if (2 < BOUND1) {
            bitmap2 |= 2;   /* activa el bit 1 para luego apagarlo con XOR  */
            bitmap2 ^= 2;   /* fuerza el bit 1 a 0: columna 1 prohibida     */
        }

        /* Segundo nivel: una tarea por cada columna libre en fila 2 */
        while (bitmap2) {
            /* Extrae la columna más a la izquierda disponible */
            bit_col2 = -bitmap2 & bitmap2;
            bitmap2 ^= bit_col2;

            /* Distribución round-robin: esta tarea global le toca al proceso
             * cuyo rank es (total_global % num_procs) */
            if ((total_global % num_procs) == rank) {
                /* Modo llenado: completar la tarea en el arreglo ya
                 * reservado al tamaño exacto. Modo conteo: omitir. */
                if (buf != NULL) {
                    Tarea *t  = &buf[total_local];
                    t->tipo   = TIPO_BT1;
                    t->board0 = 1;          /* reina fila 0 en col 0 (esquina)  */
                    t->board1 = bit1;       /* reina fila 1 en col BOUND1       */
                    t->bound1 = BOUND1;
                    t->bound2 = 0;          /* no usado en BT1                  */
                    t->y_inicio = 3;        /* fila 3 es la primera libre       */

                    /* Propagar amenazas de la tercera reina (fila 2, col
                     * bit_col2) para que el backtracking arranque con el
                     * estado correcto */
                    t->left  = ((left2  | bit_col2) << 1) & MASK;
                    t->down  =   down2  | bit_col2;
                    t->right = ((right2 | bit_col2) >> 1) & MASK;

                    /* BT1 no usa parámetros de simetría */
                    t->lastmask = 0;
                    t->endbit   = 0;
                    t->sidemask = 0;
                }
                total_local++;
            }
            total_global++;
        }
    }

    /*-----------------------------------------------------------------------
     * BLOQUE BT2: primera reina en columna interior BOUND1
     *
     * Replica el bucle:
     *   for (BOUND1=1, BOUND2=N-2; BOUND1<BOUND2; BOUND1++, BOUND2--)
     *       { Backtrack2(1, ...); }
     * de NQueens() secuencial, incluyendo la actualización de LASTMASK
     * y ENDBIT al final de cada iteración.
     *
     * Segundo nivel: para cada BOUND1, calcula las columnas libres en la
     * fila 1 y genera una tarea por cada columna libre. El backtracking
     * de cada tarea arranca en la fila 2 con las amenazas ya propagadas.
     *
     * Los parámetros de simetría (LASTMASK, ENDBIT, SIDEMASK) cambian en
     * cada iteración y se guardan en la tarea para que Backtrack2 y Check()
     * los usen correctamente.
     *----------------------------------------------------------------------*/
    int SIDEMASK = TOPBIT | 1;   /* columnas borde: 0 y N-1                 */
    int LASTMASK = TOPBIT | 1;   /* columnas prohibidas para la última reina */
    int ENDBIT   = TOPBIT >> 1;  /* columna esperada en última fila (180°)   */

    for (int BOUND1 = 1, BOUND2 = size - 2;
        BOUND1 < BOUND2;
        BOUND1++, BOUND2--) {

        /* bit1: bitmask de la primera reina en la columna BOUND1 */
        bit1 = 1 << BOUND1;

        /* Amenazas de la primera reina (a diferencia de BT1, aquí solo
         * hay una reina fija antes de calcular el segundo nivel) */
        int left1  = bit1 << 1;
        int down1  = bit1;
        int right1 = bit1 >> 1;

        /* Columnas libres en fila 1, aplicando restricción de SIDEMASK:
         * si y=1 < BOUND1, Backtrack2 prohíbe las columnas borde.
         * Para BOUND1=1 la condición es falsa (1 < 1 → false), no aplica.
         * Para BOUND1=2 es verdadera (1 < 2 → true), aplica. */
        int bitmap1 = MASK & ~(left1 | down1 | right1);
        if (1 < BOUND1) {
            bitmap1 |= SIDEMASK;
            bitmap1 ^= SIDEMASK;
        }

        /* Segundo nivel: una tarea por cada columna libre en fila 1 */
        while (bitmap1) {
            bit_col2 = -bitmap1 & bitmap1;
            bitmap1 ^= bit_col2;

            if ((total_global % num_procs) == rank) {
                /* Modo llenado: completar la tarea. Modo conteo: omitir. */
                if (buf != NULL) {
                    Tarea *t  = &buf[total_local];
                    t->tipo   = TIPO_BT2;
                    t->board0 = bit1;       /* reina fila 0 en col BOUND1       */
                    t->board1 = bit_col2;   /* reina fila 1 en col libre        */
                    t->bound1 = BOUND1;
                    t->bound2 = BOUND2;
                    t->y_inicio = 2;        /* fila 2 es la primera libre       */

                    /* Propagar amenazas de las dos primeras reinas */
                    t->left  = ((left1  | bit_col2) << 1) & MASK;
                    t->down  =   down1  | bit_col2;
                    t->right = ((right1 | bit_col2) >> 1) & MASK;

                    /* Guardar estado de simetría de esta iteración del
                     * bucle: cada iteración tiene valores distintos */
                    t->lastmask = LASTMASK;
                    t->endbit   = ENDBIT;
                    t->sidemask = SIDEMASK;
                }
                total_local++;
            }
            total_global++;
        }

        /* Actualizar LASTMASK y ENDBIT al final de cada iteración,
         * exactamente igual que en NQueens() secuencial */
        LASTMASK |= LASTMASK >> 1 | LASTMASK << 1;
        ENDBIT   >>= 1;
    }

    return total_local;
}

/*===========================================================================
 * contar_tareas
 *
 * Wrapper delgado sobre recorrer_arbol() en modo conteo (buf=NULL).
 * Recorre el árbol de búsqueda sin escribir nada, solo para determinar
 * cuántas tareas le corresponden a este proceso. El resultado se usa
 * para reservar memoria con el tamaño exacto, sin sobrante.
 *
 * Esta función representa trabajo de cómputo real (recorrido del árbol
 * con poda), por lo que SÍ debe incluirse en el tiempo medido.
 *=========================================================================*/
static int contar_tareas(int size, int rank, int num_procs)
{
    return recorrer_arbol(size, rank, num_procs, NULL);
}

/*===========================================================================
 * llenar_tareas
 *
 * Wrapper delgado sobre recorrer_arbol() en modo llenado (buf != NULL).
 * Recorre el mismo árbol que contar_tareas() y completa cada Tarea en
 * el arreglo buf, que el llamador debe haber reservado previamente con
 * exactamente el tamaño retornado por contar_tareas().
 *
 * Esta función también representa cómputo real y debe incluirse en el
 * tiempo medido.
 *=========================================================================*/
static void llenar_tareas(int size, int rank, int num_procs, Tarea *buf)
{
    recorrer_arbol(size, rank, num_procs, buf);
}

/*===========================================================================
 * main
 *=========================================================================*/
int main(int argc, char *argv[])
{
    int rank, num_procs;

    /*-----------------------------------------------------------------------
     * Inicialización MPI
     * MPI_THREAD_FUNNELED: solo el hilo principal llama a MPI.
     * Es suficiente porque las llamadas MPI (Reduce) se hacen antes y
     * después de los hilos Pthreads, nunca durante su ejecución.
     *----------------------------------------------------------------------*/
    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, &provided);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &num_procs);

    /*-----------------------------------------------------------------------
     * Lectura de argumentos
     *----------------------------------------------------------------------*/
    if (argc < 3) {
        if (rank == 0)
            fprintf(stderr, "Uso: %s <N> <num_hilos>\n", argv[0]);
        MPI_Finalize();
        return 1;
    }

    int size      = atoi(argv[1]);
    int num_hilos = atoi(argv[2]);

    if (size < 4 || size > MAXSIZE) {
        if (rank == 0)
            fprintf(stderr, "N debe estar entre 4 y %d\n", MAXSIZE);
        MPI_Finalize();
        return 1;
    }


    Pool pool;
    //alocacion de hilos y argumentos de hilos para lanzar_hilos
    pthread_t *hilos = (pthread_t *)malloc(num_hilos * sizeof(pthread_t));    
    double *tiempo = (double *)malloc(num_hilos * sizeof(double));//vetor para calculo de tiempo de hilos
    long int *idx = (long int *)malloc(num_hilos * sizeof(long int));
    //posicion 0=count2, posicion 1=count4, posicion 2=count8
    long int *count_nodo = (long int *)calloc(3, sizeof(long int));
    long int *count_total = (long int *)malloc(3 * sizeof(long int));

    /*-----------------------------------------------------------------------
     * Medición de tiempo por segmentos
     *
     * Medimos cada segmento de cómputo/comunicación por separado
     * con dwalltime() y acumular sus duraciones en tiempo_nodo. El hueco
     * entre segmentos (donde ocurre el malloc del pool) no se mide nunca,
     * porque no hay ningún dwalltime() llamado durante ese hueco.
     *----------------------------------------------------------------------*/
    double tiempo_nodo = 0.0;
    double t0, t1;

    MPI_Barrier(MPI_COMM_WORLD);
    /*-----------------------------------------------------------------------
     * Segmento 1: contar tareas
     *
     * Recorre el árbol de búsqueda para determinar
     * exactamente cuántas tareas le corresponden a este proceso.
     *----------------------------------------------------------------------*/
    t0 = dwalltime();
    int num_tareas = contar_tareas(size, rank, num_procs);
    t1 = dwalltime();
    tiempo_nodo += (t1 - t0);

    /*-----------------------------------------------------------------------
     * Alocación EXACTA del pool 
     *----------------------------------------------------------------------*/
    Tarea *tareas = (Tarea *)malloc(num_tareas * sizeof(Tarea));

    /*-----------------------------------------------------------------------
     * Segmento 2: llenar tareas
     *
     * Recorre el mismo árbol (mismo camino de ejecución que contar_tareas,
     * vía recorrer_arbol) y completa cada Tarea en el arreglo ya reservado
     * al tamaño exacto.
     *----------------------------------------------------------------------*/

    MPI_Barrier(MPI_COMM_WORLD);
    t0 = dwalltime();
    llenar_tareas(size, rank, num_procs, tareas);
    t1 = dwalltime();
    tiempo_nodo += (t1 - t0);

    pool.tareas    = tareas;
    pool.total     = num_tareas;
    pool.siguiente = 0;
    pthread_mutex_init(&pool.mutex, NULL);
    /*-----------------------------------------------------------------------
     * Lanzamiento de hilos Pthreads
     *----------------------------------------------------------------------*/
    t0 = dwalltime();
    lanzar_hilos(size, num_hilos, &pool, hilos, tiempo, idx,
                 &count_nodo[2], &count_nodo[1], &count_nodo[0]);
    /*-----------------------------------------------------------------------
     * Reducción MPI: suma de contadores de todos los procesos hacia rank 0
     *
     * Cada proceso envía sus contadores locales. El proceso 0 recibe la
     * suma global.
     *----------------------------------------------------------------------*/


    if(rank==0) {
        MPI_Reduce(count_nodo, count_total, 3, MPI_LONG, MPI_SUM, 0, MPI_COMM_WORLD);

    }else {
        MPI_Reduce(count_nodo, NULL, 3, MPI_LONG, MPI_SUM, 0, MPI_COMM_WORLD);
    }

    /*-----------------------------------------------------------------------
     * Fin de la medición de tiempo
     *----------------------------------------------------------------------*/
    t1 = dwalltime();
    tiempo_nodo += (t1 - t0);

    /*-----------------------------------------------------------------------
     * Resultado final (solo el proceso 0 calcula e imprime)
     *----------------------------------------------------------------------*/
    double max_h=-1;
    double promedio = 0;
    for(int i = 0; i < num_hilos; i++){
        //calcular promiedo de tiempo por nodo
        if(max_h<tiempo[i])
            max_h=tiempo[i];
        promedio += tiempo[i];
    }
    if (rank != 0){
        MPI_Reduce(&promedio, NULL, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
        MPI_Reduce(&max_h, NULL, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
        MPI_Reduce(&tiempo_nodo, NULL, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    }
    if (rank == 0) {
        double prom, max_t,tiempo_total,i, balance;
        long int unique = count_total[0] + count_total[1] + count_total[2];
        long int total = count_total[0]*2 + count_total[1]*4 + count_total[2]*8;
        //no lo contamos como computo porque es calculos para el informe y no para el programa
        MPI_Reduce(&promedio, &prom, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
        prom /= (num_hilos * num_procs);
        MPI_Reduce(&max_h, &max_t, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
        MPI_Reduce(&tiempo_nodo, &tiempo_total, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

        balance= prom/max_t;
        printf("N=%d  Hilos por nodo=%d  Procesos MPI=%d  Balance de carga=%.6f\n",size, num_hilos, num_procs, balance);
        printf("promedio hilo: %.6f segundos\n", prom);
        printf("maximo hilo : %.6f segundos\n", max_t);
        printf("Soluciones únicas : %ld\n", unique);
        printf("Soluciones totales: %ld\n", total);
        printf("Tiempo de ejecución: %.6f segundos\n", tiempo_total);
    }
    /*-----------------------------------------------------------------------
     * Liberación de recursos
     *----------------------------------------------------------------------*/
    free(pool.tareas);
    free(hilos);
    free(count_nodo);
    free(count_total);
    free(tiempo);
    free(idx);
    pthread_mutex_destroy(&pool.mutex);
    MPI_Finalize();
    return 0;
}