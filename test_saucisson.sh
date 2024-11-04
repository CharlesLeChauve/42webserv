# test_saucisson.sh

# Tester GET sur la page d'index
curl -v http://saucisson.slayer.fr:8081/index.html -o output_index.html

# Tester GET sur une URL inexistante pour vérifier 404
curl -v http://saucisson.slayer.fr:8081/inexistante -o output_404.html

# Tester GET sur la location /static
curl -v http://saucisson.slayer.fr:8081/static/test.html -o output_static.html
