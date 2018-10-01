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
    $solrquery = variable_get('islandora_label_maker_label_config_' . $i . '_solr_query');
    $labeltemplate = variable_get('islandora_label_maker_label_config_' . $i . '_label_template');
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
  $label = preg_replace_callback(
    '/{{\s*(?:(parent)(?:\(([^)]+)\))?\.)?([a-zA-Z0-9_]+)(?:\|(truncate)\((.+)\))?\s*}}/',
    function($matches) use ($solrdoc) {
      $value = '';
      if (isset($matches[1]) && strlen($matches[1])) {
        $value = islandora_label_maker_parent_solr_field($solrdoc['PID'], $matches[3], $matches[2]);
      }
      else {
        $solrfield = $matches[3];
        if (isset($solrdoc[$solrfield])) {
          $value = $solrdoc[$solrfield];
        }
        elseif (isset($solrdoc['solr_doc'][$solrfield])) {
          $value = $solrdoc['solr_doc'][$solrfield];
        }
      }
      if (isset($matches[4]) && strlen($matches[4])) {
        $value = islandora_label_maker_apply_function($value, $solrdoc, $matches[4], $matches[5]);
      }
      return $value;
    },
    $labeltemplate);

  if (mb_strlen($label) > 255) {
    $label = mb_substr($label, 0, 252) . '...';
  }
  return $label;
}

function islandora_label_maker_parent_solr_field($childid, $solrfield, $solrparentkey = null) {
  $solrquery = '';
  if (isset($solrparentkey)) {
    $solrquery = "$solrparentkey:\"$childid\" OR $solrparentkey:\"info:fedora/$childid\"";
  }
  else {
    $fullcid = "info:fedora/$childid";
    $solrquery = "RELS_EXT_isConstituentOf_uri_ms:\"$fullcid\" OR RELS_EXT_isMemberOf_uri_ms:\"$fullcid\" OR RELS_EXT_isPageOf_uri_ms:\"$fullcid\""; 
  }
  $solrdoc = islandora_label_maker_object_for_solr_query($solrquery);
  if ($solrdoc) {
    if (isset($solrdoc[$solrfield])) {
      return $solrdoc[$solrfield];
    }
    elseif (isset($solrdoc['solr_doc'][$solrfield])) {
      return $solrdoc['solr_doc'][$solrfield];
    }
  }
  return '';
}

function islandora_label_maker_apply_function($value, $solrdoc, $function, $args) {
  $arguments = array();
  if (isset($args)) {
    $arguments = explode(',', $args);
    array_walk($arguments, function(&$value) { $value = trim($value); } );
  }
  if ($function === 'truncate') {
    if (isset($arguments[0])) {
      if (mb_strlen($value) > $arguments[0]) {
        $len = $arguments[0];
        if (isset($arguments[1])) {
          $solrfield = $arguments[1];
          if (isset($solrdoc[$solrfield])) {
            $v = $solrdoc[$solrfield];
          }
          elseif (isset($solrdoc['solr_doc'][$solrfield])) {
            $v = $solrdoc['solr_doc'][$solrfield];
          }
          $len = $len - mb_strlen($v); 
          if (isset($arguments[2])) {
            if ($len < $arguments[2]) {
              $len = $arguments[2];
            }
          }
        }
        $value = mb_substr($value, 0, $len) . '...';
      }
    }
  }
  return $value;
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
