#include "Pthreads_source.h"

double dwalltime(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

//VARIABLES COMPARTIDAS
int size;
Pool *pool;
double *tiempo;
pthread_mutex_t count_mutex;
long int *id_threads;
long int *count8;
long int *count4;
long int *count2;

/*===========================================================================
 * SECCIÓN 1: Check() — verificación de solución canónica
 *
 * Recibe un tablero completo y decide si es la solución "representante"
 * de su grupo de rotaciones (la de menor peso lexicográfico).
 *
 * Estrategia: compara el tablero actual contra sus rotaciones de 90°,
 * 180° y 270° fila por fila. Si alguna rotación resulta ser "menor"
 * que el tablero actual, lo descarta (esa rotación será encontrada
 * y contada por otra rama del backtracking). Si ninguna es menor,
 * este tablero es el canónico y se incrementa el contador correspondiente.
 *
 * Los parámetros de simetría (BOARD1, BOARD2, ENDBIT) se pasan por
 * argumento en lugar de usar globales, para que cada hilo trabaje con
 * sus propios datos sin interferencia entre hilos.
 *=========================================================================*/
static void Check(int *BOARD, int *BOARD1, int *BOARD2, int ENDBIT,
                  int TOPBIT, int SIZEE, long int *c8, long int *c4, long int *c2)
{
    int *own, *you, bit, ptn;

    /* BOARDE apunta a la última fila del tablero */
    int *BOARDE = &BOARD[SIZEE];

    /*-----------------------------------------------------------------------
     * Test de rotación 90°
     *
     * Condición de entrada: la reina en BOARD[BOUND2] debe estar en la
     * columna 0 (bit = 1). Si no, la rotación de 90° no puede coincidir
     * con ninguna solución anterior, y se omite el test.
     *----------------------------------------------------------------------*/
    if (*BOARD2 == 1) {

        /* Recorre el tablero actual (own) comparándolo fila a fila
         * con el tablero rotado 90°.
         * ptn: columna esperada en el tablero rotado para esta fila.
         *      Arranca en 2 (columna 1) y se desplaza a la izquierda
         *      (ptn <<= 1) a medida que own avanza hacia abajo. */
        for (ptn = 2, own = BOARD + 1; own <= BOARDE; own++, ptn <<= 1) {

            /* bit: columna equivalente en el tablero rotado.
             * Se calcula recorriendo BOARDE hacia arriba (you--) hasta
             * encontrar la fila cuya reina está en la columna ptn.
             * Cada paso de you equivale a un desplazamiento de columna. */
            bit = 1;
            for (you = BOARDE; *you != ptn && *own >= bit; you--)
                bit <<= 1;

            /* Si la columna actual es mayor que la rotada:
             * el tablero actual es el canónico (llega primero en la
             * búsqueda). Continúa sin descartar. */
            if (*own > bit) return;

            /* Si la columna actual es menor que la rotada:
             * la rotación de 90° es "menor" → este tablero NO es el
             * canónico. La rotación será encontrada y contada después.
             * Sale del for sin llegar a own > BOARDE. */
            if (*own < bit) break;

            /* Si son iguales: esta fila coincide, seguir comparando. */
        }

        /* Si el for terminó sin break (own superó BOARDE), todas las
         * filas coincidieron exactamente: el tablero rotado 90° es
         * idéntico al original. Simetría de orden 4 → vale ×2. */
        if (own > BOARDE) { //Symmetry of order 4
            (*c2)++;
            return;
        }
    }

    /*-----------------------------------------------------------------------
     * Test de rotación 180°
     *
     * Condición de entrada: la reina en la última fila (BOARDE) debe
     * estar en la columna ENDBIT. ENDBIT es el bit esperado según la
     * iteración del bucle exterior en NQueens().
     *----------------------------------------------------------------------*/
    if (*BOARDE == ENDBIT) {

        /* Compara el tablero actual (own, avanza hacia abajo)
         * con el tablero rotado 180° (you, avanza hacia arriba desde
         * la penúltima fila). La rotación 180° espeja tanto filas
         * como columnas simultáneamente. */
        for (you = BOARDE - 1, own = BOARD + 1; own <= BOARDE; own++, you--) {

            /* Convierte la columna de *you a su espejo horizontal:
             * ptn arranca en TOPBIT y baja (>>=) mientras bit sube (<<=).
             * Cuando ptn coincide con *you, bit contiene la columna espejada. */
            bit = 1;
            for (ptn = TOPBIT; ptn != *you && *own >= bit; ptn >>= 1)
                bit <<= 1;

            if (*own > bit) return;  /* actual es canónico, continúa    */
            if (*own < bit) break;   /* rotación es menor, descarta     */
        }

        /* Todas las filas coinciden: rotación 180° es idéntica al original.
         * Simetría de orden 2 → vale ×4. */
        if (own > BOARDE) { 
            (*c4)++;
            return;
        }
    }

    /*-----------------------------------------------------------------------
     * Test de rotación 270°
     *
     * Condición de entrada: la reina en BOARD[BOUND1] debe estar en la
     * columna N-1 (TOPBIT).
     *----------------------------------------------------------------------*/
    if (*BOARD1 == TOPBIT) {

        /* Similar al test de 90° pero recorre you desde el inicio
         * (you++) en lugar del final, y ptn baja desde TOPBIT>>1. */
        for (ptn = TOPBIT >> 1, own = BOARD + 1; own <= BOARDE; own++, ptn >>= 1) {
            bit = 1;
            for (you = BOARD; *you != ptn && *own >= bit; you++)
                bit <<= 1;

            if (*own > bit) return;  /* actual es canónico, continúa    */
            if (*own < bit) break;   /* rotación es menor, descarta     */
        }
        /* Si llegamos aquí sin break, la rotación 270° es menor: descarta.
         * (No hay count++ aquí porque si 270° coincide exactamente
         * implica que 90° también coincidiría, caso ya cubierto arriba.) */
    }

    /* Ninguna rotación resultó menor ni idéntica:
     * este tablero es el canónico de su grupo de 8. Vale ×8. */
    (*c8)++;
}

/*===========================================================================
 * SECCIÓN 2: Backtrack1 — caso reina en esquina superior izquierda
 *
 * La primera reina está fija en (fila 0, columna 0).
 * Las soluciones encontradas aquí son siempre únicas respecto a sus
 * rotaciones: es matemáticamente imposible que una rotación de un tablero
 * con la reina en la esquina tenga también una reina en esa misma esquina.
 * Por eso se cuenta directamente como COUNT8 sin llamar a Check().
 *
 * Parámetros:
 *   y           : fila actual que se intenta poblar
 *   left        : bitmask de columnas amenazadas por diagonales izquierdas
 *   down        : bitmask de columnas directamente ocupadas (ataque vertical)
 *   right       : bitmask de columnas amenazadas por diagonales derechas
 *   BOUND1      : columna de la segunda reina fija; define hasta qué fila
 *                 se aplica la restricción de columna 1
 *   BOARD       : tablero local del hilo (array de bitmasks por fila)
 *=========================================================================*/
static void Backtrack1(int y, int left, int down, int right,
                       int BOUND1, int *BOARD,
                       int SIZEE, int MASK, int TOPBIT, long int *c8)
{
    int bitmap, bit;

    /* Calcula las columnas libres en la fila y:
     * - (left | down | right): todas las columnas amenazadas
     * - ~(...): complemento → columnas NO amenazadas
     * - & MASK: descarta bits fuera del rango [0, N-1] */
    bitmap = MASK & ~(left | down | right);

    if (y == SIZEE) {
        /*-------------------------------------------------------------------
         * Última fila: si hay alguna columna libre, hay solución.
         * En la última fila solo puede quedar libre la columna exacta
         * que completa el tablero válido.
         *-----------------------------------------------------------------*/
        if (bitmap) {
            BOARD[y] = bitmap;  /* guarda la posición de la última reina   */
            (*c8)++;        /* cuenta directamente ×8, sin Check()     */
        }
    } else {
        /*-------------------------------------------------------------------
         * Restricción de columna 1 para filas anteriores a BOUND1:
         * si y < BOUND1, la reina no puede ir en la columna 1 (bit = 2).
         * Esto evita generar soluciones simétricas de las que el bloque
         * de Backtrack2 ya contabiliza.
         *
         * Truco de bits para forzar el bit 1 a 0 sin condicional:
         *   bitmap |= 2   → enciende el bit 1 (aunque ya estuviera encendido)
         *   bitmap ^= 2   → lo apaga con XOR (si estaba encendido lo apaga,
         *                   si estaba apagado lo enciende y luego lo apaga)
         * Resultado neto: el bit 1 queda siempre en 0.
         *-----------------------------------------------------------------*/
        if (y < BOUND1) {
            bitmap |= 2;
            bitmap ^= 2;
        }

        /*-------------------------------------------------------------------
         * Prueba cada columna libre disponible en la fila y.
         *-----------------------------------------------------------------*/
        while (bitmap) {
            /* Extrae el bit más bajo de bitmap (la columna más a la
             * izquierda disponible) y lo elimina de bitmap para no
             * repetirlo en la próxima iteración.
             *
             * Truco: -bitmap en complemento a 2 da ~bitmap + 1.
             * (-bitmap) & bitmap aísla exactamente el bit más bajo. */
            bitmap ^= BOARD[y] = bit = -bitmap & bitmap;

            /* Recursión a la siguiente fila con las amenazas actualizadas:
             * - (left | bit) << 1 : la diagonal izquierda se propaga una
             *                       columna a la derecha al bajar una fila
             * - down | bit        : la columna 'bit' queda ocupada
             * - (right | bit) >> 1: la diagonal derecha se propaga una
             *                       columna a la izquierda al bajar una fila */
            Backtrack1(y + 1,
                       (left  | bit) << 1,
                        down  | bit,
                       (right | bit) >> 1,
                       BOUND1, BOARD, SIZEE, MASK, TOPBIT,
                       c8);
        }
    }
}

/*===========================================================================
 * SECCIÓN 3: Backtrack2 — caso reina en columna interior
 *
 * La primera reina está en una columna interior (no en la esquina).
 * Al llegar a la última fila se llama a Check() para determinar si
 * la solución encontrada es la representante canónica de su grupo
 * de rotaciones, y en ese caso cuánto vale (×2, ×4 u ×8).
 *
 * Además de las amenazas (left, down, right), recibe los parámetros
 * de simetría del bucle exterior de NQueens():
 *   BOUND1/BOUND2 : franjas donde se aplican restricciones de borde
 *   LASTMASK      : columnas prohibidas para la última reina
 *   ENDBIT        : columna esperada en la última fila (simetría 180°)
 *   SIDEMASK      : bitmask de columnas borde (TOPBIT | 1)
 *=========================================================================*/
static void Backtrack2(int y, int left, int down, int right,
                       int BOUND1, int BOUND2,
                       int LASTMASK, int ENDBIT, int SIDEMASK,
                       int *BOARD, int *BOARD1, int *BOARD2,
                       int SIZEE, int MASK, int TOPBIT,
                       long int *c8, long int *c4, long int *c2)
{
    int bitmap, bit;

    /* Calcula columnas libres en la fila y (igual que en Backtrack1) */
    bitmap = MASK & ~(left | down | right);

    if (y == SIZEE) {
        /*-------------------------------------------------------------------
         * Última fila: verificar que la posición libre no esté bloqueada
         * por LASTMASK antes de llamar a Check().
         *
         * LASTMASK acumula las columnas que ya fueron usadas como primera
         * reina en iteraciones anteriores del bucle exterior de NQueens().
         * Si la última reina cae en una de esas columnas, la solución sería
         * un duplicado de otra ya contada → se descarta.
         *-----------------------------------------------------------------*/
        if (bitmap) {
            if (!(bitmap & LASTMASK)) {  /* la columna libre no está bloqueada */
                BOARD[y] = bitmap;
                /* Check() verifica si este tablero es el canónico de su
                 * grupo de rotaciones y actualiza el contador correcto */
                Check(BOARD, BOARD1, BOARD2, ENDBIT, TOPBIT, SIZEE,
                      c8, c4, c2);
            }
        }
    } else {
        /*-------------------------------------------------------------------
         * Restricción de columnas borde para filas anteriores a BOUND1:
         * si y < BOUND1, se prohíben las columnas 0 y N-1 (SIDEMASK).
         * Mismo truco de bits que en Backtrack1 pero con SIDEMASK.
         *-----------------------------------------------------------------*/
        if (y < BOUND1) {
            bitmap |= SIDEMASK;
            bitmap ^= SIDEMASK;

        } else if (y == BOUND2) {
            /*---------------------------------------------------------------
             * Restricción especial en la fila BOUND2 (fila simétrica):
             * garantiza que la solución cruce el eje de simetría lateral.
             *
             * Si ninguna reina anterior ocupó una columna borde
             * (down & SIDEMASK == 0): imposible completar una solución
             * válida bajo las restricciones de simetría → poda.
             *-------------------------------------------------------------*/
            if (!(down & SIDEMASK)) return;

            /* Si solo uno de los dos bordes fue ocupado (no ambos):
             * la reina en BOUND2 DEBE ir al borde opuesto.
             * Se restringe bitmap a solo las columnas de borde. */
            if ((down & SIDEMASK) != SIDEMASK) bitmap &= SIDEMASK;
        }

        /*-------------------------------------------------------------------
         * Prueba cada columna libre (mismo mecanismo que en Backtrack1)
         *-----------------------------------------------------------------*/
        while (bitmap) {
            /* Extrae el bit más bajo y lo elimina de bitmap */
            bitmap ^= BOARD[y] = bit = -bitmap & bitmap;

            /* Recursión con amenazas propagadas a la siguiente fila */
            Backtrack2(y + 1,
                       (left  | bit) << 1,
                        down  | bit,
                       (right | bit) >> 1,
                       BOUND1, BOUND2,
                       LASTMASK, ENDBIT, SIDEMASK,
                       BOARD, BOARD1, BOARD2,
                       SIZEE, MASK, TOPBIT,
                       c8, c4, c2);
        }
    }
}

/*===========================================================================
 * SECCIÓN 4: Función de hilo
 *
 * Cada hilo toma tareas del pool con un mutex y las ejecuta.
 * Acumula los resultados en sus contadores locales (sin sincronización
 * durante el cómputo). Al terminar todas las tareas, retorna.
 *=========================================================================*/
static void *funcion_hilo(void *arg)
{
    double t_ini = dwalltime();
    long int c8 = 0;
    long int c4 = 0;
    long int c2 = 0;

    int id=*(int*)arg;
    int SIZE = size;
    int SIZEE = SIZE - 1;
    int MASK  = (1 << SIZE) - 1;
    int TOPBIT = 1 << SIZEE;  
    int idx;


    /* Tablero local: cada hilo tiene el suyo para evitar condiciones de carrera */
    int BOARD[MAXSIZE];

    /* Tomar la siguiente tarea del pool */
    pthread_mutex_lock(&(pool->mutex));
    idx = pool->siguiente;
    while (idx < pool->total) {
        pool->siguiente++;
        pthread_mutex_unlock(&(pool->mutex));
        Tarea *t = &(pool->tareas[idx]);

        /* Restaurar las dos primeras filas fijas de la tarea */
        BOARD[0] = t->board0;
        BOARD[1] = t->board1;

        if (t->tipo == TIPO_BT1) {
            /*
             * Backtrack1: reina en esquina.
             * Arranca en y_inicio (fila 2 o 3) con las amenazas ya propagadas.
             */
            Backtrack1(t->y_inicio,
                       t->left, t->down, t->right,
                       t->bound1,
                       BOARD, SIZEE, MASK, TOPBIT, &c8);
        } else {
            /*
             * Backtrack2: reina en columna interior.
             * Requiere además los parámetros de simetría de la tarea.
             */
            int *BOARD1 = &BOARD[t->bound1];
            int *BOARD2 = &BOARD[t->bound2];

            Backtrack2(t->y_inicio,
                       t->left, t->down, t->right,
                       t->bound1, t->bound2,
                       t->lastmask, t->endbit, t->sidemask,
                       BOARD, BOARD1, BOARD2, SIZEE, MASK, TOPBIT,
                       &c8, &c4, &c2);
        }
        pthread_mutex_lock(&(pool->mutex));
        idx = pool->siguiente;
    }
    pthread_mutex_unlock(&(pool->mutex));

    // Sumar contadores locales a los globales con protección de mutex
    pthread_mutex_lock(&count_mutex);
    *count8 += c8;
    *count4 += c4;
    *count2 += c2;
    pthread_mutex_unlock(&count_mutex);

    // Guardar el tiempo total de ejecución del hilo
    tiempo[id] = dwalltime() - t_ini;
    pthread_exit(NULL);
}

/*===========================================================================
 * SECCIÓN 6: lanzar_hilos
 *
 * Crea num_hilos hilos, espera que terminen y suma sus contadores locales.
 *=========================================================================*/
void lanzar_hilos(int size_arg, int num_hilos, Pool *pool_arg,
                 pthread_t *hilos, double *time, long int *id,
                 long int *count8_arg, long int *count4_arg, long int *count2_arg)
{    /* Inicializar argumentos de cada hilo */
    size=size_arg;
    pool=pool_arg;
    tiempo=time;
    id_threads=id;
    count8 = count8_arg;
    count4 = count4_arg;
    count2 = count2_arg;

    pthread_mutex_init(&count_mutex, NULL);
    *count8 = 0;
    *count4 = 0;
    *count2 = 0;

    /* Lanzar hilos */
    for (int i = 0; i < num_hilos; i++){
        id_threads[i]=i;
        pthread_create(&hilos[i], NULL, &funcion_hilo, (void *)&id_threads[i]);
    }
    /* Esperar a que todos terminen */
    for (int i = 0; i < num_hilos; i++)
        pthread_join(hilos[i], NULL);

    pthread_mutex_destroy(&count_mutex); // Destruir mutex de contadores
}