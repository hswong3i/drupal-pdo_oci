<?php

// Add devel-like dprint_r support for debug.
require_once dirname(__FILE__) . '/devel.inc';

// Add simpletest-like assert support for debug.
require_once dirname(__FILE__) . '/simpletest/autorun.php';
require_once dirname(__FILE__) . '/drupal_web_test_case.inc';

// Clone from index.php.
// This file is under includes/database/oci/debug/includes
define('DRUPAL_ROOT', getcwd() . '/../../../..');
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
require_once dirname(__FILE__) . '/settings.php';
