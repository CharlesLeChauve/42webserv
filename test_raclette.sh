# test_raclette.sh

# Tester GET sur la page d'index
curl -v http://raclette.breaker.fr:8080/index.html -o output_index.html

# Tester GET sur une URL inexistante pour vérifier 404
curl -v http://raclette.breaker.fr:8080/inexistante -o output_404.html

# Tester le proxy avec l’API
curl -v http://raclette.breaker.fr:8080/api -o output_api.html

# Tester POST sur un script CGI
curl -v -X POST http://raclette.breaker.fr:8080/cgi-bin/test.cgi -d "param=value" -o output_cgi_post.html

# Tester une méthode non autorisée (405) sur un CGI avec GET
curl -v -X GET http://raclette.breaker.fr:8080/cgi-bin/test.cgi -o output_cgi_get.html
