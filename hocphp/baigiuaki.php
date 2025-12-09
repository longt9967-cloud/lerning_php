<html><!DOCTYPE html>
<html lang="vi">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Bai kiem tra giua ki</title>
    <link rel="stylesheet" href="st.css">
</head>
<body>
    <h1>Bai kiem tra giua ki</h1>
    <?php
for ($i = 1; $i <= 100; $i++) {
    if ($i % 2 == 0) {
        echo "<span class='chan'>$i</span><br> ";
    } else {
        echo "<span class='le'>$i</span><br>";
    }
}
?>
        
</body>
</html></html>