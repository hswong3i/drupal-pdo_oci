<?php

function randomName($length = 8) {
  $values = array_merge(range(65, 90), range(97, 122), range(48, 57));
  $max = count($values) - 1;
  $str = chr(mt_rand(97, 122));
  for ($i = 1; $i < $length; $i++) {
    $str .= chr($values[mt_rand(0, $max)]);
  }
  return $str;
}

function _assert($status, $message = '', $group = 'Other', array $caller = NULL) {
  // Convert boolean status to string status.
  if (is_bool($status)) {
    $status = $status ? 'pass' : 'fail';
  }

  dpr($status . ": " . $message);

  if ($status == 'pass') {
    return TRUE;
  }
  else {
    return FALSE;
  }
}

function assertTrue($value, $message = '', $group = 'Other') {
  return _assert((bool) $value, $message ? $message : "Value $value is TRUE.", $group);
}

function assertIdentical($first, $second, $message = '', $group = 'Other') {
  return _assert($first === $second, $message ? $message : "Value $first is identical to value $second.", $group);
}

function assertNotNull($value, $message = '', $group = 'Other') {
  return _assert(isset($value), $message ? $message : "Value $value is not NULL.", $group);
}

function assertNotIdentical($first, $second, $message = '', $group = 'Other') {
  return _assert($first !== $second, $message ? $message : "Value $frst is not identical to value $second.", $group);
}

function assertEqual($first, $second, $message = '', $group = 'Other') {
  return _assert($first == $second, $message ? $message : "Value $first is equal to value $second.", $group);
}

function assertNotEqual($first, $second, $message = '', $group = 'Other') {
  return _assert($first != $second, $message ? $message : "Value $first is not equal to value $second.", $group);
}

