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
    '#title' => t('Make a new label when any of the following datastreams change.'),
    '#default_value' => variable_get('islandora_label_maker_change_label_on_modify', false),
  );

  $form['admin']['datastreams_change'] = array(
    '#type' => 'textfield',
    '#title' => t('Datastream IDs'),
    '#default_value' => implode(' ', variable_get('islandora_label_maker_datastreams_change', array('MODS', 'DC'))),
    '#states' => array(
      'visible' => array(
        "input[name='change_label_on_modify']" => array("checked" => TRUE)
      ),
    ),
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
    $element['active'] = array(
      '#type' => 'checkbox',
      '#default_value' => variable_get('islandora_label_maker_label_config_' . $i . '_active'),
    );
    $element['solr_query'] = array(
      '#type' => 'textfield',
      '#default_value' => variable_get('islandora_label_maker_label_config_' . $i . '_solr_query'),
      '#size' => 100,
      '#maxlength' => 500,
    );
    $element['label_template'] = array(
      '#type' => 'textfield',
      '#default_value' => variable_get('islandora_label_maker_label_config_' . $i . '_label_template'),
      '#size' => 135,
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
  }

  $form['admin']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save configuration'),
    '#submit' => array('islandora_label_maker_admin_form_submit'),
    '#validate' => array('islandora_label_maker_admin_form_validate'),
  );
  $form['admin']['help'] = array(
    '#type' => 'fieldset',
    '#title' => t('Label Template Help'),
    '#collapsible' => true,
    '#collapsed' => true,
  );
  $form['admin']['help']['#description'] = <<<'EODESC'
The label template is normal text where values between double brackets ({{ }}) are replaced by a calculated value. Between the double brackets any of the following is allowed:
<UL>
  <LI><I>solr_field</I>
    <P>The value of this Solr field is used.</P>
  </LI>
  <LI>alternative( <I>something</I>, <I>another thing</I> )
    <P>If <I>something</I> is not defined, use <I>another thing</I>. Can have more than 2 arguments. The last argument can be a """constant value""" (between triple quotes).</P>
  </LI>
  <LI>glue( <I>something</I>, <I>another thing</I> )
    <P>Glues <I>something</I> and <I>another thing</I> together. Can have more than 2 arguments. Any argument can be a """constant value"""".</P>
  </LI>
  <LI>parent( <I>solr_field_containing_parent_id</I>, <I>something</I> )
    <P>Use the Solr fields of the parent of this item. <I>solr_field_containing_parent_id</I> should be a Solr field that contains the identifier of the parent, such as RELS_EXT_isMemberOfCollection_uri_ms. <I>something</I> can be a Solr field or an alternative.</P>
  </LI>
  <LI>truncate( <I>something</I>, <I>type</I>, <I>maximum length</I>, <I>minimum length</I> )
    <P>Truncates the value defined by <I>something</I> to the <I>maximum length</I>. Type can be begin, end, middle or inner and defines where the truncation happens. Then type inner lets you truncate a inner value based on the outer total length.
       See examples for how to use this. The value will never be truncated past the minimum length.</P>
  </LI>
</UL>
<P><EM>Examples</EM>
  <UL>
    <LI>{{ mods_titleInfo_title_s }}
      <P>Use the title from the MODS as the label of the object</P>
    </LI>
    <LI>{{ alternative( mods_titleInfo_title_s, dc.title, """No title""" ) }}
      <P>Use the title from the MODS as the label of the object, but if it does not exist use the title from the Dublin Core, or if that does not exist use "No title" (without the quotes).</P>
    </LI>
    <LI>{{ glue( parent( RELS_EXT_isMemberOf_uri_s, mods_titleInfo_title_s ), """ - """, mods_titleInfo_title_s ) }}
      <P>Use the title from the MODS of the parent, combined with " -" and the title from the MODS as the label of the object</P>
    </LI>
    <LI>{{ truncate( glue( truncate( parent( RELS_EXT_isMemberOf_uri_s, mods_titleInfo_title_s ), end, 200, 100 ), """ - """, mods_titleInfo_title_s ), inner, 255 ) }}
      <P>The same as above, but truncates the whole string to 255 characters at most. This is done by truncating the title of the parent to 200 characters or less, in such a way that at least 100 characters remain in the title.<BR/>Note: it is possible that the resulting string is more than 255 characters if the title is more than 155 characters long; the parent title will not be truncated to less than 100 characters, so if the title is longer than 155 the total will end up being over 255 characters long. This can be fixed by also setting a truncate to the title.</P>
    </LI>
</P>
EODESC;
  return $form;
}

function islandora_label_maker_admin_form_validate(array $form, array &$form_state) {
  module_load_include('inc', 'islandora_label_maker', 'includes/utilities');

  $solrlabelfield = variable_get('islandora_solr_object_label_field', 'fgs_label_s');
  $haslabelconfig = false;
  for ($i = 1; $i <= number_of_label_configs() + 1; $i++) {
    $solrquery = $form_state['values']['label_configs']["group_$i"]['solr_query'];
    $labeltemplate = $form_state['values']['label_configs']["group_$i"]['label_template'];
    $active = $form_state['values']['label_configs']["group_$i"]['active'];

    if (isset($solrquery) && strlen($solrquery) > 0) {
      if (!(isset($labeltemplate) && strlen($labeltemplate) > 0)) {
        form_set_error('label_template', t('Label configuration %item has a Solr query (%query) but not a label template.', array('%item' => $i, '%query' => $solrquery)));
      }
      else {
        $haslabelconfig = true;
      }
      $qp = new IslandoraSolrQueryProcessor();
      $qp->buildQuery($solrquery);
      $qp->executeQuery(false);
      $r = $qp->islandoraSolrResult;
      if ($r === NULL) {
        form_set_error('solr_query', t('Label configuration %item has an invalid Solr query (%query).', array('%item' => $i, '%query' => $solrquery)));
      }
      if ($active) {
        if ($r['response']['numFound'] === 0) {
          form_set_error('solr_query', t('Label configuration %item has a Solr query (%query) that does not match anything.', array('%item' => $i, '%query' => $solrquery)));
        }
      }
    }
    if (isset($labeltemplate) && strlen($labeltemplate) > 0) {
      if (!(isset($solrquery) && strlen($solrquery) > 0)) {
        form_set_error('solr_query', t('Label configuration %item has a label template but not a Solr query.', array('%item' => $i)));
      }
      if (preg_match("/{{\s*$solrlabelfield\s*}}/", $labeltemplate) === 1) {
        form_set_error('label_template', t('Label configuration %item has a label template with the Solr label field (%solrlabelfield).', array('%item' => $i, '%solrlabelfield' => $solrlabelfield)));
      }
      try {
        $solrdoc = array();
        if (isset($r['response']['objects'][0])) {
          $solrdoc = $r['response']['objects'][0];
        }
        $label = islandora_label_maker_label_for_solr_object($solrdoc, $labeltemplate);
        if (strlen($label) === 0 || $label === $labeltemplate) {
          form_set_error('label_template', t('Label configuration %item has a label template that will produce unexpected results in some cases.', array('%item' => $i)));
        }
      }
      catch (Exception $e) {
        form_set_error('label_template', t('Label configuration %item has a malformatted label template: !error.', array('%item' => $i, '!error' => $e->getMessage())));
      }
    }
  }
  if ($form_state['values']['change_label_on_modify']) {
    if (!$haslabelconfig) {
      form_set_error('change_label_on_modify', t('Cannot make a label without label configuration.'));
    }
    $datastreams = explode(' ', $form_state['values']['datastreams_change']);
    if (count($datastreams) == 0 || strlen($datastreams[0]) == 0) {
      form_set_error('datastreams_change', t('Fill out datastream IDs, e.g. MODS DC.'));
    }
  }
}

function islandora_label_maker_admin_form_submit(array $form, array &$form_state) {
  variable_set('islandora_label_maker_change_label_on_modify', $form_state['values']['change_label_on_modify']);
  variable_set('islandora_label_maker_datastreams_change', explode(' ', $form_state['values']['datastreams_change']));
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
  $nr_of_configs = 0;
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
