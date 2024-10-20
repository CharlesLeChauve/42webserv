#!/bin/bash
echo "Content-Type: text/html"
echo ""

echo "<html><body><h1>Hello, CGI World!</h1>"

if [ "$REQUEST_METHOD" = "GET" ]; then
    echo "<p>Query String: $QUERY_STRING</p>"
    # Extraire la valeur du paramètre 'name' en utilisant awk
    name=$(echo "$QUERY_STRING" | awk -F'=' '/name/ {print $2}')
    echo "<p>Parameter 'name': $name</p>"
fi

if [ "$REQUEST_METHOD" = "POST" ]; then
    read POST_DATA
    echo "<p>Post Data: $POST_DATA</p>"
fi

echo "</body></html>"
