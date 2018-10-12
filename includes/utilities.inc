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

function islandora_label_maker_get_queue() {
  return $queue = DrupalQueue::get('islandora_label_maker_queue');
}

function islandora_label_maker_add_to_label_queue_if_needed($objectid, $dsid = null, $time = null) {
  if (variable_get('islandora_label_maker_change_label_on_modify', false)) {
    $allowed_dsids = variable_get('islandora_label_maker_datastreams_change');
    if ($dsid === null || in_array($dsid, $allowed_dsids)) {
      islandora_label_maker_add_to_label_queue($objectid, $time);
    }
  }
}

function islandora_label_maker_add_to_label_queue($objectid, $time = null, $includerelated = true) {
  $queue = islandora_label_maker_get_queue();
  if ($time === null) {
    $time = time();
  }
  $timestamp = date("Y-m-d\TH:i:s.000", $time);
  $queue->createItem( array('object_id' => $objectid, 'timestamp' => $timestamp, 'includerelated' => $includerelated) );
}

function islandora_label_maker_add_related_to_label_queue_if_needed($parentid, $time = null) {
  $parentsolrfields = variable_get("islandora_label_maker_parent_solr_fields", array());
  $solrquery = "";
  foreach ($parentsolrfields as $parentsolrfield) {
    if ($solrquery !== "") {
      $solrquery .= ' OR ';
    }
    $solrquery .= "$parentsolrfield:\"$parentid\" OR $parentsolrfield:\"info:fedora/$parentid\"";
  }
  $qp = new IslandoraSolrQueryProcessor();
  $qp->buildQuery($solrquery);
  $qp->solrParams['fq'] = array(); // make sure we get all the results
  $qp->executeQuery(false);
  $r = $qp->islandoraSolrResult;
  foreach ($r['response']['objects'] as $solrobj) {
    $objectid = $solrobj['PID'];
    islandora_label_maker_add_to_label_queue($objectid, 0, false);
  }
}

function islandora_label_maker_update_label_for_object_id($id) {
  // generate the label, only change label if different than current label
  $newlabel = islandora_label_maker_label_for_object_id($id);
  if ($newlabel === false) {
     return false;
  }
  $obj = islandora_object_load($id);
  $result = array('oldlabel' => $obj->label, 'newlabel' => $newlabel);
  if ($obj->label !== $newlabel) {
    $obj->label = $newlabel;
  }
  return $result;
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
         $result = islandora_label_maker_label_for_solr_object($solrobj, $labeltemplate);
         if ($result !== false) {
           return $result;
         }
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
    $labeltemplate
  );
  foreach (array_reverse($truncates) as $rep) {
    $order = $rep['order'];
    $type = $rep['type'];
    $min = $rep['min'];
    $max = $rep['max'];
    $label = preg_replace_callback(
      "/>>$order>>(.*?)<<$order<</",
      function($matches) use (&$truncates, $type, $min, $max) {
        $value = $matches[1];
        if (preg_match_all('/>>(\d+)>>(.*?)<<\1<</', $value, $innermatches) > 0) {
          // calculate the length of this string
          $valuelength = mb_strlen($value);
          foreach ($innermatches[1] as $im) {
            $valuelength -= mb_strlen($im) * 2 + 8;
          }
          // if the length is larger than the maximum allowed
          if ($valuelength > $max) {
            // ... truncate based on type
            if ($type === 'inner') {
              // if the type is inner, recalculate the length of the inner matches
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
              while ($valuelength > $max) {
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
              // truncate all the innermatches based on the calculated lengths
              foreach ($innermatches[1] as $index => $im) {
                $value = preg_replace("/>>$im>>.*?<<$im<</", islandora_label_maker_truncate($innermatches[2][$index], $truncates[$im]['type'], $truncates[$im]['len']), $value);
              }
            }
            else {
              // if the type is something other than inner, also truncate the innermatches based on type and recalculate the total remaining length
              $len = $valuelength;
              foreach ($innermatches[1] as $index => $im) {
                $value = preg_replace_callback(
                  "/>>$im>>.*?<<$im<</",
                  function($dontcare) use ($innermatches, $index, $im, $truncates, &$len) {
                    $oldvalue = $innermatches[2][$index];
                    $newvalue = islandora_label_maker_truncate($oldvalue, $truncates[$im]['type'], $truncates[$im]['max']);
                    $len -= (mb_strlen($oldvalue) - mb_strlen($newvalue));
                    return $newvalue;
                  },
                  $value
                );
              }
              // if the total remaining length is more than max, also truncate the whole.
              if ($len > $max) {
                $value = islandora_label_maker_truncate($value, $type, $max);
              }
            }
          }
        }
        return $value;
      },
      $label
    );
  }
  $label = trim($label);
  if (strlen($label) === 0) {
    return false;
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
  $result = preg_replace_callback(
  '/^\s*(?:([a-zA-Z0-9_-]+)|(truncate|parent|alternative|glue)\((.+)\)|"""(.*?)""")\s*$/',
  function($matches) use ($solrdoc, &$truncates, $part) {
    $result = '';
    if (isset($matches[1]) && strlen($matches[1])) {
      $solrfield = $matches[1];
      $result = islandora_label_maker_value_from_solrdoc($solrfield, $solrdoc);
    }
    elseif (isset($matches[2]) && strlen($matches[2])) {
      $func = $matches[2];
      $argstr = $matches[3];
      $args = islandora_label_maker_args_split($argstr);
      if ($func === 'truncate') {
        $max = (isset($args[2])?intval($args[2]):255);
        $min = (isset($args[3])?intval($args[3]):0);
        if (count($args) > 4) {
          throw new Exception(t("truncate() has too many arguments, expecting 3 but found @argscount: %args", array('@argscount' => count($args), '%args' => implode(',', $args))));
        }
        if (count($args) < 3) {
          throw new Exception(t("truncate() has too little arguments, expecting at least 3"));
        }
        if (!in_array($args[1], array('end','begin','middle','inner'))) {
          throw new Exception(t("truncate() second argument should be begin, end, middle or inner, but is: %arg", array('%arg' => $args[1])));
        }
        if (count($args) > 2 && ($max < 0)) {
          throw new Exception(t("truncate() third argument should be a positive number, but found %arg", array('%arg' => $args[2])));
        }
        if (count($args) > 3 && ($min >= $max)) {
          throw new Exception(t("truncate() fourth argument should be a positive number smaller than the third argument, but found %arg", array('%arg' => $args[3])));
        }
        $value = islandora_label_maker_replace_part($args[0], $solrdoc, $truncates);
        $order = count($truncates) + 1;
        $key = ">>$order>>$value<<$order<<";
        $truncates[$order] = array('type' => $args[1], 'order' => $order, 'max' => $max, 'min' => $min);
        $result = $key;
      }
      elseif ($func === 'parent') {
        if (count($args) != 2) {
          throw new Exception(t("parent() should have two arguments (the solr field containing the parent identifier and the value to use from the parent) but found @argscount: %args", array('@argscount' => count($args), '%args' => implode(',', $args))));
        }
        $parentid = islandora_label_maker_replace_part($args[0], $solrdoc, $truncates);
        $parentid = str_replace('info:fedora/', '', $parentid);
        $query = "PID:\"$parentid\"";
        $psolrdoc = islandora_label_maker_object_for_solr_query($query);
        $parentfield = $args[1];
        $result = islandora_label_maker_replace_part($parentfield, $psolrdoc, $truncates);
      }
      elseif ($func === 'alternative') {
        if (count($args) < 2) {
          throw new Exception(t("alternative() should have at least two arguments"));
        }
        foreach ($args as $arg) {
          $value = islandora_label_maker_replace_part($arg, $solrdoc, $truncates);
          if (isset($value) && strlen($value)) {
            $result = $value;
            break;
          }
        }
      }
      elseif ($func === 'glue') {
        if (count($args) < 2) {
          throw new Exception(t("glue() should have at least two arguments"));
        }
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
    else {
      throw new Exception(t("malformed label template"));
    }
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
  $qp->solrParams['fq'] = array(); // make sure we get all the results
  $qp->solrLimit = 1;
  $qp->solrStart = 0;
  $qp->executeQuery(false);
  $r = $qp->islandoraSolrResult;
  if (isset($r['response']['numFound']) && $r['response']['numFound'] > 0) {
    return $r['response']['objects'][0];
  }
  return null;
}
