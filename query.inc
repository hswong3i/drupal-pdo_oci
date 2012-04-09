<?php

/**
 * @ingroup database
 * @{
 */

/**
 * @file
 * Query code for Oracle embedded database engine.
 */

class InsertQuery_oci extends InsertQuery {

  public function execute() {
    if (!$this->preExecute()) {
      return NULL;
    }

    // Fetch the list of blobs and sequences used on that table.
    $table_information = $this->connection->schema()->queryTableInformation($this->table);

    // Oracle requires the table name to be specified explicitly
    // when requesting the last insert ID, so we pass that in via
    // the options array.
    $options = $this->queryOptions;

    if (!empty($table_information->sequences)) {
      $options['sequence_name'] = $table_information->sequences[0];
    }
    // If there are no sequences then we can't get a last insert id.
    elseif ($options['return'] == Database::RETURN_INSERT_ID) {
      $options['return'] = Database::RETURN_NULL;
    }

    // Each insert happens in its own query in the degenerate case. However,
    // we wrap it in a transaction so that it is atomic where possible. On many
    // databases, such as SQLite, this is also a notable performance boost.
    $transaction = $this->connection->startTransaction();

    $stmt = $this->connection->prepareQuery((string) $this)->getStatement(NULL);

    $blobs = array();
    $blob_count = 0;
    foreach ($this->insertValues as $insert_values) {

      $max_placeholder = 0;
      foreach ($this->insertFields as $idx => $field) {
        if (isset($table_information->blob_fields[$field])) {
          if (!is_null($insert_values[$idx])) {
            // Writing NULL into file handler means write nothing. The file
            // handler will contain an empty string but not NULL. Therefore
            // NULL will be translated as empty string when save into
            // database, and so buggy.
            $blobs[$blob_count] = fopen('php://memory', 'a');
            fwrite($blobs[$blob_count], $insert_values[$idx]);
            rewind($blobs[$blob_count]);

            $stmt->bindParam(':db_insert_placeholder_' . $max_placeholder++, $blobs[$blob_count], PDO::PARAM_LOB);

            // Pre-increment is faster in PHP than increment.
            ++$blob_count;
          }
          else {
            // BLOB NULL is a bit different from normal case: we can't use
            // StreamAPI, but bind NULL to target field direcly.
            $stmt->bindParam(':db_insert_placeholder_' . $max_placeholder++, $insert_values[$idx], PDO::PARAM_LOB);
          }
        }
        else {
          $stmt->bindParam(':db_insert_placeholder_' . $max_placeholder++, $insert_values[$idx]);
        }
      }

      // Check if values for a serial field has been passed.
      if (!empty($table_information->serial_fields)) {
        foreach ($table_information->serial_fields as $index => $serial_field) {
          $serial_key = array_search($serial_field, $this->insertFields);
          if ($serial_key !== FALSE) {
            $serial_value = $insert_values[$serial_key];

            // Force $last_insert_id to the specified value. This is only done
            // if $index is 0.
            if ($index == 0) {
              $last_insert_id = $serial_value;
            }
            // Set the sequence to the bigger value of either the passed
            // value or the max value of the column. It can happen that another
            // thread calls nextval() which could lead to a serial number being
            // used twice. However, trying to insert a value into a serial
            // column should only be done in very rare cases and is not thread
            // safe by definition.
            $current_value = $this->connection->lastInsertId($table_information->sequences[$index]);
            $this->connection->query('DROP SEQUENCE ' . $table_information->sequences[$index]);
            $this->connection->query('CREATE SEQUENCE ' . $table_information->sequences[$index] . ' MINVALUE 1 INCREMENT BY 1 START WITH ' . max($serial_value, $current_value) . ' NOCACHE NOORDER NOCYCLE');
          }
        }
      }

      // Only use the returned last_insert_id if it is not already set.
      $last_insert_id = $this->connection->query($stmt, array(), $options);
    }

    if (!empty($this->fromQuery)) {
      // bindParam stores only a reference to the variable that is followed when
      // the statement is executed. We pass $arguments[$key] instead of $value
      // because the second argument to bindParam is passed by reference and
      // the foreach statement assigns the element to the existing reference.
      $arguments = $this->fromQuery->getArguments();
      foreach ($arguments as $key => $value) {
        $stmt->bindParam($key, $arguments[$key]);
      }
    }

    // All default query.
    if (count($this->insertValues) == 0) {
      // Only use the returned last_insert_id if it is not already set.
      $last_insert_id = $this->connection->query($stmt, array(), $options);
    }

    // Re-initialize the values array so that we can re-use this query.
    $this->insertValues = array();

    // Transaction commits here where $transaction looses scope.

    return $last_insert_id;
  }

  public function __toString() {

    // Fetch the list of blobs and sequences used on that table.
    $table_information = $this->connection->schema()->queryTableInformation($this->table);

    // Create a sanitized comment string to prepend to the query.
    $comments = $this->connection->makeComment($this->comments);

    // Default fields are always placed first for consistency.
    $insert_fields = array_merge($this->defaultFields, $this->insertFields);

    // If we're selecting from a SelectQuery, finish building the query and
    // pass it back, as any remaining options are irrelevant.
    if (!empty($this->fromQuery)) {
      return $comments . 'INSERT INTO {' . $this->table . '} (' . implode(', ', $insert_fields) . ') ' . $this->fromQuery;
    }

    $query = $comments . 'INSERT INTO {' . $this->table . '} (' . implode(', ', $insert_fields) . ') VALUES ';

    $blob_fields = array();
    $blob_placeholders = array();
    if (count($this->insertValues)) {
      $placeholders = array();

      // Default fields aren't really placeholders, but this is the most convenient
      // way to handle them.
      $placeholders = array_pad($placeholders, count($this->defaultFields), 'DEFAULT');

      $i = 0;
      foreach ($this->insertFields as $field) {
        if (isset($table_information->blob_fields[$field])) {
          // BLOB field insert in Oracle required additional handling with
          // EMPTY_BLOB() pleaseholder + RETURNING INTO (see below).
          $placeholders[] = 'EMPTY_BLOB()';
          $blob_fields[] = $field;
          $blob_placeholders[] = ':db_insert_placeholder_' . $i;
        }
        else {
          $placeholders[] = ':db_insert_placeholder_' . $i;
        }
        ++$i;
      }
      $query .= '(' . implode(', ', $placeholders) . ')';
    }
    else {
      // If there are no values, then this is a default-only query. We still need to handle that.
      $placeholders = array_fill(0, count($this->defaultFields), 'DEFAULT');
      $query .= '(' . implode(', ', $placeholders) . ')';
    }

    // Oracle BLOB additional handling with RETURNING INTO.
    if (count($blob_fields)) {
      $query .= ' RETURNING ' . implode(', ', $blob_fields) . ' INTO ' . implode (', ', $blob_placeholders);
    }

    return $query;
  }
}

class UpdateQuery_oci extends UpdateQuery {

  public function execute() {
    $max_placeholder = 0;
    $blobs = array();
    $blob_count = 0;

    // Because we filter $fields the same way here and in __toString(), the
    // placeholders will all match up properly.
    $stmt = $this->connection->prepareQuery((string) $this)->getStatement(NULL);

    // Fetch the list of blobs and sequences used on that table.
    $table_information = $this->connection->schema()->queryTableInformation($this->table);

    // Expressions take priority over literal fields, so we process those first
    // and remove any literal fields that conflict.
    $fields = $this->fields;
    $expression_fields = array();
    foreach ($this->expressionFields as $field => $data) {
      if (!empty($data['arguments'])) {
        foreach ($data['arguments'] as $placeholder => $argument) {
          // We assume that an expression will never happen on a BLOB field,
          // which is a fairly safe assumption to make since in most cases
          // it would be an invalid query anyway.
          $stmt->bindParam($placeholder, $data['arguments'][$placeholder]);
        }
      }
      unset($fields[$field]);
    }

    foreach ($fields as $field => $value) {
      $placeholder = ':db_update_placeholder_' . ($max_placeholder++);

      if (isset($table_information->blob_fields[$field])) {
        $blobs[$blob_count] = fopen('php://memory', 'a');
        fwrite($blobs[$blob_count], $value);
        rewind($blobs[$blob_count]);
        $stmt->bindParam($placeholder, $blobs[$blob_count], PDO::PARAM_LOB);
        ++$blob_count;
      }
      else {
        $stmt->bindParam($placeholder, $fields[$field]);
      }
    }

    if (count($this->condition)) {
      $this->condition->compile($this->connection, $this);

      $arguments = $this->condition->arguments();
      foreach ($arguments as $placeholder => $value) {
        $stmt->bindParam($placeholder, $arguments[$placeholder]);
      }
    }

    $options = $this->queryOptions;
    $options['already_prepared'] = TRUE;
    $this->connection->query($stmt, $options);

    return $stmt->rowCount();
  }

  public function __toString() {
    // Fetch the list of blobs and sequences used on that table.
    $table_information = $this->connection->schema()->queryTableInformation($this->table);

    // Create a sanitized comment string to prepend to the query.
    $comments = $this->connection->makeComment($this->comments);

    // Expressions take priority over literal fields, so we process those first
    // and remove any literal fields that conflict.
    $fields = $this->fields;
    $update_fields = array();
    foreach ($this->expressionFields as $field => $data) {
      $update_fields[] = $field . '=' . $data['expression'];
      unset($fields[$field]);
    }

    $max_placeholder = 0;
    $blob_fields = array();
    $blob_placeholders = array();
    foreach ($fields as $field => $value) {
      if (isset($table_information->blob_fields[$field])) {
        // BLOB field update in Oracle required additional handling with
        // EMPTY_BLOB() pleaseholder + RETURNING INTO (see below).
        $update_fields[] = $field . '=EMPTY_BLOB()';
        $blob_fields[] = $field;
        $blob_placeholders[] = ':db_insert_placeholder_' . $i;
      }
      else {
        $update_fields[] = $field . '=:db_update_placeholder_' . ($max_placeholder++);
      }
    }

    $query = $comments . 'UPDATE {' . $this->connection->escapeTable($this->table) . '} SET ' . implode(', ', $update_fields);

    if (count($this->condition)) {
      $this->condition->compile($this->connection, $this);
      // There is an implicit string cast on $this->condition.
      $query .= "\nWHERE " . $this->condition;
    }

    // Oracle BLOB additional handling with RETURNING INTO.
    if (count($blob_fields)) {
      $query .= ' RETURNING ' . implode(', ', $blob_fields) . ' INTO ' . implode (', ', $blob_placeholders);
    }

    return $query;
  }
}

class TruncateQuery_oci extends TruncateQuery {

  public function __toString() {
    // Create a sanitized comment string to prepend to the query.
    $comments = $this->connection->makeComment($this->comments);

    return $comments . 'TRUNCATE TABLE {' . $this->connection->escapeTable($this->table) . '} ';
  }
}

class DatabaseCondition_oci extends DatabaseCondition {

  public function compile(DatabaseConnection $connection, QueryPlaceholderInterface $queryPlaceholder) {
    // Re-compile if this condition changed or if we are compiled against a
    // different query placeholder object.
    if ($this->changed || isset($this->queryPlaceholderIdentifier) && ($this->queryPlaceholderIdentifier != $queryPlaceholder->uniqueIdentifier())) {
      $this->queryPlaceholderIdentifier = $queryPlaceholder->uniqueIdentifier();

      $condition_fragments = array();
      $arguments = array();

      $conditions = $this->conditions;
      $conjunction = $conditions['#conjunction'];
      unset($conditions['#conjunction']);
      foreach ($conditions as $condition) {
        if (empty($condition['operator'])) {
          // This condition is a literal string, so let it through as is.
          $condition_fragments[] = ' (' . $condition['field'] . ') ';
          $arguments += $condition['value'];
        }
        else {
          // It's a structured condition, so parse it out accordingly.
          // Note that $condition['field'] will only be an object for a dependent
          // DatabaseCondition object, not for a dependent subquery.
          if ($condition['field'] instanceof QueryConditionInterface) {
            // Compile the sub-condition recursively and add it to the list.
            $condition['field']->compile($connection, $queryPlaceholder);
            $condition_fragments[] = '(' . (string) $condition['field'] . ')';
            $arguments += $condition['field']->arguments();
          }
          else {
            // For simplicity, we treat all operators as the same data structure.
            // In the typical degenerate case, this won't get changed.
            $operator_defaults = array(
              'prefix' => '',
              'postfix' => '',
              'delimiter' => '',
              'operator' => $condition['operator'],
              'use_value' => TRUE,
            );
            $operator = $connection->mapConditionOperator($condition['operator']);
            if (!isset($operator)) {
              $operator = $this->mapConditionOperator($condition['operator']);
            }
            $operator += $operator_defaults;

            $placeholders = array();
            if ($condition['value'] instanceof SelectQueryInterface) {
              $condition['value']->compile($connection, $queryPlaceholder);
              $placeholders[] = (string) $condition['value'];
              $arguments += $condition['value']->arguments();
              // Subqueries are the actual value of the operator, we don't
              // need to add another below.
              $operator['use_value'] = FALSE;
            }
            // We assume that if there is a delimiter, then the value is an
            // array. If not, it is a scalar. For simplicity, we first convert
            // up to an array so that we can build the placeholders in the same way.
            elseif (!$operator['delimiter']) {
              $condition['value'] = array($condition['value']);
            }
            if ($operator['use_value']) {
              foreach ($condition['value'] as $value) {
                $placeholder = ':db_condition_placeholder_' . $queryPlaceholder->nextPlaceholder();
                $arguments[$placeholder] = $value;
                $placeholders[] = $placeholder;
              }
            }
            $condition_fragments[] = ' (' . $connection->escapeField($condition['field']) . ' ' . $operator['operator'] . ' ' . $operator['prefix'] . implode($operator['delimiter'], $placeholders) . $operator['postfix'] . ') ';
          }
        }
      }

      $this->changed = FALSE;
      $this->stringVersion = implode($conjunction, $condition_fragments);
      $this->arguments = $arguments;
    }
  }
}
