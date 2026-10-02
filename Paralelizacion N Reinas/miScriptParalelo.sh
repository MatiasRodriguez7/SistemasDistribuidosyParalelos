#!/bin/bash

# Verificamos que se hayan pasado los dos parámetros
if [ -z "$1" ] || [ -z "$2" ]; then
  echo "Error: Faltan parámetros."
  echo "Uso: ./miScriptParalelo.sh <num_reinas> <hilos_por_nodo>"
  echo "Ejemplo: ./miScriptParalelo.sh 18 2"
  exit 1
fi

REINAS=$1
HILOS=$2

# Lanzamos el trabajo a SLURM pasando las variables dinámicas
sbatch <<EOF
#!/bin/bash
#SBATCH -N 2
#SBATCH --exclusive
#SBATCH --tasks-per-node=1
#SBATCH -o ./output${REINAS}reinas_${HILOS}hilosporNodos.txt
#SBATCH -e ./errors${REINAS}reinas_${HILOS}hilosporNodos.txt
#SBATCH --time=00:08:00

mpirun --bind-to none nreinas_hibrido $REINAS $HILOS
EOF