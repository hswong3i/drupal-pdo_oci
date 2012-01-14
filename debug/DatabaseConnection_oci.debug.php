<?php

// Initialize configuration in Drupal style.
require_once './bootstrap.inc';

$cmdstr = 'SELECT last_name, salary FROM employees';
$stmt = db_query($cmdstr);
$results = $stmt->fetchAll();

$stmt = db_query("SELECT COUNT(*) AS nrows FROM ($cmdstr)");
$nrows = $stmt->fetchObject()->nrows;

echo "<html><head><title>Oracle PHP Test</title></head><body>";
echo "<center><h2>Oracle PHP Test</h2><br>";
echo "<table border=1 cellspacing='0' width='50%'>\n<tr>\n";
echo "<td><b>Name</b></td>\n<td><b>Salary</b></td>\n</tr>\n";
for ($i = 0; $i < $nrows; $i++ ) {
        echo "<tr>\n";
        echo "<td>" . $results[$i]->last_name . "</td>";
        echo "<td>$ " . number_format($results[$i]->salary, 2). "</td>";
        echo "</tr>\n";
}
echo "<tr><td colspan='2'> Number of Rows: $nrows</td></tr></table><br>";
echo "<em>If you see data, then it works!</em><br>";
echo "</center></body></html>\n";
