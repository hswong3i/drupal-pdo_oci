<?php

// Initialize configuration in Drupal style.
require_once './bootstrap.inc';

/**
 * Dummy class for fetching into a class.
 *
 * PDO supports using a new instance of an arbitrary class for records
 * rather than just a stdClass or array. This class is for testing that
 * functionality. (See testQueryFetchClass() below)
 */
class FakeRecord { }

/**
 * Test the very basic insert functionality.
 */
dpr("function testSimpleInsert()");
$num_records_before = db_query('SELECT COUNT(*) FROM {test}')->fetchField();
$query = db_insert('test');
$query->fields(array(
  'name' => 'Yoko',
  'age' => '29',
));
$query->execute();
$num_records_after = db_query('SELECT COUNT(*) FROM {test}')->fetchField();
assertIdentical($num_records_before + 1, (int) $num_records_after, t('Record inserts correctly.'));
$saved_age = db_query('SELECT age FROM {test} WHERE name = :name', array(':name' => 'Yoko'))->fetchField();
assertIdentical($saved_age, '29', t('Can retrieve after inserting.'));

/**
 * Test that we can insert multiple records in one query object.
 */
dpr("function testMultiInsert()");
$num_records_before = (int) db_query('SELECT COUNT(*) FROM {test}')->fetchField();
$query = db_insert('test');
$query->fields(array(
  'name' => 'Larry',
  'age' => '30',
));
// We should be able to specify values in any order if named.
$query->values(array(
  'age' => '31',
  'name' => 'Curly',
));
// We should be able to say "use the field order".
// This is not the recommended mechanism for most cases, but it should work.
$query->values(array('Moe', '32'));
$query->execute();
$num_records_after = (int) db_query('SELECT COUNT(*) FROM {test}')->fetchField();
assertIdentical($num_records_before + 3, $num_records_after, t('Record inserts correctly.'));
$saved_age = db_query('SELECT age FROM {test} WHERE name = :name', array(':name' => 'Larry'))->fetchField();
assertIdentical($saved_age, '30', t('Can retrieve after inserting.'));
$saved_age = db_query('SELECT age FROM {test} WHERE name = :name', array(':name' => 'Curly'))->fetchField();
assertIdentical($saved_age, '31', t('Can retrieve after inserting.'));
$saved_age = db_query('SELECT age FROM {test} WHERE name = :name', array(':name' => 'Moe'))->fetchField();
assertIdentical($saved_age, '32', t('Can retrieve after inserting.'));
