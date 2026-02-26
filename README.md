# SHELL À BASE DE C

L'objectif du projet est de produire un shell amateur.  

## Gestion des commandes internes

Nous avons un struct:

```
typedef struct {
  int (*BuiltinFunc)(char **args);
  char *BuiltinName;
} builtin;

typedef struct {
  int size;
  builtin tab[];
} builtinTab;

builtinTab Builtins {//defini dans shell.c}

``` 
La variable globale `Builtins` étant de type `builtinTab` nous permet d'associer les fonctions avec certains mots. On lit la commande avec la bibliothèque fourni `readcmd.c`, si la séquence de commande n'a qu'une seule commande (pas de pipe), le shell cherche la commande dans le struct et exécute la fonction associée.  

Les commandes associés sont:  
- `Quit`est associée à "q" et "quit".
- `Echo`est associée à "echo".
- `ChangeDir`est associée à "cd".
- `ActiveJobsList`est associée à "jobs".
- `Foreground`est associée à "fg".
- `Background`est associée à "bg".
- `Terminate`est associée à "term".
- `Stop`est associée à "stop".

## Gestion des commandes externes

Lorsqu'une séquence de commande est saisie, le shell vérifie si la première commande appartient à la liste des commandes internes`builtin`.

- Si la première commande est dans la liste des commandes internes, et qu'il n'y a pas de pipe, la sequence sera interprétée comme étant des commandes internes.
- Sinon, la séquence de commande sera interprétée comme des commandes externes.

Chaque commande externe sera un fils du shell, duppliqué via `fork`, et vont être executer via `execvp`. 

## Gestion des pipes

La gestion des pipes se fera par différentes étapes:  
- Création d'un tableau `int pipes[nbCmd -1][2]`.
- Associer le pipe numéro i avec avec le fils numéro i et le fils numéro i +1, les fils étant classer par ordre croissant lors de leur création. 

Pour chaque processus fils i:
- si i > 0, l'entrée standard , stdin, est dirigée avec la sortie du pipe i-1.
- si i < nbCmd -1, la sortie standard, stdout, est redirigée  avec l'entrée du pipe i.   


Les redirections se feront par `dup2`.

On s'assure à bien fermer les descripteurs de pipes inutles.

## Gestion des processus Zombies

Le processus père a pour rôle de récupérer tous les processus fils.
Pour cela, nous avons besoin d'un handler, `handlerSIGCHLD`qui sera associé avec le signal `SIGCHLD` au `handlerSIGCHLD`, ce signal est envoyé au processus père lorsqu'un fils change d'état.  
Le handler appelle `waitpid` avec les options `WNOHANG`, `WUNTRACED`,`WCONTINUED`: qui nous permettra de :
- ne pas bloquer l'éxecution du père.
- récupérer tous les processus fils terminés.
- revenir en cas de changement d'état du côté du fils.



