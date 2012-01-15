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
 * Confirm that we can update a single record successfully.
 */
dpr("function testSimpleUpdate()");
$num_updated = db_update('test')
  ->fields(array('name' => 'Tiffany'))
  ->condition('id', 1)
  ->execute();
assertIdentical($num_updated, 1, t('Updated 1 record.'));
$saved_name = db_query('SELECT name FROM {test} WHERE id = :id', array(':id' => 1))->fetchField();
assertIdentical($saved_name, 'Tiffany', t('Updated name successfully.'));

/**
 * Confirm that we can update a multiple records successfully.
 */
dpr("function testMultiUpdate()");
$num_updated = db_update('test')
  ->fields(array('job' => 'Musician'))
  ->condition('job', 'Singer')
  ->execute();
assertIdentical($num_updated, 2, t('Updated 2 records.'));
$num_matches = db_query('SELECT COUNT(*) FROM {test} WHERE job = :job', array(':job' => 'Musician'))->fetchField();
assertIdentical($num_matches, '2', t('Updated fields successfully.'));

/**
 * Confirm that we can update a multiple records with a non-equality condition.
 */
dpr("function testMultiGTUpdate()");
$num_updated = db_update('test')
  ->fields(array('job' => 'Musician'))
  ->condition('age', 26, '>')
  ->execute();
assertIdentical($num_updated, 2, t('Updated 2 records.'));
$num_matches = db_query('SELECT COUNT(*) FROM {test} WHERE job = :job', array(':job' => 'Musician'))->fetchField();
assertIdentical($num_matches, '2', t('Updated fields successfully.'));

