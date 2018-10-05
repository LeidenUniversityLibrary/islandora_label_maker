<?php
/**
 * @file
 * utilities.inc
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

function islandora_label_maker_add_to_label_queue($object, $datastream) {
  if (variable_get('islandora_label_maker_change_label_on_modify', false)) {
    $datastreams = variable_get('islandora_label_maker_datastreams_change');
    if (in_array($datastream->id, $datastreams)) {
      $queue = DrupalQueue::get(‘islandora_label_maker_queue’);
      $queue->createItem( array(“object_id” => $object->id, “timestamp” => time()) );
    }
  }
}

function islandora_label_maker_generate_label_for_object_id($id) {
 // generate the label, only change label if different than current label
 $newlabel = islandora_label_maker_label_for_object_id($id);
 if ($newlabel === false) {
    return;
 }
 $obj = islandora_object_load($id);
 if ($obj->label !== $newlabel) {
   $obj->label = $newlabel;
 }
}

function islandora_label_maker_label_for_object_id($id) {
  for ($i = 1; $i <= 99; $i++) {
    $active = variable_get('islandora_label_maker_label_config_' . $i . '_active');
    $solrquery = variable_get('islandora_label_maker_label_config_' . $i . '_solr_query');
    $labeltemplate = variable_get('islandora_label_maker_label_config_' . $i . '_label_template');
    if (!$active) {
      continue;
    }
    if (isset($solrquery) && strlen($solrquery) > 0) {
       $query = "PID:\"$id\" AND $solrquery";
       $solrobj = islandora_label_maker_object_for_solr_query($query);
       if ($solrobj !== null) {
         return islandora_label_maker_label_for_solr_object($solrobj, $labeltemplate);
       }
    }
    else {
      break;
    }
  }
  return false;
}

function islandora_label_maker_label_for_solr_object($solrdoc, $labeltemplate) {
  $replacements = array();
  $label = preg_replace_callback(
    '/{{\s*(.+?)\s*}}/',
    function($matches) use ($solrdoc, &$replacements) {
      $part = $matches[1];
      return islandora_label_maker_replace_part($part, $solrdoc, $replacements);
    },
    $labeltemplate);
dpm($replacements, 'replacements');
  foreach (array_reverse($replacements) as $rep) {
    $order = $rep['order'];
    $type = $rep['type'];
    $min = $rep['min'];
    $max = $rep['max'];
    $label = preg_replace_callback(
      "/>>$order>>(.*?)<<$order<</",
      function($matches) use ($type, $min, $max) {

      },
      $label
    );
  }
  return $label;
}

function islandora_label_maker_value_from_solrdoc($solrfield, $solrdoc) {
  $value = null;
  if (isset($solrdoc[$solrfield])) {
    $value = $solrdoc[$solrfield];
  }
  elseif (isset($solrdoc['solr_doc'][$solrfield])) {
    $value = $solrdoc['solr_doc'][$solrfield];
  }
  if (is_array($value)) {
    $value = $value[0];
  }
  return $value;
}

function islandora_label_maker_replace_part($part, $solrdoc, &$replacements) {
dpm($part,'part');
  $result = preg_replace_callback(
  '/^(?:([a-zA-Z0-9_-]+)|(truncate|parent|alternative|glue)\((.+)\)|"""(.*?)""")$/',
  function($matches) use ($solrdoc, &$replacements) {
dpm($matches, 'matches');
    if (isset($matches[1]) && strlen($matches[1])) {
      $solrfield = $matches[1];
      return islandora_label_maker_value_from_solrdoc($solrfield, $solrdoc);
    }
    elseif (isset($matches[2]) && strlen($matches[2])) {
      $func = $matches[2];
      $argstr = $matches[3] . ',';
      preg_match_all('/(?:([a-zA-Z0-9_-]+|""".+?"""|truncate\(.+\)|parent\(.+\)|alternative\(.+\)|glue\(.+\))\s*,\s*)/A', $argstr, $argmatches);
      $args = $argmatches[1];
      if ($func === 'truncate') {
        $value = islandora_label_maker_replace_part($args[0], $solrdoc, $replacements);
        $order = count($replacements) + 1;
        $key = ">>$order>>$value<<$order<<";
        $replacements[] = array('type' => $args[1], 'order' => $order, 'max' => (isset($args[2])?$args[2]:255), 'min' => (isset($args[3])?$args[3]:0));
        return $key;
      }
      elseif ($func === 'parent') {
        $parentid = islandora_label_maker_replace_part($args[0], $solrdoc, $replacements);
        $parentid = str_replace('info:fedora/', '', $parentid);
        $query = "PID:\"$parentid\"";
        $psolrdoc = islandora_label_maker_object_for_solr_query($query);
        $solrfield = $args[1];
        return islandora_label_maker_value_from_solrdoc($solrfield, $psolrdoc);
      }
      elseif ($func === 'alternative') {
        foreach ($args as $arg) {
          $value = islandora_label_maker_replace_part($arg, $solrdoc, $replacements);
          if (isset($value) && strlen($value)) {
            return $value;
          }
        }
        return '';
      }
      elseif ($func === 'glue') {
        $result = '';
        foreach ($args as $arg) {
          $value = islandora_label_maker_replace_part($arg, $solrdoc, $replacements);
          if (isset($value) && strlen($value)) {
            $result .= $value;
          }
        }
        return $result;
      }
    }
    elseif (isset($matches[4]) && strlen($matches[4])) {
      return $matches[4];
    }
  },
  $part);
  return $result;
}

function islandora_label_maker_object_for_solr_query($query) {
  $qp = new IslandoraSolrQueryProcessor();
  $qp->buildQuery($query);
  $qp->solrLimit = 1;
  $qp->solrStart = 0;
  $qp->executeQuery(false);
  $r = $qp->islandoraSolrResult;
  if (isset($r['response']['numFound']) && $r['response']['numFound'] > 0) {
    return $r['response']['objects'][0];
  }
  return null;
}
