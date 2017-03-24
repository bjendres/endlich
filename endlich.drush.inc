<?php
/*-------------------------------------------------------------+
| ENDLICH search/replace tool in Drupal                        |
| Copyright (C) 2017 SYSTOPIA                                  |
| Author: B. Endres (endres -at- systopia.de)                  |
| http://www.systopia.de/                                      |
+--------------------------------------------------------------*/

function endlich_drush_command() {
  $items = array();

  // the key in the $items array is the name of the command.
  $items['endlich-replace'] = array(
    'description' => 'search/replace in drupal DB',
    'examples' => array(
                    'drush endlich-replace /old/drupal/path /new/drupal/path' => 'Adjust paths in the database',
                    'drush endlich "http://my-old.domain/" "https://my.new-doma.in/"' => 'Adjust stored links',
                  ),
    'options' => array(
      'search'  => 'search',
      'replace' => 'replace',
    ),
    'aliases' => array('endlich'),
  );

  return $items;
}

function drush_endlich_replace($search, $replace) {
  if (empty($search) || empty($replace)) {
    error_log("You need to provide search and replace terms");
    return;
  }

  if ($serach === $replace) {
    error_log("Nothing to do, terms are equal.");
    return;
  }

  // ok, let's go:
  error_log("REPLACING '$search' WITH '$replace'");

  $all_tables = db_query("SHOW tables");
  while ($table = $all_tables->fetch()) {
    $table_name = reset($table);

    // load columns
    $all_columns = db_query("SHOW COLUMNS FROM `{$table_name}`");
    $primary_key = NULL;
    $search_columns = array();
    while ($column = $all_columns->fetch()) {
      if ($column->Key == 'PRI') $primary_key = $column->Field;
      if (substr($column->Type, 0, 4) == 'int(') continue;
      if (substr($column->Type, 0, 8) == 'tinyint(') continue;
      if (substr($column->Type, 0, 9) == 'smallint(') continue;
      if (substr($column->Type, 0, 7) == 'bigint(') continue;
      if (substr($column->Type, 0, 5) == 'float') continue;
      if (substr($column->Type, 0, 6) == 'double') continue;
      $search_columns[] = $column->Field;
    }
    if (empty($search_columns)) continue;

    foreach ($search_columns as $search_column) {
      if ($primary_key) {
        $search_query = db_query("SELECT `{$primary_key}` AS match_key, `{$search_column}` AS match_value FROM `{$table_name}` WHERE `{$search_column}` LIKE '%{$search}%'");
      } else {
        $search_query = db_query("SELECT `{$search_column}` AS match_value FROM `{$table_name}` WHERE `{$search_column}` LIKE '%{$search}%'");
      }
      $ser_matches = 0;
      $reg_matches = 0;

      while ($match = $search_query->fetch()) {
        // check if we can potentially do do changes here
        if ($primary_key == NULL) {
          error_log("MATCH IN TABLE {$table_name} BUT NO PRIMARY KEY! CANNOT UPDATE");
          continue 3;
        }

        // try to unserialize
        $data = unserialize($match->match_value);
        if ($data == NULL) {
          // this is a regular string -> replace
          // error_log("REGULAR MATCH IN TABLE {$table_name}.{$search_column}");
          $new_value = str_replace($search, $replace, $match->match_value);
          $reg_matches += 1;
        } else {
          // this is serialised data
          // error_log("SERIALISED MATCH IN TABLE {$table_name}.{$search_column}");
          _drush_endlich_recursive_replace($data, $search, $replace);
          $new_value = serialize($data);
          $ser_matches += 1;
        }

        $query = db_query("UPDATE `{$table_name}` SET `{$search_column}` = :new_value WHERE `{$primary_key}` = :match_key",
          array(':new_value' => $new_value,
                ':match_key' => $match->match_key));
      }

      if ($ser_matches) {
        error_log("{$ser_matches} SERIALISED CHANGES IN TABLE {$table_name}.{$search_column}");
      }

      if ($reg_matches) {
        error_log("{$reg_matches} REGULAR CHANGES IN TABLE {$table_name}.{$search_column}");
      }
    }
  }
}

/**
 * recursive search/replace in object/array
 */
function _drush_endlich_recursive_replace(&$data, $search, $replace) {
  if ($data === NULL) {
    // nothing to do
  } elseif (is_string($data)) {
    // a string? simply replace it
    $data = str_replace($search, $replace, $data);
  } elseif (is_array($data) || is_object($data)) {
    // this is a structure -> replace recursively
    foreach ($data as $key => &$value) {
      _drush_endlich_recursive_replace($value, $search, $replace);
    }
  } elseif (is_numeric($data) || is_bool($data)) {
    // nothing to do
  } else {
    error_log("SERMATCH: UNKNOWN TYPE: " . gettype($data));
  }
}
