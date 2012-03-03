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
 * Confirm that we can fetch a record properly in default object mode.
 */
dpr("function testQueryFetchDefault()");
$records = array();
$result = db_query('SELECT name FROM {test} WHERE age = :age', array(':age' => 25));
assertTrue($result instanceof DatabaseStatementInterface, t('Result set is a Drupal statement object.'));
foreach ($result as $record) {
  $records[] = $record;
  assertTrue(is_object($record), t('Record is an object.'));
  assertIdentical($record->name, 'John', t('25 year old is John.'));
}
assertIdentical(count($records), 1, t('There is only one record.'));

/**
 * Confirm that we can fetch a record to an object explicitly.
 */
dpr("function testQueryFetchObject()");
$records = array();
$result = db_query('SELECT name FROM {test} WHERE age = :age', array(':age' => 25), array('fetch' => PDO::FETCH_OBJ));
foreach ($result as $record) {
  $records[] = $record;
  assertTrue(is_object($record), t('Record is an object.'));
  assertIdentical($record->name, 'John', t('25 year old is John.'));
}
assertIdentical(count($records), 1, t('There is only one record.'));

/**
 * Confirm that we can fetch a record to an array associative explicitly.
 */
dpr("function testQueryFetchArray()");
$records = array();
$result = db_query('SELECT name FROM {test} WHERE age = :age', array(':age' => 25), array('fetch' => PDO::FETCH_ASSOC));
foreach ($result as $record) {
  $records[] = $record;
  if (assertTrue(is_array($record), t('Record is an array.'))) {
    assertIdentical($record['name'], 'John', t('Record can be accessed associatively.'));
  }
}
assertIdentical(count($records), 1, t('There is only one record.'));

/**
 * Confirm that we can fetch a record into a new instance of a custom class.
 *
 * @see FakeRecord
 */
dpr("function testQueryFetchClass()");
$records = array();
$result = db_query('SELECT name FROM {test} WHERE age = :age', array(':age' => 25), array('fetch' => 'FakeRecord'));
foreach ($result as $record) {
  $records[] = $record;
  if (assertTrue($record instanceof FakeRecord, t('Record is an object of class FakeRecord.'))) {
    assertIdentical($record->name, 'John', t('25 year old is John.'));
  }
}
assertIdentical(count($records), 1, t('There is only one record.'));
