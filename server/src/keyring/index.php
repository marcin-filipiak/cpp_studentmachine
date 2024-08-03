<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>KeyRing for Studentmachine</title>
    <style>
        /* Resetowanie marginesów i paddingu */
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
        }

        /* Tło i główny kolor */
        body {
            background-color: #1e1e1e;
            color: #f0f0f0;
            line-height: 1.6;
            display: flex;
            justify-content: center;
            align-items: flex-start;
            min-height: 100vh;
            padding: 20px;
        }

        /* Kontener na formularz i odpowiedź serwera */
        .container {
            width: 100%;
            max-width: 400px;
        }

        /* Stylizacja formularza */
        form {
            background-color: #2b2b2b;
            padding: 20px;
            border-radius: 8px;
            box-shadow: 0 2px 10px rgba(0, 0, 0, 0.5);
            width: 100%;
            margin-bottom: 20px;
        }

        /* Nagłówki */
        h1 {
            font-size: 24px;
            margin-bottom: 20px;
            color: #f0f0f0;
            text-align: center;
        }

        h2 {
            font-size: 20px;
            margin-top: 20px;
            color: #f0f0f0;
        }

        /* Pola tekstowe i przyciski */
        label {
            display: block;
            margin-bottom: 8px;
            font-weight: bold;
            color: #cccccc;
        }

        input[type="text"] {
            width: 100%;
            padding: 10px;
            margin-bottom: 20px;
            border: 1px solid #444;
            border-radius: 4px;
            font-size: 16px;
            background-color: #333;
            color: #f0f0f0;
        }

        button[type="submit"] {
            width: 100%;
            padding: 10px;
            background-color: #007bff;
            border: none;
            border-radius: 4px;
            color: #f0f0f0;
            font-size: 16px;
            cursor: pointer;
            transition: background-color 0.3s ease;
        }

        button[type="submit"]:hover {
            background-color: #0056b3;
        }

        /* Stylowanie odpowiedzi serwera */
        #yourkey, #notfound {
            background-color: #2b2b2b;
            padding: 20px;
            border-radius: 8px;
            box-shadow: 0 2px 10px rgba(0, 0, 0, 0.5);
            margin-top: 20px;
            width: 100%;
        }

        textarea {
            width: 100%;
            padding: 10px;
            margin-top: 10px;
            border: 1px solid #444;
            border-radius: 4px;
            font-family: 'Courier New', Courier, monospace;
            font-size: 14px;
            height: 200px;
            resize: vertical;
            background-color: #333;
            color: #f0f0f0;
        }

        /* Komunikat o błędzie */
        p {
            color: #e74c3c;
            font-weight: bold;
        }

    </style>
</head>
<body>

    <div class="container">
        <form action="" method="post">
            <h1>Check your public key</h1>
            <label for="filename">Your login on GitHub:</label>
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
                echo "<div id=\"yourkey\"><h2>Your key $filename:</h2>";
                echo "<textarea readonly>$content</textarea></div>";
            } else {
                echo "<div id=\"notfound\"><p>Key $filename not found.</p></div>";
            }
        }
        ?>
    </div>

</body>
</html>
