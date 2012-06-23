<?php

openlog('pdo_oci', LOG_PID | LOG_PERROR, LOG_USER);

function dpr($input, $return = FALSE, $name = NULL) {
  return dprint_r($input, $return, $name);
}

function dprint_r($input, $return = FALSE, $name = NULL, $function = 'print_r', $check= TRUE) {
  if ($name) {
    $name .= ' => ';
  }

  ob_start();
  $function($input);
  $output = ob_get_clean();

  syslog(LOG_ERR, $name . $output);
  return;

  $printed_value = '<pre>'. $name . $output .'</pre>';

  if ($return) {
    return $printed_value;
  }
  else {
    print $printed_value;
  }
}

