#!/bin/bash
# Nombre d'utilisateurs simultanés
concurrent_users=100
# Nombre de répétitions par utilisateur
repetitions=5

for ((i=1; i<=concurrent_users; i++)); do
    for ((j=1; j<=repetitions; j++)); do
        # Lance une requête de téléversement en arrière-plan
        curl -X POST -F "file=@www/images/fleur.jpg" http://raclette.breaker.fr:8080/uploads &
    done
done

# Attends la fin de tous les processus en arrière-plan
wait
