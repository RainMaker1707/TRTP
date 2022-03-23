# TRTP

### Introduction
Projet du cours de Reseau LINFO1341 à l'UCLouvain en Q2, 21-22.\
Ce projet a pour but de réimplémenter le protocol TRTP au dessus 
d'un protocol UDP classique fournis par le systême de socket.\

### Makefile commandes utiles
``make`` lance la compilation de toutes les dépendances et du sender et receiver  
``make sender`` lance la compilation du sender et de ses dépendances \
``make receiver`` lance la compilation du receiver et de ses dépendances \
``make run_sender`` lance l'unique test de passage d'un ficheir de trois packet \
``make run_reiceiver`` lance le client qui doit récupérer ce dit fichier


### Bug connus
Pour run l'unique test actuel, utilisez les commandes suivante dans cet ordre:\
`make run_reiceiver`\
`make run_sender`\
Il existe encore un bug de segmentation fault lors du run du sender avant le receiver,
celui ci devrait etre corrigé sous peu