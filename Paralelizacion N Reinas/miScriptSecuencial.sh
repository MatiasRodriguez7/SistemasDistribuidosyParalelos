#!/bin/bash

# Verificamos que se haya pasado un parámetro
if [ -z "$1" ]; then
  echo "Debes ingresar un número. Ejemplo: ./miScriptSecuencial.sh 8"
  exit 1
fi

N=$1

# Lanzamos el trabajo a SLURM pasando las variables dinámicas
sbatch <<EOF
#!/bin/bash
#SBATCH -N 1
#SBATCH --exclusive
#SBATCH -o ./output${N}reinas_secuencial.txt
#SBATCH -e ./errors${N}reinas_secuencial.txt
#SBATCH --time=00:10:00

./nreinas $N
EOF