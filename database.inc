<?php

/**
 * @file
 * Database interface code for Oracle database servers.
 */

/**
 * @ingroup database
 * @{
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
    $query = $this->escapeQuery($query);

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

  protected function escapeQuery($query) {
    #$query = preg_replace('/ ([a-z][a-z0-9_.]*) /', ' "$1" ', $query);
    return $query;
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
    $id = $this->query('SELECT "' . $sequence_name . '".NEXTVAL FROM DUAL')->fetchField();
    if ($id > $existing_id) {
      return $id;
    }

    // Reset the sequence to a higher value than the existing id.
    $this->query('DROP SEQUENCE "' . $sequence_name . '"');
    $this->query('CREATE SEQUENCE "' . $sequence_name. '" MINVALUE 1 INCREMENT BY 1 START WITH ' . ($existing_id + 1) . ' NOCACHE NOORDER NOCYCLE');

    // Retrive the next id. We know this will be as high as we want it.
    $id = $this->query('SELECT "'. $sequence_name . '".NEXTVAL FROM DUAL')-> fetchField();

    return $id;
  }

  public function lastInsertId($name = NULL) {
    if (!$name) {
      throw new Exception('The name of the sequence is mandatory for Oracle.');
    }

    return $this->query('SELECT "' . $name . '".CURRVAL FROM DUAL')->fetchField();
  }
}

/**
 * @} End of "ingroup database".
 */
