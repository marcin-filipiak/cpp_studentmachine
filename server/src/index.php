<?php
if ($_FILES["data"]["name"] != "") {
    $tmp_name = $_FILES["data"]["tmp_name"];
    $destination = "keyring/".$_FILES["data"]["name"];; 
    move_uploaded_file($tmp_name, $destination);
    echo "Keyring say that everything is ok";
} else {
    echo "Something wrong on keyring server";
}
?>

