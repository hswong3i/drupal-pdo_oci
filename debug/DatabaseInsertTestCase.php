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
$this->assertIdentical($num_records_before + 1, (int) $num_records_after, t('Record inserts correctly.'));
$saved_age = db_query('SELECT age FROM {test} WHERE name = :name', array(':name' => 'Yoko'))->fetchField();
$this->assertIdentical($saved_age, '29', t('Can retrieve after inserting.'));

