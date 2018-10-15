<?php
/**
 * @file
 * The generate label form for the Islandora label maker module.
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
 * Implementation of a form.
 *
 * @see drupal_get_form
 */
function islandora_label_maker_generate_form(array $form, array &$form_state, AbstractObject $object) {
  module_load_include('inc', 'islandora_label_maker', 'includes/utilities');

  $form = array();

  $newlabels = array();
  $newlabels[$object->id] = array('oldlabel' => $object->label, 'newlabel' => islandora_label_maker_label_for_object_id($object->id));

  $related = islandora_label_maker_related_solrobj_for_object($object->id);
  foreach ($related as $relsolrobj) {
    $relid = $relsolrobj['PID'];
    $relobj = islandora_object_load($relid);
    $newlabels[$relid] = array('oldlabel' => $relobj->label, 'newlabel' => islandora_label_maker_label_for_object_id($relid));
  }

  $form['pid'] = array(
    '#type' => 'hidden',
    '#value' => $object->id,
  );

  $form['generate'] = array(
    '#type' => 'fieldset',
    '#title' => (count($related) > 0)?t('Generate New Labels'):t('Generate New Label'),
    '#collapsible' => false,
  );

  $form['generate']['table'] = array(
    '#theme_wrappers' => array('fieldset'),
    '#tree' => TRUE,
    '#title' => t(''),
    '#theme' => 'islandora_label_maker_generate_table_components',
  );

  foreach ($newlabels as $id => $label) {
    $form['generate']['table'][$id] = array();
    $element =& $form['generate']['table'][$id];

    $element['active'] = array(
      '#type' => 'checkbox',
      '#default_value' => (($label['newlabel'] !== false) && ($label['oldlabel'] != $label['newlabel'])),
    );
    $element['newlabel'] = array(
      '#type' => 'textfield',
      '#default_value' => $label['newlabel'],
      '#size' => 100,
      '#maxlength' => 255,
    );
    $element['oldlabel'] = array(
      '#markup' => '<SPAN style="display: inline-block; min-width:40em;">' . $label['oldlabel'] . '</SPAN>',
    );
    $element['objectid'] = array(
      '#markup' => $id,
    );
  }

  $form['admin']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Change marked labels'),
    '#submit' => array('islandora_label_maker_generate_form_submit'),
    '#validate' => array('islandora_label_maker_generate_form_validate'),
  );
  $form['admin']['cancel'] = array(
    '#type' => 'submit',
    '#value' => t('Cancel'),
    '#submit' => array('islandora_label_maker_generate_form_cancel'),
  );

  return $form;
}

function islandora_label_maker_generate_form_validate(array $form, array &$form_state) {
  foreach ($form_state['values']['table'] as $id => $fields) {
    if ($fields['active']) {
      $newlabel = trim($fields['newlabel']);
      if (strlen($newlabel) === 0) {
        form_set_error('newlabel', t('Empty label is not allowed.'));
      }
    }
  }
}

function islandora_label_maker_generate_form_submit(array $form, array &$form_state) {
  $changecount = array();
  foreach ($form_state['values']['table'] as $id => $fields) {
    if ($fields['active']) {
      $newlabel = trim($fields['newlabel']);
      $object = islandora_object_load($id);
      if ($object && $object->label !== $newlabel) {
        $object->label = $newlabel;
        $changecount[$object->id] = $object->label;
      }
    }
  }
  if (count($changecount) > 0) {
    drupal_set_message(t("Changed the labels of the following items: @changes", array("@changes" => implode(', ', array_keys($changecount)))));
  }
  else {
    drupal_set_message(t("No labels changed."));
  }
  $form_state['redirect'] = "islandora/object/{$form_state['values']['pid']}/manage/properties";
}

function islandora_label_maker_generate_form_cancel(array $form, array &$form_state) {
  $form_state['redirect'] = "islandora/object/{$form_state['values']['pid']}/manage/properties";
}
