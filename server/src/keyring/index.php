<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>KeyRing for Studentmachine</title>
</head>
<body>

    <h1>Check your public key</h1>

    <form action="" method="post">
        <label for="filename">Your login on github:</label>
        <input type="text" id="filename" name="filename" required>
        <button type="submit">Show</button>
    </form>

    <?php
    // Obsługa formularza
    if ($_SERVER["REQUEST_METHOD"] == "POST") {
        // Pobieranie nazwy pliku z formularza
        $filename = $_POST["filename"].".pub";

        // Sprawdzenie czy plik istnieje
        if (file_exists($filename)) {
            // Wyświetlanie zawartości pliku
            $content = file_get_contents($filename);
            echo "<h2>Your key $filename:</h2>";
            echo "<pre>$content</pre>";
        } else {
            echo "<p style='color: red;'>Key $filename not found.</p>";
        }
    }
    ?>

</body>
</html>

