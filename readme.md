//?? Probleme quand tentative d'upload les gif, probablement l'erreur maax_body n'est plus bien gérée, du coup c'est pending

//?? Utilisation du code 204 : Pour les requetes qui ont abouti mais qui n'attendent pas de réponse (comme une requete DELETE par exemple). 200 OK semble signifier que le serveur veut envoyer une réponse

//?? Dans Evals, il demande de verifier que le client est supprimé après une erreur dans un read ou dans un write, Pour le read ca à l'air nickel mais pour le write je suis pas sur : Search for all read/recv/write/send on a socket and check that, if an error is returned, the client is removed.

//?? Est ce qu'on ferait pas bien de choisir un navigateur completement eclaté comme ca on doit gérer moins de trucs juste pour rester comppatible avec notre internet explorer 1.2 

//?! Notre manière d'envoyer les réponses ne convient probablement pas. De la meme manière que les requêtes sont recues en fragmentées, ca peut être le cas pour les réponses, et alors si on essaye de tout write en une fois on obtient à nouveau un comportement bloquant. ==> L'utilisation de POLLUP semble être la solution. Comme pour les requêtes, il faut envoyer dans POLLUP, checker si on a envoyé la totalité de la réponse, remettre le fd dans POLLUP etc jusqu'a ce que toute la réponse ait été envoyée.
Après ca, il est très IMPORTANT de close la connection, sans quoi le navigateur pourrait croire que la réponse n'est pas terminée. 

//?? Du coup, on a le même probleme pour les CGI, si on essaye de lire toute la réponse du CGI, ou de lui envoyer trop de données d'un seul coup, on se retrouve encore avec un comportement bloquant. A priori, il faudrait ouvrir les pipes dédiés au CGI dès le début et gérer la lecture/ecriture vers ce pipe depuis POLL (POLLOUT pour envoyer au CGI, POLLIN pour recevoir sa réponse);

//?? Refactoriser handlegetorpost si possible