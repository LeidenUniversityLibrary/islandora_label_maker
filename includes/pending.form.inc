<?php
/**
 * @file
 * The pending form for the Islandora label maker module.
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
function islandora_label_maker_pending_form(array $form, array &$form_state) {
  module_load_include('inc', 'islandora_label_maker', 'includes/utilities');

  $form = array();

  $queue = islandora_label_maker_get_queue();
  $queuesize = $queue->numberOfItems();
  if ($queuesize > 0) {
    $description = format_plural($queuesize, "There might be 1 item pending (but maybe it is processed already).", "There are approximately !count items pending.", array('!count' => $queuesize));
  }
  else {
    $description = t("There seem to be no items pending.");
  }

  $form['admin'] = array(
    '#type' => 'fieldset',
    '#title' => t('Islandora Label Maker Pending Labels'),
    '#description' => $description,
    '#collapsible' => false,
  );

  $form['admin']['spacer'] = array(
    '#markup' => '<p></p>',
  );

  $form['admin']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Make labels for pending items'),
    '#submit' => array('islandora_label_maker_pending_form_submit'),
  );

  return $form;
}

function islandora_label_maker_pending_form_submit(array $form, array &$form_state) {
  module_load_include('inc', 'islandora_label_maker', 'includes/utilities');

  $queues = module_invoke('islandora_label_maker', 'cron_queue_info');
  drupal_alter('cron_queue_info', $queues);

  foreach ($queues as $queuename => $info) {
    $itemsdone = 0;
    $func = $info['worker callback'];
    $starttime = time();
    $allowedtime = (isset($info['time'])?$info['time']:10);
    $endtime = $starttime + $allowedtime;
    $queue = DrupalQueue::get($queuename);
    if ($queue) {
      while ((time() < $endtime) && ($item = $queue->claimItem())) {
        try {
          $func($item->data); 
          $queue->deleteItem($item);
          $itemsdone++;
        }
        catch (Exception $e) {
          drupal_set_message($e->getMessage(), 'error', FALSE);
        }
      }
    }
    if ($itemsdone === 0) {
      drupal_set_message(t("No items found to process."));
    }
    else {
      drupal_set_message(format_plural($itemsdone, "Generated 1 new label in the time allowed (!allowed seconds).", "Generated !count new labels in the time allowed (!allowed seconds).", array('!count' => $itemsdone, '!allowed' => $allowedtime)));
    }
  }
}
