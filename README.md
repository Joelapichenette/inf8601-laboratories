# INF8601 Laboratories

Ce dépôt GitHub contient les laboratoires du cours INF8601.

## Structure du dépôt

- `Lab1/` : Ce laboratoire est une introduction à la programmation parallèle. On y met en place un pipeline de traitement d'image. Pour cela, on utilise d'abord les librairies POSIX Threads (pthreads) puis les librairies Thread Building Blocks (TBB).
- `Lab2/` : Ce laboratoire est destiné à la conversion d'un calcul sériel en calcul parallèle via les librairies OpenMP et OpenCL. On y affiche un dessin coloré dont l'animation est régie par une équation simple.
- `Lab3/` : Ce laboratoire permet de se familiariser avec le standard MPI. On y adapte un problème de diffusion thermique via une grille de propogation, et le travail est partagé entre plusieurs nœuds.

## Instructions

1. Clonez le dépôt sur votre machine locale :
    ```bash
    git clone https://github.com/votre-utilisateur/inf8601-laboratories.git
    ```
