<?php

/**
 * @file
 * theme/theme.inc
 *
 *
 *  Copyright 2018 Leiden University Library
 *
 *  This file is part of islandora_label_maker.
 *
 *  islandora_label_maker is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
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
 *
 */

/**
 * Returns HTML for the metadata_table_drag_components weight table.
 *
 * @param array $vars
 *   An associative array containing:
 *   - element: Table element array.
 *   - theme_hook_original: Initial theme hook.
 *
 * @return string
 *   Themed table html.
 *
 * @ingroup themeable
 */
function theme_islandora_label_maker_config_table_drag_components($vars) {
  $element = $vars['element'];
  $clean_id = drupal_html_id('islandora_label_maker_config_table');
  drupal_add_tabledrag($clean_id, 'order', 'sibling', 'item-row-weight');

  $header = array(
    'active' => t('Active'),
    'solr_query' => t('Solr query'),
    'label_template' => t('Label template'),
    'weight' => t('Weight'),
  );

  $rows = array();
  foreach (element_children($element) as $key) {
    $row = array();
    $row['data'] = array();
    foreach ($header as $fieldname => $title) {
      $row['data'][] = drupal_render($element[$key][$fieldname]);
      $row['class'] = array('draggable');
    }
    $rows[] = $row;
  }

  return theme('table', array(
    'header' => $header,
    'rows' => $rows,
    'attributes' => array('id' => $clean_id),
  ));
}

function theme_islandora_label_maker_generate_table_components($vars) {
  $element = $vars['element'];

  $header = array(
    'active' => t('Change'),
    'newlabel' => t('Newly Generated Label'),
    'oldlabel' => t('Old Label'),
    'objectid' => t('Object'),
  );

  $rows = array();
  foreach (element_children($element) as $key) {
    $row = array();
    $row['data'] = array();
    foreach ($header as $fieldname => $title) {
      $row['data'][] = drupal_render($element[$key][$fieldname]);
    }
    $rows[] = $row;
  }

  $result = theme('table', array(
    'header' => $header,
    'rows' => $rows,
  ));

  return $result;
}

