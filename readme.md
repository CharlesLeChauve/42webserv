
//?? Utilisation du code 204 : Pour les requetes qui ont abouti mais qui n'attendent pas de réponse (comme une requete DELETE par exemple). 200 OK semble signifier que le serveur veut envoyer une réponse

//?? In tester : 
		- any file with .bla as extension must answer to POST request by calling the cgi_test executable

Du coup il semblerait qu'il faille définir dès le fichier de configuration qui éxecute quoi pour les cgi ^^

//?? Je sais pas trop ce qu'il doit se passer mais vu ce que me demande le testeur, je pense aussi qu'il y a un probleme sur nos location ou sur la manière dont on décide not found. Si je crée une location /directory (qui n'existe pas), mais que je lui définis root = <un dossier qui existe>, c'est censé fonctionner je pense, et la ca renvoit not found...
