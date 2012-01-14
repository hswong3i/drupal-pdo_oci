<?php

function dprint_r($input) {
  print "<pre>";
  print_r($input);
  print "</pre>";
}

// Clone from index.php.
// This file is under includes/database/oci/debug
define('DRUPAL_ROOT', getcwd() . '../../../../../');
require_once DRUPAL_ROOT . '/includes/bootstrap.inc';

// Clone from drupal_bootstrap().
// Just initialize basic settings.
_drupal_bootstrap_configuration();

// NOTE: We skip _drupal_bootstrap_page_cache() here, because just hope to
// debug database driver development.
//_drupal_bootstrap_page_cache();

// Clone from _drupal_bootstrap_database().
// We don't call _drupal_bootstrap_database() directly since don't need to 
// pass to installer or from simpletest.
require_once DRUPAL_ROOT . '/includes/database/database.inc';
spl_autoload_register('drupal_autoload_class');
spl_autoload_register('drupal_autoload_interface');

// Fake the database connection setting to sample database.
$databases = array (
  'default' =>
  array (
    'default' =>
    array (
      'database' => 'AL32UTF8.localdomain',
      'username' => 'HR',
      'password' => 'CHANGE',
      'host' => 'localhost.localdomain',
      'port' => '',
      'driver' => 'oci',
      'prefix' => '',
    ),
  ),
);

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
