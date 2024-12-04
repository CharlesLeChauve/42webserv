
//?? Utilisation du code 204 : Pour les requetes qui ont abouti mais qui n'attendent pas de réponse (comme une requete DELETE par exemple). 200 OK semble signifier que le serveur veut envoyer une réponse

//?? In tester :
		- any file with .bla as extension must answer to POST request by calling the cgi_test executable

Du coup il semblerait qu'il faille définir dès le fichier de configuration qui éxecute quoi pour les cgi ^^

//?? Je sais pas trop ce qu'il doit se passer mais vu ce que me demande le testeur, je pense aussi qu'il y a un probleme sur nos location ou sur la manière dont on décide not found. Si je crée une location /directory (qui n'existe pas), mais que je lui définis root = <un dossier qui existe>, c'est censé fonctionner je pense, et la ca renvoit not found...
cf suejt : ◦ Définir un répertoire ou un fichier à partir duquel le fichier doit être recherché
(par exemple si l’url /kapouet est rootée sur /tmp/www, l’url /kapouet/pouic/toto/pouet
est /tmp/www/pouic/toto/pouet).

//?? SUJET : • Il doit être non bloquant et n’utiliser qu’un seul poll() (ou équivalent) pour
toutes les opérations entrées/sorties entre le client et le serveur (listen inclus).
Le listen inclus m'interroge... Le notre est pas du tout géré par poll mais je vois pas trop

//?? Voir ce que cette phrase veut dire : — Parce que vous n’allez pas appeler le CGI mais utiliser directement le chemin
complet comme PATH_INFO.
Je suis vraiment pas sûr...

//?? SUJET : — Le CGI doit être exécuté dans le bon répertoire pour l’accès au fichier de
chemin relatif.
Peut-être est-ce là le problème qui nous empêche d'envoyer l'image correctement quand on est dans le dossier cgi-bin du CGI
