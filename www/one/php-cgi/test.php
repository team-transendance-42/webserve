#!/usr/bin/php-cgi
<?php

header('Content-Type: text/plain');

echo "PHP CGI is working!\n\n";

echo "PHP Version: " . PHP_VERSION . "\n";
echo "Server Software: " . ($_SERVER['SERVER_SOFTWARE'] ?? 'Unknown') . "\n";
echo "Request Method: " . ($_SERVER['REQUEST_METHOD'] ?? 'Unknown') . "\n";
echo "Script Name: " . ($_SERVER['SCRIPT_NAME'] ?? 'Unknown') . "\n";

?>