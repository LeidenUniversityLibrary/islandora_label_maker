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
  $truncates = array();
  $label = preg_replace_callback(
    '/{{\s*(.+?)\s*}}/',
    function($matches) use ($solrdoc, &$truncates) {
      $part = $matches[1];
      return islandora_label_maker_replace_part($part, $solrdoc, $truncates);
    },
    $labeltemplate);
//dpm($truncates, 'truncates');
  foreach (array_reverse($truncates) as $rep) {
    $order = $rep['order'];
    $type = $rep['type'];
    $min = $rep['min'];
    $max = $rep['max'];
    $label = preg_replace_callback(
      "/>>$order>>(.*?)<<$order<</",
      function($matches) use (&$truncates, $type, $min, $max) {
        $value = $matches[1];
//dpm($value, 'preg match replce for type ' . $type);
        if ($type === 'inner') {
          if (preg_match_all('/>>(\d+)>>(.*?)<<\1<</', $value, $innermatches) > 0) {
      //dpm($innermatches, 'innermatches');
            $valuelength = mb_strlen($value);
            foreach ($innermatches[1] as $im) {
              $valuelength -= mb_strlen($im) * 2 + 8;
            }
            if ($valuelength > $max) {
//dpm(($valuelength - $max), "$valuelength > $max");
              foreach ($innermatches[1] as $index => $im) {
                $trunc = $truncates[$im];
                if (mb_strlen($innermatches[2][$index]) > $trunc['max']) {
                  $valuelength -= (mb_strlen($innermatches[2][$index]) - $trunc['max']);
                  $truncates[$im]['len'] = $trunc['max'];
                }
                else {
                  $truncates[$im]['len'] = mb_strlen($innermatches[2][$index]);
                }
              }
//dpm($valuelength, 'value length');
              while ($valuelength > $max) {
//dpm(($valuelength - $max), "$valuelength > $max");
//dpm($truncates, 'truncs');
                $madeshorter = false;
                foreach ($innermatches[1] as $index => $im) {
                  $trunc = $truncates[$im];
                  if ($truncates[$im]['len'] > $trunc['min']) {
                    $valuelength--;
                    $truncates[$im]['len']--;
                    $madeshorter = true;
                  }
                }
                if (!$madeshorter) {
                  break;
                }
              }
              foreach ($innermatches[1] as $index => $im) {
//dpm($value, "before $index");
                $value = preg_replace("/>>$im>>.*?<<$im<</", islandora_label_maker_truncate($innermatches[2][$index], $truncates[$im]['type'], $truncates[$im]['len']), $value);
//dpm($value, "after $index");
              }
            }
          }
        }
        return $value;
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

function islandora_label_maker_replace_part($part, $solrdoc, &$truncates) {
//dpm($part,'part');
  $result = preg_replace_callback(
  '/^\s*(?:([a-zA-Z0-9_-]+)|(truncate|parent|alternative|glue)\((.+)\)|"""(.*?)""")\s*$/',
  function($matches) use ($solrdoc, &$truncates, $part) {
//dpm($matches, 'matches');
    $result = '';
    if (isset($matches[1]) && strlen($matches[1])) {
      $solrfield = $matches[1];
      $result = islandora_label_maker_value_from_solrdoc($solrfield, $solrdoc);
    }
    elseif (isset($matches[2]) && strlen($matches[2])) {
      $func = $matches[2];
      $argstr = $matches[3];
      $args = islandora_label_maker_args_split($argstr);
      //dpm($args, 'split args');
      //$funcargs = '\\((?:[^()]+|\\)';
      //preg_match_all('/(?:([a-zA-Z0-9_-]+|""".+?"""|truncate\(.+\)|parent\(.+\)|alternative\(.+\)|glue\(.+\))\s*,\s*)/A', $argstr, $argmatches);
      //$args = $argmatches[1];
      if ($func === 'truncate') {
        $value = islandora_label_maker_replace_part($args[0], $solrdoc, $truncates);
        $order = count($truncates) + 1;
        $key = ">>$order>>$value<<$order<<";
        $truncates[$order] = array('type' => $args[1], 'order' => $order, 'max' => (isset($args[2])?$args[2]:255), 'min' => (isset($args[3])?$args[3]:0));
dpm($truncates, 'added to truncates for ' . $part);
        $result = $key;
      }
      elseif ($func === 'parent') {
        $parentid = islandora_label_maker_replace_part($args[0], $solrdoc, $truncates);
        $parentid = str_replace('info:fedora/', '', $parentid);
        $query = "PID:\"$parentid\"";
        $psolrdoc = islandora_label_maker_object_for_solr_query($query);
        $parentfield = $args[1];
        $result = islandora_label_maker_replace_part($parentfield, $psolrdoc, $truncates);
      }
      elseif ($func === 'alternative') {
//dpm($args, 'alternative');
        foreach ($args as $arg) {
          $value = islandora_label_maker_replace_part($arg, $solrdoc, $truncates);
//dpm($arg, "alternative value = '$value'");
          if (isset($value) && strlen($value)) {
            $result = $value;
            break;
          }
        }
      }
      elseif ($func === 'glue') {
//dpm($args, 'glue');
        foreach ($args as $arg) {
          $value = islandora_label_maker_replace_part($arg, $solrdoc, $truncates);
          if (isset($value) && strlen($value)) {
            $result .= $value;
          }
        }
      }
    }
    elseif (isset($matches[4]) && strlen($matches[4])) {
      $result = $matches[4];
    }
dpm($result, "result voor part $part");
    return $result;
  },
  $part);
  return $result;
}

function islandora_label_maker_args_split($argstr) {
  $args = array();
  $carg = '';
  $brackets = 0;
  foreach (preg_split('//u', $argstr) as $char) {
    if ($char === '(') {
      $brackets++;
      $carg .= $char;
    }
    elseif ($char === ')') {
      $brackets--;
      $carg .= $char;
    }
    elseif ($char === ',' && $brackets === 0) {
      $args[] = trim($carg);
      $carg = '';
    }
    else {
      $carg .= $char;
    }
  }
  $args[] = trim($carg);
  if ($brackets > 0) {
    throw new Exception("Too many opening brackets in $argstr");
  }
  if ($brackets < 0) {
    throw new Exception("Too many closing brackets in $argstr");
  }

  return $args;
}

function islandora_label_maker_truncate($value, $type, $maxlength, $omissionsign = '...') {
dpm(array($value, $type, $maxlength), 'truncating');
  if (mb_strlen($value) > $maxlength) {
    if ($type === 'end') {
      $value = mb_substr($value, 0, $maxlength - mb_strlen($omissionsign)) . $omissionsign;
    }
    elseif ($type === 'begin') {
      $value = $omissionsign . mb_substr($value, mb_strlen($value) - $maxlength + mb_strlen($omissionsign));
    }
    elseif ($type === 'middle') {
      $partlen = floor(($maxlength - mb_strlen($omissionsign)) / 2);
      $value = mb_substr($value, 0, $partlen) . $omissionsign . mb_substr($value, -$partlen);
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
