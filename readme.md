
//?? Utilisation du code 204 : Pour les requetes qui ont abouti mais qui n'attendent pas de réponse (comme une requete DELETE par exemple). 200 OK semble signifier que le serveur veut envoyer une réponse

//?? In tester :
		- any file with .bla as extension must answer to POST request by calling the cgi_test executable

Du coup il semblerait qu'il faille définir dès le fichier de configuration qui éxecute quoi pour les cgi ^^

//?? SUJET : • Il doit être non bloquant et n’utiliser qu’un seul poll() (ou équivalent) pour
toutes les opérations entrées/sorties entre le client et le serveur (listen inclus).
Le listen inclus m'interroge... Le notre est pas du tout géré par poll mais je vois pas trop

//?? Voir ce que cette phrase veut dire : — Parce que vous n’allez pas appeler le CGI mais utiliser directement le chemin
complet comme PATH_INFO.
Je suis vraiment pas sûr...

//?? SUJET : — Le CGI doit être exécuté dans le bon répertoire pour l’accès au fichier de
chemin relatif.
Peut-être est-ce là le problème qui nous empêche d'envoyer l'image correctement quand on est dans le dossier cgi-bin du CGI
