<?php
/**
 * @file
 * Functions for managing label creation via drush.
 *
 *
 *  Copyright 2018 Leiden University Library
 *
 *  This file is part of islandora_label_maker.
 *
 *  islandora_label_maker is free software: you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * Implements hook_drush_command().
 */
function islandora_label_maker_drush_command() {
  $items['islandora_label_maker_list'] = array(
    'description' => 'List the new labels for the identifiers. If the label can not be made, an error is generated.',
    'options' => array(
      'ids_file' => 'The absolute filepath to a file containing a list of Islandora identifiers that will be used.',
      'format' => 'Optionally, the format of the output. Allowed values are: identifier, oldlabel, newlabel. Other values will be unchanged. Defaults to identifier;newlabel;oldlabel',
      'include_related' => 'Optionally, also add the related items to the queue',
      'skip_unchanged' => 'Optionally, skip when the new label is the same as the old label',
      'silent' => 'Optionally, does not report when a label can not be made.',
    ),
    'aliases' => array('ilmlist'),
    'examples' => array(
      'drush --user=admin islandora_label_maker_list --ids_file=/path/to/idsfile.csv',
      'drush --user=admin ilmlist --ids_file=/path/to/idsfile.csv --format="identifier;newlabel" --skip_unchanged',
    ),
  );
  $items['islandora_label_maker_queue'] = array(
    'description' => 'Adds the items to the queue, to be generated later.',
    'options' => array(
      'ids_file' => 'The absolute filepath to a file containing a list of Islandora identifiers that will be used.',
      'include_related' => 'Optionally, also add the related items to the queue',
    ),
    'aliases' => array('ilmqueue'),
    'examples' => array(
      'drush --user=admin islandora_label_maker_qeueu --ids_file=/path/to/idsfile.csv',
      'drush --user=admin ilmqeueu --ids_file=/path/to/idsfile.csv',
    ),
  );
  $items['islandora_label_maker_update'] = array(
    'description' => 'Updates the labels for the identifiers. If the label can not be made, an error is generated. Only run this when you are sure!',
    'options' => array(
      'ids_file' => 'The absolute filepath to a file containing a list of Islandora identifiers that will be used.',
      'include_related' => 'Optionally, also add the related items to the queue',
      'silent' => 'Optionally, does not report when a label can or should not be updated.',
    ),
    'aliases' => array('ilmupdate'),
    'examples' => array(
      'drush --user=admin islandora_label_maker_update --ids_file=/path/to/idsfile.csv',
      'drush --user=admin ilmupdate --ids_file=/path/to/idsfile.csv --silent',
    ),
  );
  return $items;
}

/**
 * Implements drush_hook_COMMAND_validate().
 */
function drush_islandora_label_maker_list_validate() {
  return _islandora_label_maker_drush_validate();
}

/**
 * Implements drush_hook_COMMAND_validate().
 */
function drush_islandora_label_maker_queue_validate() {
  return _islandora_label_maker_drush_validate();
}

/**
 * Implements drush_hook_COMMAND_validate().
 */
function drush_islandora_label_maker_update_validate() {
  return _islandora_label_maker_drush_validate();
}

/**
 * Helper function for validation.
 */
function _islandora_label_maker_drush_validate() {
  $idsfile = drush_get_option('ids_file');
  if (!isset($idsfile)) {
    return drush_set_error("Argument ids_file missing");
  }
  if (!file_exists($idsfile)) {
    return drush_set_error("ids_file '$idsfile' does not exist");
  }
}

/**
 * Implements drush_hook_COMMAND().
 */
function drush_islandora_label_maker_list() {
  module_load_include('inc', 'islandora_label_maker', 'includes/utilities');

  $idsfile = drush_get_option('ids_file');
  $includerelated = drush_get_option('include_related', FALSE);
  $skipunchanged = drush_get_option('skip_unchanged', FALSE);
  $silent = drush_get_option('silent', FALSE);

  $ids = islandora_label_maker_ids_from_file($idsfile);

  $columns = explode(';', drush_get_option('format', 'identifier;newlabel;oldlabel'));
  $columns = array_combine($columns, $columns);

  $f = fopen('php://stdout', 'w');

  foreach ($ids as $id) {
    islandora_label_maker_putcsv_columns_for_id($f, $columns, $id, $skipunchanged, $silent);
    if ($includerelated) {
      $related = islandora_label_maker_related_solrobj_for_object($id);
      foreach ($related as $solrrel) {
        $relid = $solrrel['PID'];
        islandora_label_maker_putcsv_columns_for_id($f, $columns, $relid, $skipunchanged, $silent);
      }
    }
  }
  fclose($f);
}

/**
 * Helper function for printing of CSV line to file pointer.
 *
 * @param resource $f
 *   A file pointer to write to.
 * @param array $columns
 *   An associated array containing column names for both keys and values.
 * @param string $id
 *   The id of the object used.
 * @param bool $skipunchanged
 *   If TRUE, don't write anything if the new label is the same as the old.
 * @param bool $silent
 *   If TRUE, does not log when the label cannot be generated.
 *
 * @return bool
 *   True if handled succesfully, FALSE if printing failed.
 */
function islandora_label_maker_putcsv_columns_for_id($f, $columns, $id, $skipunchanged = FALSE, $silent = FALSE) {
  $newlabel = islandora_label_maker_label_for_object_id($id);
  if ($newlabel === FALSE) {
    if (!$silent) {
      drush_log(t("Cannot generate label for @id", array('@id' => $id)), 'warning');
    }
    return TRUE;
  }
  $fields = $columns;
  $object = NULL;
  if ($skipunchanged) {
    $object = islandora_object_load($id);
    if ($object->label === $newlabel) {
      return TRUE;
    }
  }
  if (isset($columns['oldlabel'])) {
    if (!isset($object)) {
      $object = islandora_object_load($id);
    }
    $fields['oldlabel'] = $object->label;
  }
  if (isset($columns['identifier'])) {
    $fields['identifier'] = $id;
  }
  $fields['newlabel'] = $newlabel;

  if (fputcsv($f, $fields, ';') === FALSE) {
    return FALSE;
  }
  return TRUE;
}

/**
 * Implements drush_hook_COMMAND().
 */
function drush_islandora_label_maker_queue() {
  module_load_include('inc', 'islandora_label_maker', 'includes/utilities');

  if (!variable_get('islandora_label_maker_change_label_on_modify', FALSE)) {
    if (!drush_confirm(t("Automatic label generation is disabled in the configuration. Are you sure you want to manually add these ids to the queue and make new labels for them?"))) {
      drush_user_abort();
    }
  }

  $idsfile = drush_get_option('ids_file');
  $includerelated = drush_get_option('include_related', FALSE);

  $ids = islandora_label_maker_ids_from_file($idsfile);

  foreach ($ids as $id) {
    islandora_label_maker_add_to_label_queue($id, 0, $includerelated);
  }
}

/**
 * Implements drush_hook_COMMAND().
 */
function drush_islandora_label_maker_update() {
  module_load_include('inc', 'islandora_label_maker', 'includes/utilities');

  $idsfile = drush_get_option('ids_file');
  $silent = drush_get_option('silent', FALSE);
  $includerelated = drush_get_option('include_related', FALSE);

  $ids = islandora_label_maker_ids_from_file($idsfile);

  foreach ($ids as $id) {
    islandora_label_maker_drush_update_label_for_object_id($id, $silent);
    if ($includerelated) {
      $related = islandora_label_maker_related_solrobj_for_object($id);
      foreach ($related as $solrrel) {
        $relid = $solrrel['PID'];
        islandora_label_maker_drush_update_label_for_object_id($relid, $silent);
      }
    }
  }
}

/**
 * Helper function for updating label.
 */
function islandora_label_maker_drush_update_label_for_object_id($id, $silent) {
  $result = islandora_label_maker_update_label_for_object_id($id);
  if ($result === FALSE) {
    if (!$silent) {
      drush_log(t("Cannot generate label for @id", array('@id' => $id)), 'warning');
    }
  }
  else {
    if ($result['oldlabel'] === $result['newlabel']) {
      if (!$silent) {
        drush_log(t("Label for @id is already up-to-date.", array('@id' => $id)));
      }
    }
    else {
      drush_log(t(
        "Changed label for @id from '@oldlabel' to '@newlabel'.",
        array(
          '@id' => $id,
          '@oldlabel' => $result['oldlabel'],
          '@newlabel' => $result['newlabel'],
        )
      ));
    }
  }
}

/**
 * Helper function for retrieving ids from file.
 */
function islandora_label_maker_ids_from_file($idsfile) {
  $foundids = array();
  $idsstr = file_get_contents($idsfile);
  $fieldseparator = "";
  $ids = str_getcsv($idsstr, "\n");
  foreach ($ids as $index => $id) {
    if (strlen($id) == 0) {
      continue;
    }
    if ($fieldseparator === "") {
      $fieldseparator = "\t";
      $comps = str_getcsv($id, $fieldseparator);
      if (count($comps) > 1) {
        // Fields are separated by a tab.
      }
      else {
        $fieldseparator = ",";
        $comps = str_getcsv($id, $fieldseparator);
        if (count($comps) > 1) {
          // Fields are separated by a comma.
        }
        else {
          $fieldseparator = ";";
          $comps = str_getcsv($id, $fieldseparator);
          if (count($comps) > 1) {
            // Fields are separated by a semicolon.
          }
          else {
            // Fields are not separated.
            $fieldseparator = FALSE;
          }
        }
      }
    }
    if ($fieldseparator !== FALSE) {
      $comps = str_getcsv($id, $fieldseparator);
      if (count($comps) > 1) {
        $id = $comps[0];
      }
    }
    $foundids[] = $id;
  }
  return $foundids;
}
