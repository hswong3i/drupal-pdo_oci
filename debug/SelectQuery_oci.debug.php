<?php

// Initialize configuration in Drupal style.
require_once './bootstrap.inc';

$cmdstr = 'SELECT last_name, salary FROM employees';

$stmt = db_select('employees', 'e')
  ->fields('e', array('last_name', 'salary'))
  ->execute();
$results = $stmt->fetchAll();

$stmt = db_select('employees', 'e');
$stmt->addExpression('COUNT(*)', 'nrows');
$nrows = $stmt->execute()->fetchField();

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

// Test rudimentary SELECT statements.
$query = db_select('employees');
$last_name_field = $query->addField('employees', 'last_name');
$salary_field = $query->addField('employees', 'salary');
$result = $query->execute();
$num_records = 0;
foreach ($result as $record) {
  $num_records++;
}
dpr($num_records == 107 ? 'TRUE' : 'FALSE');

// Test SELECT statements with expressions.
$query = db_select('employees');
$last_name_field = $query->addField('employees', 'last_name');
$employee_id_field = $query->addExpression("employee_id*2", 'double');
$query->condition('employee_id', 100);
$result = $query->execute();
$record = $result->fetch();
dpr($record->$employee_id_field == 200 ? 'TRUE' : 'FALSE');

// Test range queries. The SQL clause varies with the database.
$query = db_select('employees');
$last_name_field = $query->addField('employees', 'last_name');
$salary_field = $query->addField('employees', 'salary');
$query->range(0, 2);
$result = $query->execute();
$num_records = 0;
foreach ($result as $record) {
    $num_records++;
}
dpr($num_records == 2 ? 'TRUE' : 'FALSE');
