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

// Confirm that we can fetch a record into an indexed array explicitly.
dpr("function testQueryFetchNum()");
$records = array();
$result = db_query('SELECT name FROM {test} WHERE age = :age', array(':age' => 25), array('fetch' => PDO::FETCH_NUM));
foreach ($result as $record) {
  $records[] = $record;
  if (assertTrue(is_array($record), t('Record is an array.'))) {
    assertIdentical($record[0], 'John', t('Record can be accessed numerically.'));
  }
}
assertIdentical(count($records), 1, 'There is only one record');

/**
 * Confirm that we can fetch a record into a doubly-keyed array explicitly.
 */
dpr("function testQueryFetchBoth()");
$records = array();
$result = db_query('SELECT name FROM {test} WHERE age = :age', array(':age' => 25), array('fetch' => PDO::FETCH_BOTH));
foreach ($result as $record) {
  $records[] = $record;
  if (assertTrue(is_array($record), t('Record is an array.'))) {
    assertIdentical($record[0], 'John', t('Record can be accessed numerically.'));
    assertIdentical($record['name'], 'John', t('Record can be accessed associatively.'));
  }
}
assertIdentical(count($records), 1, t('There is only one record.'));

/**
 * Confirm that we can fetch an entire column of a result set at once.
 */
dpr("function testQueryFetchCol()");
$records = array();
$result = db_query('SELECT name FROM {test} WHERE age > :age', array(':age' => 25));
$column = $result->fetchCol();
assertIdentical(count($column), 3, t('fetchCol() returns the right number of records.'));
$result = db_query('SELECT name FROM {test} WHERE age > :age', array(':age' => 25));
$i = 0;
foreach ($result as $record) {
  assertIdentical($record->name, $column[$i++], t('Column matches direct accesss.'));
}
