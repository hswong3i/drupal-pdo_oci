<?php

require_once dirname(__FILE__) . '/simpletest/autorun.php';
require_once dirname(__FILE__) . '/simpletest/reporter.php';

class DrupalWebTestCase extends UnitTestCase {

  function setUp() {
    shell_exec("sqlplus SYS@AL32UTF8/CHANGE AS SYSDBA @" . dirname(__FILE__) . "/drupal_web_test_case.sql");
  }

  public static function randomString($length = 8) {
    $str = '';
    for ($i = 0; $i < $length; $i++) {
      $str .= chr(mt_rand(32, 126));
    }
    return $str;
  }

  public static function randomName($length = 8) {
    $values = array_merge(range(65, 90), range(97, 122), range(48, 57));
    $max = count($values) - 1;
    $str = chr(mt_rand(97, 122));
    for ($i = 1; $i < $length; $i++) {
      $str .= chr($values[mt_rand(0, $max)]);
    }
    return $str;
  }

}

class DrupalHtmlReporter extends HtmlReporter {

  function paintPass($message) {
    parent::paintPass($message);
    print "<span class=\"pass\">Pass</span>: ";
    $breadcrumb = $this->getTestList();
    array_shift($breadcrumb);
    print implode(" -&gt; ", $breadcrumb);
    print " -&gt; " . $this->htmlEntities($message) . "<br />\n";
  }

}

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

  $printed_value = '<pre>'. $name . $output .'</pre>';

  if ($return) {
    return $printed_value;
  }
  else {
    print $printed_value;
  }
}

SimpleTest::prefer(new DrupalWebTestCase());
SimpleTest::prefer(new DrupalHtmlReporter());
