//?? SUJET : • Il doit être non bloquant et n’utiliser qu’un seul poll() (ou équivalent) pour
toutes les opérations entrées/sorties entre le client et le serveur (listen inclus).
Le listen inclus m'interroge... Le notre est pas du tout géré par poll mais je vois pas trop


//?? SUJET : — Le CGI doit être exécuté dans le bon répertoire pour l’accès au fichier de
chemin relatif.
Peut-être est-ce là le problème qui nous empêche d'envoyer l'image correctement quand on est dans le dossier cgi-bin du CGI
