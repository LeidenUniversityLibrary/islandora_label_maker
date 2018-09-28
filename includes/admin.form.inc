<?php
/**
 * @file
 * The admin form for the Islandora label maker module.
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
function islandora_label_maker_admin_form(array $form, array &$form_state) {
  $form = array();

  $form['admin'] = array(
    '#type' => 'fieldset',
    '#title' => t('Islandora Label Maker Configuration'),
    '#collapsible' => false,
  );

  $form['admin']['change_label_on_modify'] = array(
    '#type' => 'checkbox',
    '#title' => t('Make a new label when the Islandora object changes.'),
  );

  $form['admin']['label_configs'] = array(
    '#theme_wrappers' => array('fieldset'),
    '#tree' => TRUE,
    '#title' => t('Label Configurations'),
    '#description' => t('For every type of object a different label can be configurated. The type of object is determined by a Solr query. The first configuration that has a matching Solr query will be used.'),
    '#theme' => 'islandora_label_maker_config_table_drag_components',
  );

  for ($i = 1; $i <= number_of_label_configs() + 1; $i++) {
    $form['admin']['label_configs']["group_$i"] = array();
    $element =& $form['admin']['label_configs']["group_$i"];

    $element['#weight'] = $i;
    $element['solr_query'] = array(
      '#type' => 'textfield',
      '#default_value' => variable_get('islandora_label_maker_label_config_' . $i . '_solr_query'),
      '#size' => 60,
      '#maxlength' => 500,
    );
    $element['label_template'] = array(
      '#type' => 'textfield',
      '#default_value' => variable_get('islandora_label_maker_label_config_' . $i . '_label_template'),
      '#size' => 60,
      '#maxlength' => 500,
    );
    $element['weight'] = array(
      '#type' => 'textfield',
      '#default_value' => $element['#weight'],
      '#size' => 3,
      '#attributes' => array(
        'class' => array(
          'item-row-weight',
        ),
      ),
    );
    $element['active'] = array(
      '#type' => 'checkbox',
      '#default_value' => variable_get('islandora_label_maker_label_config_' . $i . '_active'),
    );    
  }

  $form['admin']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save configuration'),
    '#submit' => array('islandora_label_maker_admin_form_submit'),
    '#validate' => array('islandora_label_maker_admin_form_validate'),
  );

  return $form;
}

function islandora_label_maker_admin_form_validate(array $form, array &$form_state) {
  for ($i = 1; $i <= number_of_label_configs() + 1; $i++) {
    $solrquery = $form_state['values']['label_configs']["group_$i"]['solr_query'];
    $labeltemplate = $form_state['values']['label_configs']["group_$i"]['label_template'];

    if (isset($solrquery) && strlen($solrquery) > 0) {
      if (!(isset($labeltemplate) && strlen($labeltemplate) > 0)) {
        form_set_error('label_template', t('Label configuration %item has a Solr query (%query) but not a label template', array('%item' => $i, '%query' => $solrquery)));
      }
      $qp = new IslandoraSolrQueryProcessor();
      $qp->buildQuery($solrquery);
      $qp->executeQuery(false);
      $r = $qp->islandoraSolrResult;
      if ($r === NULL) {
        form_set_error('label_template', t('Label configuration %item has an invalid Solr query (%query)', array('%item' => $i, '%query' => $solrquery)));
      }
    }
    if (isset($labeltemplate) && strlen($labeltemplate) > 0) {
      if (!(isset($solrquery) && strlen($solrquery) > 0)) {
        form_set_error('label_template', t('Label configuration %item has a label template but not a Solr query', array('%item' => $i)));
      }
    }
  }
}

function islandora_label_maker_admin_form_submit(array $form, array &$form_state) {
  $vscounter = 1;
  $labelconfigs = array_values($form_state['values']['label_configs']);
  usort($labelconfigs, 'drupal_sort_weight');
  for ($i = 0; $i <= 99; $i++) {
    if (isset($labelconfigs[$i]['solr_query']) && strlen($labelconfigs[$i]['solr_query']) > 0) {
      variable_set('islandora_label_maker_label_config_' . $vscounter . '_solr_query', $labelconfigs[$i]['solr_query']);
      variable_set('islandora_label_maker_label_config_' . $vscounter . '_label_template', $labelconfigs[$i]['label_template']);
      variable_set('islandora_label_maker_label_config_' . $vscounter . '_active', $labelconfigs[$i]['active']);
      $vscounter += 1;
    }
  }
  for ($i = $vscounter; $i <= 99; $i++) {
    variable_del('islandora_label_maker_label_config_' . $i . '_solr_query');
    variable_del('islandora_label_maker_label_config_' . $i . '_label_template');
    variable_del('islandora_label_maker_label_config_' . $i . '_active');
  }
  drupal_set_message(t('The settings have been saved!'));
}

function number_of_label_configs() {
  $nr_of_configs = 1;
  for ($i = 1; $i <= 99; $i++) {
    $solrquery = variable_get('islandora_label_maker_label_config_' . $i . '_solr_query');
    $labeltemplate = variable_get('islandora_label_maker_label_config_' . $i . '_label_template');
    if (isset($solrquery) && strlen($solrquery) > 0) {
      $nr_of_configs = $i;
    }
    elseif (isset($labeltemplate) && strlen($labeltemplate) > 0) {
      $nr_of_configs = $i;
    }
  }
  return $nr_of_configs;
}
