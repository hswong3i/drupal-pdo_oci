<?php

/**
 * An alias for dprint_r(). Saves carpal tunnel syndrome.
 */
function dpr($input, $return = FALSE, $name = NULL) {
  return dprint_r($input, $return, $name);
}

/**
 * Pretty-print a variable to the browser (no krumo).
 * Displays only for users with proper permissions. If
 * you want a string returned instead of a print, use the 2nd param.
 */
function dprint_r($input, $return = FALSE, $name = NULL, $function = 'print_r', $check= TRUE) {
  if ($name) {
    $name .= ' => ';
  }
  
  ob_start();
  $function($input);
  $output = ob_get_clean();

  $printed_value = '<pre>'. $name . $output .'</pre>';

  if ($return) {
    return $printed_value;
  }
  else {
    print $printed_value;
  }
}
