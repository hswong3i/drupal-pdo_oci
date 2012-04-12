<?php

/**
 * @file
 * Database schema code for Oracle database servers.
 */

/**
 * @ingroup schemaapi
 * @{
 */

class DatabaseSchema_oci extends DatabaseSchema {

  /**
   * A cache of information about blob columns of tables.
   *
   * This is collected by DatabaseConnection_oci->queryTableInformation(),
   * by introspecting the database.
   *
   * @see DatabaseConnection_oci->queryTableInformation()
   * @var array
   */
  protected $tableInformation = array();

  /**
   * Fetch the list of blobs used on a table.
   *
   * We introspect the database to collect the information required by insert
   * and update queries.
   *
   * @param $table_name
   *   The non-prefixed name of the table.
   * @return
   *   An object with two member variables:
   *     - 'blob_fields' that lists all the blob fields in the table.
   */
  public function queryTableInformation($table, $rebuild = FALSE) {
    // Generate a key to reference this table's information on.
    $key = $this->connection->prefixTables('{' . $table . '}');
    if (!strpos($key, '.')) {
      $key = 'public.' . $key;
    }

    if (!isset($this->tableInformation[$key]) || $rebuild) {
      // Split the key into schema and table for querying.
      list($schema, $table_name) = explode('.', $key);
      $table_information = (object) array(
        'blob_fields' => array(),
        'sequences' => array(),
        'serial_fields' => array(),
        'indexes' => array(),
      );
      // Don't use {} around user_tab_cols table.
      $result = $this->connection->query("SELECT COLUMN_NAME FROM USER_TAB_COLS WHERE TABLE_NAME = :table_name AND DATA_TYPE = 'BLOB'", array(':table_name' => strtoupper($table_name)));
      foreach ($result as $column) {
        $table_information->blob_fields[strtolower($column->column_name)] = TRUE;
      }
      // Don't use {} around user_constraints and user_cons_columns table/
      $result = $this->connection->query("SELECT COLS.TABLE_NAME, COLS.COLUMN_NAME FROM USER_CONSTRAINTS CONS, USER_CONS_COLUMNS COLS, USER_TAB_COLS TCOLS  WHERE COLS.TABLE_NAME = :table_name AND CONS.CONSTRAINT_TYPE = 'P' AND CONS.CONSTRAINT_NAME = COLS.CONSTRAINT_NAME AND CONS.OWNER = COLS.OWNER AND COLS.TABLE_NAME = TCOLS.TABLE_NAME AND COLS.COLUMN_NAME = TCOLS.COLUMN_NAME AND TCOLS.DATA_TYPE = 'NUMBER'", array(':table_name' => strtoupper($table_name)));
      foreach ($result as $column) {
        $table_information->sequences[] = $this->connection->makeSequenceName(strtolower($column->table_name), strtolower($column->column_name));
        $table_information->serial_fields[] = strtolower($column->column_name);
      }
      // Don't use {} around user_indexes table.
      $result = $this->connection->query("SELECT INDEX_NAME FROM USER_INDEXES WHERE TABLE_NAME = :table_name", array(':table_name' => strtoupper($table_name)));
      foreach ($result as $column) {
        $table_information->indexes[strtolower($column->index_name)] = TRUE;
      }
      $this->tableInformation[$key] = $table_information;
    }
    return $this->tableInformation[$key];
  }

  public function tableExists($table) {
    $info = $this->getPrefixInfo($table);

    // Don't use {} around user_tables table.
    return (bool) $this->connection->query('SELECT 1 FROM USER_TABLES WHERE TABLE_NAME = :table_name', array(':table_name' => strtoupper($info['table'])))->fetchField();
  }

  public function fieldExists($table, $column) {
    $info = $this->getPrefixInfo($table);

    // Don't use {} around user_tab_cols table.
    return (bool) $this->connection->query('SELECT 1 FROM USER_TAB_COLS WHERE TABLE_NAME = :table_name AND COLUMN_NAME = :column_name', array(':table_name' => strtoupper($info['table']), ':column_name' => strtoupper($column)))->fetchField();
  }

  /**
   * Generate SQL to create a new table from a Drupal schema definition.
   *
   * @param $name
   *   The name of the table to create.
   * @param $table
   *   A Schema API table definition array.
   * @return
   *   An array of SQL statements to create the table.
   */
  protected function createTableSql($name, $table) {
    $info = $this->getPrefixInfo($name);

    $sql_fields = array();
    foreach ($table['fields'] as $field_name => $field) {
      $sql_fields[] = $this->createFieldSql($field_name, $this->processField($field));
    }

    $sql_keys = array();
    if (isset($table['primary key']) && is_array($table['primary key'])) {
      $sql_keys[] = 'CONSTRAINT ' . $this->prefixNonTable($name, 'pk') . ' PRIMARY KEY (' . implode(', ', $table['primary key']) . ')';
    }
    if (isset($table['unique keys']) && is_array($table['unique keys'])) {
      foreach ($table['unique keys'] as $key_name => $key) {
        $sql_keys[] = 'CONSTRAINT ' . $this->prefixNonTable($name, $key_name, 'uix') . ' UNIQUE (' . implode(', ', $key) . ')';
      }
    }

    $sql = 'CREATE TABLE {' . $name . '} ' . "(\n\t";
    $sql .= implode(",\n\t", $sql_fields);
    if (count($sql_keys) > 0) {
      $sql .= ",\n\t";
    }
    $sql .= implode(",\n\t", $sql_keys);
    $sql .= "\n)";
    $statements[] = $sql;

    if (isset($table['indexes']) && is_array($table['indexes'])) {
      foreach ($table['indexes'] as $key_name => $key) {
        $statements[] = $this->_createIndexSql($name, $key_name, $key);
      }
    }

    foreach ($table['fields'] as $field_name => $field) {
      if (isset($field['type']) && ($field['type'] == 'serial')) {
        $statements[] = $this->_createSequenceSql($name, $field_name);
        $statements[] = $this->_createTriggerSql($name, $field_name);
      }
    }

    // Add table comment.
    if (!empty($table['description'])) {
      $statements[] = $this->connection->prepare('COMMENT ON TABLE ' . $this->connection->escapeReservedWords($info['table']) . ' IS ' . $this->prepareComment($table['description']));
    }

    // Add column comments.
    foreach ($table['fields'] as $field_name => $field) {
      if (!empty($field['description'])) {
        $statements[] = $this->connection->prepare('COMMENT ON COLUMN ' . $this->connection->escapeReservedWords($info['table']) . '.' . $this->connection->escapeReservedWords($field_name) . ' IS ' . $this->prepareComment($field['description']));
      }
    }

    return $statements;
  }

  /**
   * Create an SQL string for a field to be used in table creation or
   * alteration.
   *
   * Before passing a field out of a schema definition into this
   * function it has to be processed by _db_process_field().
   *
   * @param $name
   *    Name of the field.
   * @param $spec
   *    The field specification, as per the schema data structure format.
   */
  protected function createFieldSql($name, $spec) {
    $sql = $name . ' ' . $spec['oci_type'];

    if (isset($spec['type']) && $spec['type'] == 'serial') {
      unset($spec['not null']);
    }

    // With poor Oracle we have a lots of limitation.
    if (isset($spec['precision']) && isset($spec['scale'])) {
      $sql .= '(' . min($spec['precision'], 38) . ', ' . $spec['scale'] . ')';
    }
    elseif ($spec['oci_type'] == 'NVARCHAR2') {
      $sql .= '(' . (isset($spec['length']) ? min($spec['length'], 2000) : 2000) . ')';
    }
    elseif ($spec['oci_type'] == 'NCHAR') {
      $sql .= '(' . (isset($spec['length']) ? min($spec['length'], 1000) : 1000) . ')';
    }
    elseif ($spec['oci_type'] == 'NUMBER') {
      $sql .= '(' . (isset($spec['length']) ? min($spec['length'], 38) : 38) . ')';
    }

    // Oracle require DEFAULT before NULL.
    if (isset($spec['default'])) {
      if ($spec['oci_type'] == 'BLOB') {
        $default = 'EMPTY_BLOB()';
      }
      elseif ($spec['oci_type'] == 'NVARCHAR2' || $spec['oci_type'] == 'NCHAR') {
        $default = "'" . $spec['default'] . "'";
      }
      else {
        $default = is_string($spec['default']) ? "'" . $spec['default'] . "'" : $spec['default'];
      }
      $sql .= " DEFAULT $default";
    }

    if (isset($spec['not null'])) {
      if ($spec['not null']) {
        $sql .= ' NOT NULL';
      }
      else {
        $sql .= ' NULL';
      }
    }

    // Oracle require CHECK constraint as the last condition.
    if (!empty($spec['unsigned'])) {
      $sql .= ' CHECK (' . $name . ' >= 0)';
    }

    return $sql;
  }

  /**
   * Set database-engine specific properties for a field.
   *
   * @param $field
   *   A field description array, as specified in the schema documentation.
   */
  protected function processField($field) {
    if (!isset($field['size'])) {
      $field['size'] = 'normal';
    }

    // Set the correct database-engine specific datatype.
    // In case one is already provided, force it to lowercase.
    if (isset($field['oci_type'])) {
      $field['oci_type'] = drupal_strtoupper($field['oci_type']);
    }
    else {
      $map = $this->getFieldTypeMap();
      $field['oci_type'] = $map[$field['type'] . ':' . $field['size']];
    }

    if (isset($field['type']) && $field['type'] == 'serial') {
      unset($field['not null']);
    }

    return $field;
  }

  /**
   * This maps a generic data type in combination with its data size
   * to the engine-specific data type.
   */
  public function getFieldTypeMap() {
    // Put :normal last so it gets preserved by array_flip. This makes
    // it much easier for modules (such as schema.module) to map
    // database types back into schema types.
    // $map does not use drupal_static as its value never changes.
    static $map = array(
      'varchar:normal'  => 'NVARCHAR2',
      'char:normal'     => 'NCHAR',

      'text:tiny'       => 'NVARCHAR2',
      'text:small'      => 'NVARCHAR2',
      'text:medium'     => 'NVARCHAR2',
      'text:big'        => 'NVARCHAR2',
      'text:normal'     => 'NVARCHAR2',

      'serial:tiny'     => 'NUMBER',
      'serial:small'    => 'NUMBER',
      'serial:medium'   => 'NUMBER',
      'serial:big'      => 'NUMBER',
      'serial:normal'   => 'NUMBER',

      'int:tiny'        => 'NUMBER',
      'int:small'       => 'NUMBER',
      'int:medium'      => 'NUMBER',
      'int:big'         => 'NUMBER',
      'int:normal'      => 'NUMBER',

      'float:tiny'      => 'FLOAT',
      'float:small'     => 'FLOAT',
      'float:medium'    => 'FLOAT',
      'float:big'       => 'FLOAT',
      'float:normal'    => 'FLOAT',

      'numeric:normal'  => 'NUMBER',

      'blob:big'        => 'BLOB',
      'blob:normal'     => 'BLOB',
    );
    return $map;
  }

  protected function _createKeySql($fields) {
    $return = array();
    foreach ($fields as $field) {
      if (is_array($field)) {
        $return[] = 'SUBSTR(' . $field[0] . ', 1, ' . $field[1] . ')';
      }
      else {
        $return[] = $field;
      }
    }
    return implode(', ', $return);
  }

  public function renameTable($table, $new_name) {
    if (!$this->tableExists($table)) {
      throw new DatabaseSchemaObjectDoesNotExistException(t("Cannot rename %table to %table_new: table %table doesn't exist.", array('%table' => $table, '%table_new' => $new_name)));
    }
    if ($this->tableExists($new_name)) {
      throw new DatabaseSchemaObjectExistsException(t("Cannot rename %table to %table_new: table %table_new already exists.", array('%table' => $table, '%table_new' => $new_name)));
    }

    $info = $this->getPrefixInfo($new_name);
    $table_information = $this->queryTableInformation($table, TRUE);

    // Rename Table.
    $this->connection->query('ALTER TABLE {' . $table . '} RENAME TO ' . $info['table']);

    // Rename Indexes.
    foreach ($table_information->indexes as $index => $value) {
      $new_index = strtr($index, array($table => $new_name));
      $this->connection->query('ALTER INDEX ' . $index . ' RENAME TO ' . $new_index);
    }

    // Drop Trigger, rename Sequence, and re-create Trigger.
    if (!empty($table_information->serial_fields)) {
      $field = $table_information->serial_fields[0];
      $this->connection->query('DROP TRIGGER ' . $this->prefixNonTable($table, $field, 'trg'));
      $this->connection->query('RENAME ' . $this->connection->makeSequenceName($table, $field) . ' TO ' . $this->connection->makeSequenceName($new_name, $field));
      $this->connection->query($this->_createTriggerSql($new_name, $field));
    }
  }

  public function dropTable($table) {
    if (!$this->tableExists($table)) {
      return FALSE;
    }

    $table_information = $this->queryTableInformation($table, TRUE);

    // Drop Trigger and Sequence.
    if (!empty($table_information->serial_fields)) {
      $field = $table_information->serial_fields[0];
      $this->connection->query('DROP TRIGGER ' . $this->prefixNonTable($table, $field, 'trg'));
      $this->connection->query('DROP SEQUENCE ' . $this->connection->makeSequenceName($table, $field));
    }

    $this->connection->query('DROP TABLE {' . $table . '} CASCADE CONSTRAINTS');
    return TRUE;
  }

  public function addField($table, $field, $spec, $new_keys = array()) {
    if (!$this->tableExists($table)) {
      throw new DatabaseSchemaObjectDoesNotExistException(t("Cannot add field %table.%field: table doesn't exist.", array('%field' => $field, '%table' => $table)));
    }
    if ($this->fieldExists($table, $field)) {
      throw new DatabaseSchemaObjectExistsException(t("Cannot add field %table.%field: field already exists.", array('%field' => $field, '%table' => $table)));
    }

    $fixnull = FALSE;
    if (!empty($spec['not null']) && !isset($spec['default'])) {
      $fixnull = TRUE;
      $spec['not null'] = FALSE;
    }

    $query = 'ALTER TABLE {' . $table . '} ADD ';
    $query .= $this->createFieldSql($field, $this->processField($spec));
    $this->connection->query($query);

    if (isset($spec['initial'])) {
      // TODO: Once $this->connection->update() get ready.
      // $this->connection->update($table)
      //   ->fields(array($field => $spec['initial']))
      //   ->execute();
      $this->connection->query('UPDATE {' . $table . '} SET ' . $field . ' = :initial_value', array(':initial_value' => $spec['initial']));
    }

    if ($fixnull) {
      $this->connection->query('ALTER TABLE {' . $table . '} MODIFY (' . $field . ' NOT NULL)');
    }

    if (isset($new_keys)) {
      $this->_createKeys($table, $new_keys);
    }

    // Add Trigger and Sequence.
    if ($spec['type'] == 'serial') {
      $this->connection->query($this->_createSequenceSql($table, $field));
      $this->connection->query($this->_createTriggerSql($table, $field));
    }

    // Add column comment.
    if (!empty($spec['description'])) {
      $stmt = $this->connection->prepare('COMMENT ON COLUMN ' . $this->connection->escapeReservedWords($table) . '.' . $this->connection->escapeReservedWords($field) . ' IS ' . $this->prepareComment($spec['description']));
      $this->connection->query($stmt);
    }
  }

  public function dropField($table, $field) {
    if (!$this->fieldExists($table, $field)) {
      return FALSE;
    }

    $table_information = $this->queryTableInformation($table, TRUE);

    // Drop Trigger and Sequence.
    if (!empty($table_information->serial_fields) && ($table_information->serial_fields[0] == $field)) {
      $this->connection->query('DROP TRIGGER ' . $this->prefixNonTable($table, $field, 'trg'));
      $this->connection->query('DROP SEQUENCE ' . $this->connection->makeSequenceName($table, $field));
    }

    $this->connection->query('ALTER TABLE {' . $table . '} DROP COLUMN ' . $field);
    return TRUE;
  }

  public function fieldSetDefault($table, $field, $default) {
    if (!$this->fieldExists($table, $field)) {
      throw new DatabaseSchemaObjectDoesNotExistException(t("Cannot set default value of field %table.%field: field doesn't exist.", array('%table' => $table, '%field' => $field)));
    }

    if (!isset($default)) {
      $default = 'NULL';
    }
    else {
      $default = is_string($default) ? "'$default'" : $default;
    }

    $this->connection->query('ALTER TABLE {' . $table . '} MODIFY (' . $field . ' DEFAULT ' . $default . ')');
  }

  public function fieldSetNoDefault($table, $field) {
    if (!$this->fieldExists($table, $field)) {
      throw new DatabaseSchemaObjectDoesNotExistException(t("Cannot remove default value of field %table.%field: field doesn't exist.", array('%table' => $table, '%field' => $field)));
    }

    $this->connection->query('ALTER TABLE {' . $table . '} MODIFY (' . $field . ' DEFAULT NULL)');
  }

  public function indexExists($table, $name) {
    $info = $this->getPrefixInfo($table);

    // Don't use {} around user_indexes table.
    return (bool) $this->connection->query('SELECT 1 FROM USER_INDEXES WHERE TABLE_NAME = :table_name AND INDEX_NAME = :index_name', array(':table_name' => strtoupper($info['table']), ':index_name' => strtoupper($this->prefixNonTable($info['table'], $name, 'ix'))))->fetchField();
  }

  public function constraintExists($table) {
    $args = func_get_args();

    $info = $this->getPrefixInfo($table);
    $constraint_name = implode('_', $args);

    // Don't use {} around user_cons_columns table.
    return (bool) $this->connection->query('SELECT 1 FROM USER_CONS_COLUMNS WHERE TABLE_NAME = :table_name AND COLUMN_NAME = :column_name', array(':table_name' => strtoupper($info['table']), ':column_name' => strtoupper($constraint_name)))->fetchField();
  }

  public function addPrimaryKey($table, $fields) {
    if (!$this->tableExists($table)) {
      throw new DatabaseSchemaObjectDoesNotExistException(t("Cannot add primary key to table %table: table doesn't exist.", array('%table' => $table)));
    }
    if ($this->constraintExists($table, 'pk')) {
      throw new DatabaseSchemaObjectExistsException(t("Cannot add primary key to table %table: primary key already exists.", array('%table' => $table)));
    }

    $this->connection->query('ALTER TABLE {' . $table . '} ADD CONSTRAINT ' . $this->prefixNonTable($table, 'pk') . ' PRIMARY KEY (' . implode(', ', $fields) . ')');
  }

  public function dropPrimaryKey($table) {
    if (!$this->constraintExists($table, 'pk')) {
      return FALSE;
    }

    $this->connection->query('ALTER TABLE {' . $table . '} DROP CONSTRAINT ' . $this->prefixNonTable($table, 'pk'));
    return TRUE;
  }

  public function addUniqueKey($table, $name, $fields) {
    if (!$this->tableExists($table)) {
      throw new DatabaseSchemaObjectDoesNotExistException(t("Cannot add unique key %name to table %table: table doesn't exist.", array('%table' => $table, '%name' => $name)));
    }
    if ($this->constraintExists($table, $name, 'uix')) {
      throw new DatabaseSchemaObjectExistsException(t("Cannot add unique key %name to table %table: unique key already exists.", array('%table' => $table, '%name' => $name)));
    }

    $this->connection->query('ALTER TABLE {' . $table . '} ADD CONSTRAINT ' . $this->prefixNonTable($table, $name, 'uix') . ' UNIQUE (' . implode(', ', $fields) . ')');
  }

  public function dropUniqueKey($table, $name) {
    if (!$this->constraintExists($table, $name, 'uix')) {
      return FALSE;
    }

    $this->connection->query('ALTER TABLE {' . $table . '} DROP CONSTRAINT ' . $this->prefixNonTable($table, $name, 'uix'));
    return TRUE;
  }

  public function addIndex($table, $name, $fields) {
    if (!$this->tableExists($table)) {
      throw new DatabaseSchemaObjectDoesNotExistException(t("Cannot add index %name to table %table: table doesn't exist.", array('%table' => $table, '%name' => $name)));
    }
    if ($this->indexExists($table, $name)) {
      throw new DatabaseSchemaObjectExistsException(t("Cannot add index %name to table %table: index already exists.", array('%table' => $table, '%name' => $name)));
    }

    $this->connection->query($this->_createIndexSql($table, $name, $fields));
  }

  public function dropIndex($table, $name) {
    if (!$this->indexExists($table, $name)) {
      return FALSE;
    }

    $this->connection->query('DROP INDEX ' . $this->prefixNonTable($table, $name, 'ix'));
    return TRUE;
  }

  public function changeField($table, $field, $field_new, $spec, $new_keys = array()) {
    if (!$this->fieldExists($table, $field)) {
      throw new DatabaseSchemaObjectDoesNotExistException(t("Cannot change the definition of field %table.%name: field doesn't exist.", array('%table' => $table, '%name' => $field)));
    }
    if (($field != $field_new) && $this->fieldExists($table, $field_new)) {
      throw new DatabaseSchemaObjectExistsException(t("Cannot rename field %table.%name to %name_new: target field already exists.", array('%table' => $table, '%name' => $field, '%name_new' => $field_new)));
    }

    $table_information = $this->queryTableInformation($table, TRUE);

    $not_null = isset($spec['not null']) ? $spec['not null'] : FALSE;
    unset($spec['not null']);

    $spec['size'] = isset($spec['size']) ? $spec['size'] : 'normal';

    // Drop Trigger and rename Sequence.
    if (!empty($table_information->serial_fields) && ($table_information->serial_fields[0] == $field)) {
      $this->connection->query('DROP TRIGGER ' . $this->prefixNonTable($table, $field, 'trg'));
      $serial_value = $this->connection->query('SELECT ' . $this->connection->makeSequenceName($table, $field) . ' CURRVAL FROM DUAL')->fetchField();
      $this->connection->query('DROP SEQUENCE ' . $this->connection->makeSequenceName($table, $field));
    }

    // Backup legacy field data.
    $this->connection->query('ALTER TABLE {' . $table . '} RENAME COLUMN ' . $field . ' TO ' . $field . '_old');

    // Add new field and transfer legacy data.
    $this->addField($table, $field_new, $spec);
    $this->connection->query('UPDATE {' . $table . '} SET ' . $field_new . ' = ' . $field . '_old');

    if ($not_null) {
      $this->connection->query('ALTER TABLE {' . $table . '} MODIFY (' . $field_new . ' NOT NULL)');
    }

    $this->dropField($table, $field . '_old');

    if (isset($new_keys)) {
      $this->_createKeys($table, $new_keys);
    }

    // Restore latest Sequence ID.
    if (!empty($table_information->serial_fields) && ($table_information->serial_fields[0] == $field)) {
      $this->connection->query('DROP SEQUENCE ' . $this->connection->makeSequenceName($table, $field_new));
      $this->connection->query('CREATE SEQUENCE ' . $this->connection->makeSequenceName($table, $field_new) . ' MINVALUE 1 INCREMENT BY 1 START WITH :serial_value NOCACHE NOORDER NOCYCLE', array(':serial_value' => $serial_value));
    }
  }

  protected function _createIndexSql($table, $name, $fields) {
    $query = 'CREATE INDEX ' . $this->prefixNonTable($table, $name, 'ix') . ' ON {' . $table . '} (';
    $query .= $this->_createKeySql($fields) . ')';
    return $query;
  }

  protected function _createSequenceSql($table, $field) {
    return 'CREATE SEQUENCE ' . $this->connection->makeSequenceName($table, $field) . ' MINVALUE 1 INCREMENT BY 1 START WITH 1 NOCACHE NOORDER NOCYCLE';
  }

  protected function _createTriggerSql($table, $field) {
    $query = 'CREATE OR REPLACE TRIGGER ' . $this->prefixNonTable($table, $field, 'trg') . ' ';
    $query .= 'BEFORE INSERT ON {' . $table . '} ';
    $query .= 'FOR EACH ROW ';
    $query .= 'BEGIN ';
    $query .= 'SELECT ' . $this->connection->makeSequenceName($table, $field) . '.NEXTVAL ';
    $query .= 'INTO :NEW.' . $field . ' ';
    $query .= 'FROM DUAL;';
    $query .= 'END;';
    return $query;
  }

  protected function _createKeys($table, $new_keys) {
    if (isset($new_keys['primary key'])) {
      $this->addPrimaryKey($table, $new_keys['primary key']);
    }
    if (isset($new_keys['unique keys'])) {
      foreach ($new_keys['unique keys'] as $name => $fields) {
        $this->addUniqueKey($table, $name, $fields);
      }
    }
    if (isset($new_keys['indexes'])) {
      foreach ($new_keys['indexes'] as $name => $fields) {
        $this->addIndex($table, $name, $fields);
      }
    }
  }

  /**
   * Retrieve a table or column comment.
   */
  public function getComment($table, $column = NULL) {
    $info = $this->getPrefixInfo($table);
    // Don't use {} around user_col_comments, user_tab_comments tables.
    if (isset($column)) {
      return $this->connection->query('SELECT COMMENTS FROM USER_COL_COMMENTS WHERE TABLE_NAME = :table_name AND COLUMN_NAME = :column_name', array(':table_name' => strtoupper($info['table']), ':column_name' => strtoupper($column)))->fetchField();
    }
    else {
      return $this->connection->query('SELECT COMMENTS FROM USER_TAB_COMMENTS WHERE TABLE_NAME = :table_name', array(':table_name' => strtoupper($info['table'])))->fetchField();
    }
  }
}

/**
 * @} End of "ingroup schemaapi".
 */
