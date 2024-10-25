<?php

ini_set('display_errors', 1);
ini_set('display_startup_errors', 1);
error_reporting(E_ALL);

echo "<html><body><h1>Merci pour votre message!</h1>";

if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    // Récupérer et sécuriser les données du formulaire
    $nom = htmlspecialchars($_POST['nom']);
    $prenom = htmlspecialchars($_POST['prénom']);
    $email = htmlspecialchars($_POST['email']);
    $tel = htmlspecialchars($_POST['tel']);
    $sujet = htmlspecialchars($_POST['sujet']);
    $message = htmlspecialchars($_POST['message']);

    // Afficher les données reçues
    echo "<p><strong>Nom :</strong> $nom</p>";
    echo "<p><strong>Prénom :</strong> $prenom</p>";
    echo "<p><strong>Email :</strong> $email</p>";
    echo "<p><strong>Téléphone :</strong> $tel</p>";
    echo "<p><strong>Sujet :</strong> $sujet</p>";
    echo "<p><strong>Message :</strong><br>" . nl2br($message) . "</p>";
} else {
    // Si la requête n'est pas en POST, afficher un message ou rediriger
    echo "<p>Veuillez soumettre le formulaire.</p>";
}

echo "</body></html>";
?>