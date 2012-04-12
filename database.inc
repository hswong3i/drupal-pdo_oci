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

  protected $willRollback;

  public function __construct(array $connection_options = array()) {
    // This driver defaults to transaction support, except if explicitly passed FALSE.
    $this->transactionSupport = !isset($connection_options['transactions']) || ($connection_options['transactions'] !== FALSE);

    // Transactional DDL is not available in Oracle.
    $this->transactionalDDLSupport = FALSE;

    $this->connectionOptions = $connection_options;

    $dsn = 'oci:dbname=//' . $connection_options['host'] . ':' . (empty($connection_options['port']) ? '1521' : $connection_options['port']) . '/' . $connection_options['database'] . ';charset=AL32UTF8';

    // Allow PDO options to be overridden.
    $connection_options += array(
      'pdo' => array(),
    );
    $connection_options['pdo'] += array(
      // Prepared statements are most effective for performance when queries
      // are recycled (used several times). However, if they are not re-used,
      // prepared statements become ineffecient. Since most of Drupal's
      // prepared queries are not re-used, it should be faster to emulate
      // the preparation than to actually ready statements for re-use. If in
      // doubt, reset to FALSE and measure performance.
      PDO::ATTR_EMULATE_PREPARES => TRUE,
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
        //dpr(array_filter(array($stmt->queryString, $args)));
        $stmt->execute(NULL, $options);
      }
      else {
        $this->expandArguments($query, $args);
        $stmt = $this->prepareQuery($query);
        //dpr(array_filter(array($stmt->queryString, $args)));
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
      //dpr(array_filter(array($stmt->queryString, $args)));
      //dpr($e->errorInfo);
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

  public function prepare($query, $options = array()) {
    return new DatabaseStatement_oci($this, $query, $options);
  }

  public function PDOPrepare($query, array $options = array()) {
    return parent::prepare($query, $options);
  }

  public function prepareQuery($query) {
    $query = $this->prefixTables($query);
    $query = $this->escapeReservedWords($query);
    $query = $this->escapeEmptyString($query);
    $query = $this->escapeLongIdentifiers($query);
    return $this->prepare($query);
  }

  public function escapeReservedWords($query) {
    $reserved_words = implode('|', $this->getReservedWords());
    $pattern = array(
      '/:(' . $reserved_words . ')([\)\.\s\=\,])/',
      '/([\(\.\s\=\,])(' . $reserved_words . ')([\)\.\s\=\,])/',
      '/^(' . $reserved_words . ')$/',
    );
    $replacement = array(
      ':$1$2',
      '$1"$2"$3',
      '"$1"',
    );
    return preg_replace($pattern, $replacement, $query);
  }

  public function escapeEmptyString($query) {
    return str_replace("''", "'E#'", $query);
  }

  public function escapeLongIdentifiers($query) {
    return preg_replace_callback('/[a-z0-9_]{31,}/', array($this, 'escapeLongIdentifier'), $query);
  }

  public function escapeLongIdentifier($matches) {
    global $conf;

    $match = $matches[0];

    $settings_file = DRUPAL_ROOT . '/' . conf_path() . '/settings.php';

    // Do we have the mapping?
    if (!isset($conf['long_identifiers'][$match])) {
      // Dirty workaround to prevent duplicate append of mapping.
      include $settings_file;

      // Double check the mapping once again.
      if (!isset($conf['long_identifiers'][$match])) {
        // Generate the mapping with prefix.
        $shorthand = 'L#' . crc32($match);
        $conf['long_identifiers'][$match] = $shorthand;
        $content = "\$conf['long_identifiers']['$match'] = '$shorthand';\n";

        // Save the long identifiers mapping to settings.php.
        chmod($settings_file, 0666) or die ("Can't chmod($settings_file, 0666");
        $fh = fopen($settings_file, 'a') or die ("Can't open file.");
        fwrite($fh, $content);
        fclose($fh);
        chmod($settings_file, 0444) or die ("Can't chmod($settings_file, 0444");
      }
    }

    return $conf['long_identifiers'][$match];
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
        'LIKE' => array('postfix' => " ESCAPE '\\'"),
        'NOT LIKE' => array('postfix' => " ESCAPE '\\'"),
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
    $transaction = $this->startTransaction();

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

    try {
      return $this->query('SELECT ' . $name . '.CURRVAL FROM DUAL')->fetchField();
    }
    catch (PDOException $e) {
      // Ignore ORA-08002: sequence $name.CURRVAL is not yet defined in this session.
      if ($e->errorInfo[1] !== '8002') {
        throw $e;
      }
    }
  }

  protected function popCommittableTransactions() {
    // Commit all the committable layers.
    foreach (array_reverse($this->transactionLayers) as $name => $active) {
      // Stop once we found an active transaction.
      if ($active) {
        break;
      }

      // If there are no more layers left then we should commit.
      unset($this->transactionLayers[$name]);
      if (empty($this->transactionLayers)) {
        if (!PDO::commit()) {
          throw new DatabaseTransactionCommitFailedException();
        }
      }
      else {
        $this->query('BEGIN NULL; END;');
      }
    }
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

class DatabaseStatement_oci extends DatabaseStatementPrefetch implements Iterator, DatabaseStatementInterface {

  public $queryString;

  public function __construct(DatabaseConnection $connection, $query, array $driver_options = array()) {
    $this->dbh = $connection;
    $this->queryString = $query;
    $this->driverOptions = $driver_options;
  }

  public function escapeEmptyString($query) {
    return $query === '' ? "E#" : $query;
  }

  public function escapeLongIdentifiers($query) {
    return preg_replace_callback('/^[a-z0-9_]{31,}$/', array($this->dbh, 'escapeLongIdentifier'), $query);
  }

  public function bindValue($parameter, $value, $data_type = PDO::PARAM_STR) {
    $value = $this->escapeEmptyString($value);
    $value = $this->escapeLongIdentifiers($value);
    return parent::bindValue($parameter, $value, $data_type);
  }

  public function bindParam($parameter, &$variable, $data_type = PDO::PARAM_STR, $length = 0, $driver_options = array()) {
    $variable = $this->escapeEmptyString($variable);
    $variable = $this->escapeLongIdentifiers($variable);
    return parent::bindParam($parameter, $variable, $data_type, $length, $driver_options);
  }

  public function execute($args = array(), $options = array()) {
    if (isset($options['fetch'])) {
      if (is_string($options['fetch'])) {
        // Default to an object. Note: db fields will be added to the object
        // before the constructor is run. If you need to assign fields after
        // the constructor is run, see http://drupal.org/node/315092.
        $this->setFetchMode(PDO::FETCH_CLASS, $options['fetch']);
      }
      else {
        $this->setFetchMode($options['fetch']);
      }
    }

    $logger = $this->dbh->getLogger();
    if (!empty($logger)) {
      $query_start = microtime(TRUE);
    }

    // Prepare the query.
    $statement = $this->getStatement($this->queryString, $args);
    if (!$statement) {
      $this->throwPDOException();
    }

    $return = $statement->execute($args);
    if (!$return) {
      $this->throwPDOException();
    }

    // Fetch all the data from the reply, in order to release any lock
    // as soon as possible.
    try {
      $this->rowCount = $statement->rowCount();
      $this->data = $statement->fetchAll(PDO::FETCH_ASSOC);
      // Destroy the statement as soon as possible. See
      // DatabaseConnection_sqlite::PDOPrepare() for explanation.
      unset($statement);
    }
    catch (PDOException $e) {
      // Ignore ORA-24374: define not done before fetch or execute and fetch.
      if (!(isset($e->errorInfo) && is_array($e->errorInfo) && $e->errorInfo[1] == '24374')) {
        throw $e;
      }
    }

    $this->resultRowCount = count($this->data);

    if ($this->resultRowCount) {
      $this->columnNames = array_keys($this->data[0]);
    }
    else {
      $this->columnNames = array();
    }

    if (!empty($logger)) {
      $query_end = microtime(TRUE);
      $logger->log($this, $args, $query_end - $query_start);
    }

    // Initialize the first row in $this->currentRow.
    $this->next();

    return $return;
  }

  public function getStatement($query, &$args = array()) {
    return $this->dbh->PDOPrepare($this->queryString);
  }

  private function restoreValue($data) {
    global $conf;

    $long_identifiers = empty($conf['long_identifiers']) ? array() : $conf['long_identifiers'];
    $shorthand = array_flip($long_identifiers);

    // Value stored as empty string placeholder.
    if (is_string($data) && $data === 'E#') {
      return '';
    }
    // Rare condition: why store a constraint name as value into column?
    elseif (is_string($data) && strpos($data, 'L#') === 0 && isset($shorthand[$data])) {
      return $shorthand[$data];
    }

    return $data;
  }

  private function restoreEscaped($data) {
    global $conf;

    $long_identifiers = empty($conf['long_identifiers']) ? array() : $conf['long_identifiers'];
    $shorthand = array_flip($long_identifiers);

    if (is_array($data)) {
      foreach ($data as $key => $value) {
        // Column name is present as shorthand.
        if (strpos($key, 'L#') === 0 && isset($shorthand[$key])) {
          $data[$shorthand[$key]] = $this->restoreEscaped($value);
          unset($data[$key]);
        }
        else {
          $data[$key] = $this->restoreEscaped($value);
        }
      }
    }
    elseif (is_object($data)) {
      foreach ($data as $key => $value) {
        // Column name is present as shorthand.
        if (strpos($key, 'L#') === 0 && isset($shorthand[$key])) {
          $data->{$shorthand[$key]} = $this->restoreEscaped($value);
          unset($data->{$key});
        }
        else {
          $data->{$key} = $this->restoreEscaped($value);
        }
      }
    }
    else {
      $data = $this->restoreValue($data);
    }

    return $data;
  }

  public function current() {
    return $this->restoreEscaped(parent::current());
  }

  public function next() {
    return $this->restoreEscaped(parent::next());
  }

  public function fetch($fetch_style = NULL, $cursor_orientation = PDO::FETCH_ORI_NEXT, $cursor_offset = 0) {
    return $this->restoreEscaped(parent::fetch($fetch_style, $cursor_orientation, $cursor_offset));
  }

  public function fetchField($index = 0) {
    return $this->restoreEscaped(parent::fetchField($index));
  }

  public function fetchObject($class_name = NULL, $ctor_args = array()) {
    return $this->restoreEscaped(parent::fetchObject($class_name, $ctor_args));
  }

  public function fetchAssoc() {
    return $this->restoreEscaped(parent::fetchAssoc());
  }

  public function fetchAll($fetch_style = NULL, $fetch_argument = NULL, $ctor_args = array()) {
    return $this->restoreEscaped(parent::fetchAll($fetch_style, $fetch_argument, $ctor_args));
  }

  public function fetchCol($index = 0) {
    return $this->restoreEscaped(parent::fetchCol($index));
  }

  public function fetchAllKeyed($key_index = 0, $value_index = 1) {
    return $this->restoreEscaped(parent::fetchAllKeyed($key_index, $value_index));
  }

  public function fetchAllAssoc($key, $fetch = NULL) {
    return $this->restoreEscaped(parent::fetchAllAssoc($key, $fetch));
  }
}

/**
 * @} End of "ingroup database".
 */
