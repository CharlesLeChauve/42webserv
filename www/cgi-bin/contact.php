<?php
ini_set('display_errors', 1);
ini_set('display_startup_errors', 1);
error_reporting(E_ALL);

// Inclure PHPMailer
require 'PHPMailer/src/PHPMailer.php';
require 'PHPMailer/src/SMTP.php';
require 'PHPMailer/src/Exception.php';

use PHPMailer\PHPMailer\PHPMailer;
use PHPMailer\PHPMailer\Exception;

if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    // Récupérer et sécuriser les données du formulaire
    $nom = htmlspecialchars($_POST['nom']);
    $prenom = htmlspecialchars($_POST['prénom']);
    $email = htmlspecialchars($_POST['email']);
    $tel = htmlspecialchars($_POST['tel']);
    $sujet = htmlspecialchars($_POST['sujet']);
    $message = htmlspecialchars($_POST['message']);

    // Préparer le contenu de l'email
    $body = "Nom: $nom\n";
    $body .= "Prénom: $prenom\n";
    $body .= "Email: $email\n";
    $body .= "Téléphone: $tel\n";
    $body .= "Message:\n$message\n";

    $mail = new PHPMailer(true);

    try {
        // Paramètres du serveur SMTP
        $mail->isSMTP();
        $mail->Host = 'smtp.gmail.com'; // Remplacez par le serveur SMTP de votre fournisseur
        $mail->SMTPAuth = true;
        $mail->Username = 'teddy.gibert@gmail.com'; // Votre adresse email SMTP
        $mail->Password = 'cvxyutavnlmpwciy'; // Votre mot de passe SMTP ou mot de passe d'application
        $mail->SMTPSecure = 'tls'; // 'tls' ou 'ssl' selon votre fournisseur
        $mail->Port = 587; // 587 pour 'tls', 465 pour 'ssl'

        // Destinataires
        $mail->setFrom($email, "$nom $prenom"); // L'email de l'expéditeur (l'utilisateur qui remplit le formulaire)
        $mail->addAddress('anporced@student.42perpignan.fr', 'Teddy Gibert'); // Votre adresse email où vous recevrez le message

        // Contenu de l'email
        $mail->isHTML(false); // Paramétrer sur true si vous envoyez un email au format HTML
        $mail->Subject = $sujet;
        $mail->Body    = $body;

        $mail->send();
        echo "<html><body>";
        echo "<h1>Merci pour votre message!</h1>";
        echo "<p>Votre message a été envoyé avec succès.</p>";
        echo "</body></html>";
    } catch (Exception $e) {
        echo "<html><body>";
        echo "<h1>Erreur</h1>";
        echo "<p>Le message n'a pas pu être envoyé. Erreur : {$mail->ErrorInfo}</p>";
        echo "</body></html>";
    }
} else {
    // Afficher le formulaire de contact
    echo '<html><body>';
	echo "<p>Veuillez soumettre le formulaire.</p>";
    echo '</body></html>';
}
?>
