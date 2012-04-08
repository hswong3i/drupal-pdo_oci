<?php

/**
 * @file
 * Database interface code for Oracle database servers.
 */

/**
 * @ingroup database
 * @{
 */

include_once DRUPAL_ROOT . '/includes/database/prefetch.inc';

/**
 * Specific Oracle implementation of DatabaseConnection.
 */
class DatabaseConnection_oci extends DatabaseConnection {

  public function __construct(array $connection_options = array()) {
    // This driver defaults to transaction support, except if explicitly passed FALSE.
    $this->transactionSupport = !isset($connection_options['transactions']) || ($connection_options['transactions'] !== FALSE);

    // Transactional DDL is not available in Oracle.
    $this->transactionalDDLSupport = FALSE;

    // Default to TCP connection on port 1521.
    if (empty($connection_options['port'])) {
      $connection_options['port'] = 1521;
    }

    $this->connectionOptions = $connection_options;

    $dsn = 'oci:dbname=//' . $connection_options['host'] . ':' . $connection_options['port'] . '/' . $connection_options['database'] . ';charset=AL32UTF8';

    // Allow PDO options to be overridden.
    $connection_options += array(
      'pdo' => array(),
    );
    $connection_options['pdo'] += array(
      // Convert numeric values to strings when fetching.
      PDO::ATTR_STRINGIFY_FETCHES => TRUE,
      // Force column names to lower case.
      PDO::ATTR_CASE => PDO::CASE_LOWER,
    );
    parent::__construct($dsn, $connection_options['username'], $connection_options['password'], $connection_options['pdo']);

    // Execute Oracle init_commands.
    if (isset($connection_options['init_commands'])) {
      $this->exec(implode('; ', $connection_options['init_commands']));
    }
  }

  public function query($query, array $args = array(), $options = array()) {

    $options += $this->defaultOptions();

    try {
      if ($query instanceof DatabaseStatementInterface) {
        $stmt = $query;

        // TODO
        dpr(array_filter(array($stmt->queryString, $args)));

        $stmt->execute(NULL, $options);
      }
      else {
        $this->expandArguments($query, $args);
        $stmt = $this->prepareQuery($query);

        // TODO
        dpr(array_filter(array($stmt->queryString, $args)));

        $stmt->execute($args, $options);
      }

      switch ($options['return']) {
        case Database::RETURN_STATEMENT:
          return $stmt;
        case Database::RETURN_AFFECTED:
          return $stmt->rowCount();
        case Database::RETURN_INSERT_ID:
          return $this->lastInsertId($options['sequence_name']);
        case Database::RETURN_NULL:
          return;
        default:
          throw new PDOException('Invalid return directive: ' . $options['return']);
      }
    }
    catch (PDOException $e) {
      if ($options['throw_exception']) {
        // Add additional debug information.
        if ($query instanceof DatabaseStatementInterface) {
          $e->query_string = $stmt->getQueryString();
        }
        else {
          $e->query_string = $query;
        }
        $e->args = $args;
        throw $e;
      }
      return NULL;
    }
  }

//  public function prepare($query, $options = array()) {
//    return new DatabaseStatement_oci($this, $query, $options);
//  }
//
//  public function PDOPrepare($query, array $options = array()) {
//    return parent::prepare($query, $options);
//  }

  public function prepareQuery($query) {
    $query = $this->prefixTables($query);
    $query = $this->escapeReserved($query);
    $query = $this->escapeEmptyString($query);
    $query = $this->escapeLongIdentifiers($query);

    // Call PDO::prepare.
    return $this->prepare($query);
  }

  public function escapeReserved($query) {
    $pattern = array(
      '/:(' . implode('|', $this->getReservedWords()) . ')([\)\.\s\=\,])/',
      '/([\(\.\s\=\,])(' . implode('|', $this->getReservedWords()) . ')([\)\.\s\=\,])/',
    );
    $replacement = array(
      ':$1$2',
      '$1"$2"$3',
    );
    return preg_replace($pattern, $replacement, $query);
  }

  public function escapeEmptyString($query) {
    return preg_replace("/''/", "'E#'", $query);
  }

  public function escapeLongIdentifiers($query) {
    return preg_replace_callback('/[a-z0-9_]{31,}/', array($this, 'escapeLongIdentifier'), $query);
  }

  public function escapeLongIdentifier($matches) {
    global $long_identifiers;

    $match = $matches[0];
    if (!isset($long_identifiers[$match])) {
      require_once DRUPAL_ROOT . '/includes/install.inc';
      $settings_file = DRUPAL_ROOT . '/' . conf_path() . '/settings.php';

      $shorthand = 'L#' . crc32($match);

      drupal_install_fix_file($settings_file, FILE_WRITABLE);

      $fh = fopen($settings_file, 'a') or die ('Cannot open file.');
      $content = "\$long_identifiers['$match'] = '$shorthand';\n";
      fwrite($fh, $content);
      fclose($fh);

      drupal_install_fix_file($settings_file, FILE_NOT_WRITABLE);
      $long_identifiers[$match] = $shorthand;
    }

    return $long_identifiers[$match];
  }

  public function queryRange($query, $from, $count, array $args = array(), array $options = array()) {
    return $this->query('SELECT * FROM (SELECT SUB1.*, ROWNUM AS LINE2 FROM (' . $query . ') SUB1) WHERE LINE2 BETWEEN ' . ($from + 1) . ' AND ' . ($from + $count), $args, $options);
  }

  public function queryTemporary($query, array $args = array(), array $options = array()) {
    $tablename = $this->generateTemporaryTableName();
    $this->query(preg_replace('/^SELECT/i', 'CREATE GLOBAL TEMPORARY TABLE {' . $tablename . '} ON COMMIT PRESERVE ROWS AS SELECT', $query), $args, $options);
    return $tablename;
  }

  public function driver() {
    return 'oci';
  }

  public function databaseType() {
    return 'oci';
  }

  public function mapConditionOperator($operator) {
    static $specials;

    // Function calls not allowed in static declarations, thus this method.
    if (!isset($specials)) {
      $specials = array(
        'IFNULL' => array('operator' => 'NVL'),
      );
    }

    return isset($specials[$operator]) ? $specials[$operator] : NULL;
  }

  /**
   * Retrive a the next id in a sequence.
   *
   * Oracle has built in sequence. We'll use these instead of inserting
   * and updating a sequences table.
   */
  public function nextId($existing_id = 0) {

    // Retrive the name of the sequence. This information cannot be cached
    // because the prefix may change, for example, like it does in simpletests.
    $sequence_name = $this->makeSequenceName('sequences', 'value');

    // When Oracle gets a value too small then it will lock the table,
    // retry the INSERT and if is's still too small than alter the sequence.
    $id = $this->query('SELECT ' . $sequence_name . '.NEXTVAL FROM DUAL')->fetchField();
    if ($id > $existing_id) {
      return $id;
    }

    // Reset the sequence to a higher value than the existing id.
    $this->query('DROP SEQUENCE ' . $sequence_name);
    $this->query('CREATE SEQUENCE ' . $sequence_name. ' MINVALUE 1 INCREMENT BY 1 START WITH ' . ($existing_id + 1) . ' NOCACHE NOORDER NOCYCLE');

    // Retrive the next id. We know this will be as high as we want it.
    $id = $this->query('SELECT '. $sequence_name . '.NEXTVAL FROM DUAL')-> fetchField();

    return $id;
  }

  public function lastInsertId($name = NULL) {
    if (!$name) {
      throw new Exception('The name of the sequence is mandatory for Oracle.');
    }

    return $this->query('SELECT ' . $name . '.CURRVAL FROM DUAL')->fetchField();
  }

  /**
   * Returns an array of reserved words (lowercase) for this DB.
   *
   * This file contains the reserved words for Oracle databases.
   * See http://docs.oracle.com/cd/E11882_01/server.112/e26088/ap_keywd001.htm
   */
  public function getReservedWords() {
    $reserved_words = array (
      'access', 'add', 'all', 'alter', 'and', 'any', 'as', 'asc', 'audit',
      'between', 'by', 'char', 'check', 'cluster', 'column', 'comment',
      'compress', 'connect', 'create', 'current', 'date', 'decimal',
      'default', 'delete', 'desc', 'distinct', 'drop', 'else', 'exclusive',
      'exists', 'file', 'float', 'for', 'from', 'grant', 'group', 'having',
      'identified', 'immediate', 'in', 'increment', 'index', 'initial',
      'insert', 'integer', 'intersect', 'into', 'is', 'level', 'like', 'lock',
      'long', 'maxextents', 'minus', 'mlslabel', 'mode', 'modify', 'nchar',
      'nclob', 'noaudit', 'nocompress', 'not', 'nowait', 'null', 'number',
      'nvarchar2', 'of', 'offline', 'on', 'online', 'option', 'or', 'order',
      'pctfree', 'prior', 'privileges', 'public', 'raw', 'rename', 'resource',
      'revoke', 'row', 'rowid', 'rownum', 'rows', 'select', 'session', 'set',
      'share', 'size', 'smallint', 'start', 'successful', 'synonym',
      'sysdate', 'table', 'then', 'to', 'trigger', 'uid', 'union', 'unique',
      'update', 'user', 'validate', 'values', 'varchar', 'varchar2', 'view',
      'whenever', 'where', 'with'
    );
    return $reserved_words;
  }
}

//class DatabaseStatement_oci extends DatabaseStatementPrefetch implements Iterator, DatabaseStatementInterface {
//
//  public $queryString;
//
//  public function __construct(DatabaseConnection $connection, $query, array $driver_options = array()) {
//    $this->dbh = $connection;
//    $this->queryString = $query;
//    $this->driverOptions = $driver_options;
//  }
//
//  public function getStatement($query, &$args = array()) {
//    return $this->dbh->PDOPrepare($this->queryString);
//  }
//}

/**
 * @} End of "ingroup database".
 */
