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
 * Exceptions that may occur when making a label.
 */
class IslandoraLabelMakerMalformedTemplateException extends Exception {
}

class IslandoraLabelMakerGuardException extends Exception {
}


/**
 * Get the queue we use.
 */
function islandora_label_maker_get_queue() {
  return $queue = DrupalQueue::get('islandora_label_maker_queue');
}

/**
 * Add item to queue so the label will be generated later if needed.
 *
 * @param string $objectid
 *   The ID of the object to generate a label for.
 * @param string $dsid
 *   The ID of the datastream of the object that was changed. Only add to queue
 *   if the configuration matches this id.
 * @param int $time
 *   The current time in seconds since the epoch. Label will only be created if
 *   the Solr object has a time later than this time.
 */
function islandora_label_maker_add_to_label_queue_if_needed($objectid, $dsid = NULL, $time = NULL) {
  if (variable_get('islandora_label_maker_change_label_on_modify', FALSE)) {
    $allowed_dsids = variable_get('islandora_label_maker_datastreams_change');
    if ($dsid === NULL || in_array($dsid, $allowed_dsids)) {
      islandora_label_maker_add_to_label_queue($objectid, $time);
    }
  }
}

/**
 * Add item to queue so the label will be generated later.
 *
 * @param string $objectid
 *   The ID of the object to generate a label for.
 * @param int $time
 *   The current time in seconds since the epoch. Label will only be created if
 *   the Solr object has a time later than this time.
 * @param bool $includerelated
 *   If TRUE related items will be added to the queue when item is processed.
 */
function islandora_label_maker_add_to_label_queue($objectid, $time = NULL, $includerelated = TRUE) {
  $queue = islandora_label_maker_get_queue();
  if ($time === NULL) {
    $time = time();
  }
  $timestamp = date("Y-m-d\TH:i:s.000", $time);
  $queue->createItem(array(
    'object_id' => $objectid,
    'timestamp' => $timestamp,
    'includerelated' => $includerelated,
  ));
}

/**
 * Find the IDs of the related objects.
 *
 * @param string $parentid
 *   The ID of the parent object.
 *
 * @return array
 *   An array with Solr docs containing only the PID of the related objects.
 */
function islandora_label_maker_related_solrobj_for_object($parentid) {
  $parentsolrfields = variable_get("islandora_label_maker_parent_solr_fields", array());
  $solrquery = "";
  foreach ($parentsolrfields as $parentsolrfield) {
    if (strlen($parentsolrfield) > 0) {
      if ($solrquery !== "") {
        $solrquery .= ' OR ';
      }
      $solrquery .= "$parentsolrfield:\"$parentid\" OR $parentsolrfield:\"info:fedora/$parentid\"";
    }
  }
  if ($solrquery === "") {
    return array();
  }
  $qp = new IslandoraSolrQueryProcessor();
  $qp->buildQuery($solrquery);
  $qp->solrParams['fq'] = array();
  $qp->solrStart = 0;
  $qp->solrLimit = 10000;
  $qp->solrParams['fl'] = 'PID';
  $qp->executeQuery(FALSE);
  $r = $qp->islandoraSolrResult;

  return $r['response']['objects'];
}

/**
 * Add all related items to queue so their labels will be generated later.
 *
 * @param string $parentid
 *   The ID of the parent object.
 * @param int $time
 *   The current time in seconds since the epoch. Label will only be created if
 *   the Solr object has a time later than this time.
 */
function islandora_label_maker_add_related_to_label_queue_if_needed($parentid, $time = NULL) {
  $solrobjs = islandora_label_maker_related_solrobj_for_object($parentid);
  foreach ($solrobjs as $solrobj) {
    $objectid = $solrobj['PID'];
    islandora_label_maker_add_to_label_queue($objectid, $time, FALSE);
  }
}

/**
 * Update the label of the object, only if new label exists and is different.
 *
 * @param string $id
 *   The ID of the object to make a label for.
 *
 * @return array
 *   Returns FALSE if the new label cannot be generated, or an array containing
 *   oldlabel and newlabel.
 */
function islandora_label_maker_update_label_for_object_id($id) {
  $newlabel = islandora_label_maker_label_for_object_id($id);
  if ($newlabel === FALSE) {
    return FALSE;
  }
  $obj = islandora_object_load($id);
  $result = array('oldlabel' => $obj->label, 'newlabel' => $newlabel);
  if ($obj->label !== $newlabel) {
    $obj->label = $newlabel;
  }
  return $result;
}

/**
 * Returns the label of the object, only if new label exists.
 *
 * @param string $id
 *   The ID of the object to make a label for.
 *
 * @return string
 *   Returns FALSE if the new label cannot be generated, or a string containing
 *   the new label
 */
function islandora_label_maker_label_for_object_id($id) {
  for ($i = 1; $i <= 99; $i++) {
    $active = variable_get('islandora_label_maker_label_config_' . $i . '_active');
    $solrquery = str_replace(array("\r\n", "\r", "\n"), ' ', variable_get('islandora_label_maker_label_config_' . $i . '_solr_query'));
    $labeltemplate = str_replace(array("\r\n", "\r", "\n"), ' ', variable_get('islandora_label_maker_label_config_' . $i . '_label_template'));
    if (!$active) {
      continue;
    }
    if (isset($solrquery) && strlen($solrquery) > 0) {
      $query = "PID:\"$id\" AND $solrquery";
      $solrobj = islandora_label_maker_object_for_solr_query($query);
      if ($solrobj !== NULL) {
        return islandora_label_maker_label_for_solr_object($solrobj, $labeltemplate);
      }
    }
    else {
      break;
    }
  }
  return FALSE;
}

/**
 * Returns the label based on the Solr doc and label template, if it exists.
 *
 * @param array $solrdoc
 *   An array as returned by Solr for 1 document.
 * @param string $labeltemplate
 *   The template for the label.
 *
 * @return string
 *   Returns FALSE if the new label cannot be generated, or a string containing
 *   the new label
 */
function islandora_label_maker_label_for_solr_object($solrdoc, $labeltemplate) {
  $truncates = array();
  try {
    $label = preg_replace_callback(
      '/{{\s*(.+?)\s*}}/',
      function($matches) use ($solrdoc, &$truncates) {
        $part = $matches[1];
        return islandora_label_maker_replace_part($part, $solrdoc, $truncates);
      },
      $labeltemplate
    );
    $label = str_replace(array("\r\n", "\r", "\n"), ' ', $label);
  }
  catch (IslandoraLabelMakerGuardException $e) {
    $label = FALSE;
    $truncates = array();
  }
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
          // Calculate the length of this string.
          $valuelength = mb_strlen($value);
          foreach ($innermatches[1] as $im) {
            $valuelength -= mb_strlen($im) * 2 + 8;
          }
          // If the length is larger than the maximum allowed...
          if ($valuelength > $max) {
            // ... truncate based on type.
            if ($type === 'inner') {
              // If the type is inner, recalculate the length of innermatches.
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
                $madeshorter = FALSE;
                foreach ($innermatches[1] as $index => $im) {
                  $trunc = $truncates[$im];
                  if ($truncates[$im]['len'] > $trunc['min']) {
                    $valuelength--;
                    $truncates[$im]['len']--;
                    $madeshorter = TRUE;
                  }
                }
                if (!$madeshorter) {
                  break;
                }
              }
              // Truncate all the innermatches based on the calculated lengths.
              foreach ($innermatches[1] as $index => $im) {
                $value = preg_replace("/>>$im>>.*?<<$im<</", islandora_label_maker_truncate($innermatches[2][$index], $truncates[$im]['type'], $truncates[$im]['len']), $value);
              }
            }
            else {
              // If the type is something other than inner, also truncate the
              // innermatches based on type and recalculate the total remaining
              // length.
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
              // If the total remaining length is more than max, also truncate
              // the whole.
              if ($len > $max) {
                $value = islandora_label_maker_truncate($value, $type, $max);
              }
            }
          }
        }
        else {
          $value = islandora_label_maker_truncate($value, $type, $max);
        }
        return $value;
      },
      $label
    );
  }
  if ($label === FALSE) {
    return FALSE;
  }
  $label = trim($label);
  $label = preg_replace('/\s\s+/', ' ', $label);
  if (strlen($label) === 0) {
    return FALSE;
  }
  return $label;
}

/**
 * Returns the value from a Solr doc.
 *
 * @param string $solrfield
 *   The Solrfield to get the value from.
 * @param array $solrdoc
 *   An array as returned by Solr for 1 document.
 *
 * @return string
 *   Returns the value from a Solr doc, or NULL is the value cannot be found.
 */
function islandora_label_maker_value_from_solrdoc($solrfield, $solrdoc) {
  $value = NULL;
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

/**
 * Returns the value from a Solr doc.
 *
 * @param string $part
 *   A part of the label template to replace with found values.
 * @param array $solrdoc
 *   An array as returned by Solr for 1 document.
 * @param array &$truncates
 *   An array containing the truncated values, used for truncation.
 *
 * @throws IslandoraLabelMakerMalformedTemplateException
 *   Throws this exception if the template is malformed.
 * @throws IslandoraLabelMakerGuardException
 *   Throws this exception if a guarded part of the template cannot be obtained.
 *
 * @return string
 *   Returns a value based on the template found in part and the values of
 *   solrdoc.
 */
function islandora_label_maker_replace_part($part, $solrdoc, &$truncates) {
  $result = preg_replace_callback(
  '/^\s*(?:([a-zA-Z0-9_.]+)|(truncate|parent|alternative|glue|guard|order)\((.+)\)|"""(.*?)""")\s*$/',
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
        $max = (isset($args[2]) ? intval($args[2]) : 255);
        $min = (isset($args[3]) ? intval($args[3]) : 0);
        if (count($args) > 4) {
          throw new IslandoraLabelMakerMalformedTemplateException(t(
            "truncate() has too many arguments, expecting 3 but found @argscount: %args",
            array('@argscount' => count($args), '%args' => implode(',', $args))
          ));
        }
        if (count($args) < 3) {
          throw new IslandoraLabelMakerMalformedTemplateException(t("truncate() has too little arguments, expecting at least 3"));
        }
        $allowed = array(
          'wordend',
          'wordbegin',
          'wordmiddle',
          'end',
          'begin',
          'middle',
          'inner',
        );
        if (!in_array($args[1], $allowed)) {
          throw new IslandoraLabelMakerMalformedTemplateException(t(
            "truncate() second argument should be %allowed, but is: %arg",
            array('%allowed' => implode(', ', $allowed), '%arg' => $args[1])
          ));
        }
        if (count($args) > 2 && ($max < 0)) {
          throw new IslandoraLabelMakerMalformedTemplateException(t(
            "truncate() third argument should be a positive number, but found %arg",
            array('%arg' => $args[2])
          ));
        }
        if (count($args) > 3 && ($min >= $max)) {
          throw new IslandoraLabelMakerMalformedTemplateException(t(
            "truncate() fourth argument should be a positive number smaller than the third argument, but found %arg",
            array('%arg' => $args[3])
          ));
        }
        $value = islandora_label_maker_replace_part($args[0], $solrdoc, $truncates);
        $order = count($truncates) + 1;
        $key = ">>$order>>$value<<$order<<";
        $truncates[$order] = array(
          'type' => $args[1],
          'order' => $order,
          'max' => $max,
          'min' => $min,
        );
        $result = $key;
      }
      elseif ($func === 'parent') {
        if (count($args) != 2) {
          throw new IslandoraLabelMakerMalformedTemplateException(t(
            "parent() should have two arguments (the solr field containing the parent identifier and the value to use from the parent) but found @argscount: %args",
            array('@argscount' => count($args), '%args' => implode(',', $args))
          ));
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
          throw new IslandoraLabelMakerMalformedTemplateException(t("alternative() should have at least two arguments"));
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
          throw new IslandoraLabelMakerMalformedTemplateException(t("glue() should have at least two arguments"));
        }
        foreach ($args as $arg) {
          $value = islandora_label_maker_replace_part($arg, $solrdoc, $truncates);
          if (isset($value) && strlen($value)) {
            $result .= $value;
          }
        }
      }
      elseif ($func === 'guard') {
        if (count($args) == 0 || count($args) > 2) {
          throw new IslandoraLabelMakerMalformedTemplateException(t(
            "guard() should have one or two arguments (the solr field and optionally a Solr query) but found @argscount: %args",
            array('@argscount' => count($args), '%args' => implode(',', $args))
          ));
        }
        if (count($args) == 2 && strlen($args[1]) < 3) {
          throw new IslandoraLabelMakerMalformedTemplateException(t(
            "guard() second argument should be a valid Solr query, but found: %arg",
            array('%arg' => $args[1])
          ));
        }
        $value = islandora_label_maker_replace_part($args[0], $solrdoc, $truncates);
        if ($value === FALSE || strlen($value) === 0) {
          throw new IslandoraLabelMakerGuardException("No value found");
        }
        if (count($args) == 2) {
          $objectid = $solrdoc['PID'];
          $query = "PID:\"$objectid\" AND $args[1]";
          $solrobj = islandora_label_maker_object_for_solr_query($query);
          if ($solrobj === NULL) {
            throw new IslandoraLabelMakerGuardException("Solr query '$query' found no objects");
          }
        }
        $result = $value;
      }
      elseif ($func === 'order') {
        if (count($args) != 1 || !in_array($args[0], array('book', 'newspaper', 'compound'))) {
          throw new IslandoraLabelMakerMalformedTemplateException(t(
            "order() should have one argument (indicating the type of parent: book, newspaper, compound) but found @argscount: %args",
            array('@argscount' => count($args), '%args' => implode(',', $args))
          ));
        }
        $parenttype = $args[0];
        if ($parenttype === 'book') {
          if (module_load_include('inc', 'islandora_paged_content', 'includes/utilities')) {
            $objectid = islandora_label_maker_value_from_solrdoc('RELS_EXT_isPageOf_uri_ms', $solrdoc);
            $objectid = str_replace('info:fedora/', '', $objectid);
            $object = islandora_object_load($objectid);
            $pages = islandora_paged_content_get_pages($object);
            if (isset($pages[$solrdoc['PID']])) {
              $result = ltrim($pages[$solrdoc['PID']]['page'], '0');
            }
          }
        }
        elseif ($parenttype === 'newspaper') {
          if (module_load_include('inc', 'islandora_newspaper', 'includes/utilities')) {
            $objectid = $solrdoc['PID'];
            $object = islandora_object_load($objectid);
            $seq = islandora_newspaper_get_current_sequence($object);
            if ($seq !== FALSE) {
              $result = $seq;
            }
          }
        }
        elseif ($parenttype === 'compound') {
          if (drupal_load('module', 'islandora_compound_object')) {
            $objectid = islandora_label_maker_value_from_solrdoc('RELS_EXT_isConstituentOf_uri_ms', $solrdoc);
            $objectid = str_replace('info:fedora/', '', $objectid);
            $object = islandora_object_load($objectid);
            $children = islandora_compound_object_get_parts($object, TRUE);
            if (isset($children[$solrdoc['PID']])) {
              $result = $children[$solrdoc['PID']]['seq'];
            }
          }
        }
        else {
          throw new IslandoraLabelMakerMalformedTemplateException(t(
            "order() should have one argument (indicating the type of parent: book, newspaper, compound) but found @argscount: %args",
            array('@argscount' => count($args), '%args' => implode(',', $args))
          ));
        }
      }
    }
    elseif (isset($matches[4]) && strlen($matches[4])) {
      $result = $matches[4];
    }
    return $result;
  },
  $part, -1, $count
  );
  if ($count === 0) {
    throw new IslandoraLabelMakerMalformedTemplateException(t(
      "Unexpected part of template found: '%part'",
      array("%part" => $result)
    ));
  }
  return $result;
}

/**
 * Splits an argument string in one or more argument parts.
 *
 * @param string $argstr
 *   The argument string.
 *
 * @throws IslandoraLabelMakerMalformedTemplateException
 *   Throws this exception if there are too many opening or closing brackets.
 *
 * @return array
 *   Returns an array containing one or more argument parts.
 */
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
    throw new IslandoraLabelMakerMalformedTemplateException("Too many opening brackets in $argstr");
  }
  if ($brackets < 0) {
    throw new IslandoraLabelMakerMalformedTemplateException("Too many closing brackets in $argstr");
  }

  return $args;
}

/**
 * Truncates the value by type if it is longer than max length.
 *
 * @param string $value
 *   The value to truncate.
 * @param string $type
 *   The type of truncation to use, can be: wordend, wordbegin, wordmiddle,
 *   end, begin or middle.
 * @param int $maxlength
 *   If value is longer than this, the value will be truncated to a string with
 *   at most this length.
 * @param string $omissionsign
 *   Use this string in the place where the value is truncated.
 *
 * @return string
 *   The truncated value.
 */
function islandora_label_maker_truncate($value, $type, $maxlength, $omissionsign = '...') {
  if (mb_strlen($value) > $maxlength) {
    if ($maxlength < 3) {
      // Leave out the omissionsign if the truncated word is very short.
      $omissionsign = '';
    }
    $maxlength -= mb_strlen($omissionsign);
    if ($type === 'wordend') {
      // We should use truncate_utf8() instead of rolling our own, but cannot
      // set omissionsign...
      $matches = array();
      // Make sure that there is not to much truncated.
      $minlength = max(0, ($maxlength - max(10, ($maxlength / 10))));

      $found = preg_match('/^(.{' . $minlength . ',' . intval($maxlength) . '})[' . PREG_CLASS_UNICODE_WORD_BOUNDARY . ']/u', $value, $matches);
      if ($found) {
        $value = $matches[1] . $omissionsign;
      }
      else {
        $value = mb_substr($value, 0, $maxlength) . $omissionsign;
      }
    }
    elseif ($type === 'wordbegin') {
      $matches = array();
      // Make sure that there is not to much truncated.
      $minlength = max(0, ($maxlength - max(10, ($maxlength / 10))));

      $found = preg_match('/[' . PREG_CLASS_UNICODE_WORD_BOUNDARY . '](.{' . $minlength . ',' . intval($maxlength) . '})$/u', $value, $matches);
      if ($found) {
        $value = $omissionsign . $matches[1];
      }
      else {
        $value = $omissionsign . mb_substr($value, 0, $maxlength);
      }
    }
    elseif ($type === 'wordmiddle') {
      $matches = array();
      // Calculate the part max length.
      $partmaxlen = floor($maxlength / 2) + ($maxlength % 2);
      // Make sure that there is not to much truncated.
      $partminlen = max(0, ($partmaxlen - max(6, floor($partmaxlen / 10))));
      $range = '{' . $partminlen . ',' . $partmaxlen . '}';

      $found = preg_match('/^(.' . $range . ')[' . PREG_CLASS_UNICODE_WORD_BOUNDARY . '].*[' . PREG_CLASS_UNICODE_WORD_BOUNDARY . '](.' . $range . ')$/u', $value, $matches);
      if ($found) {
        $value = $matches[1] . $omissionsign . $matches[2];
      }
      else {
        $value = mb_substr($value, 0, $partmaxlen) . $omissionsign . mb_substr($value, -$partmaxlen);
      }
    }
    elseif ($type === 'end') {
      $value = mb_substr($value, 0, $maxlength) . $omissionsign;
    }
    elseif ($type === 'begin') {
      $value = $omissionsign . mb_substr($value, mb_strlen($value) - $maxlength);
    }
    elseif ($type === 'middle') {
      $partlen = floor($maxlength / 2);
      $value = mb_substr($value, 0, $partlen) . $omissionsign . mb_substr($value, -$partlen);
    }
  }
  return $value;
}

/**
 * Returns the first Solr document that meets the query.
 *
 * @param string $query
 *   A valid Solr query.
 *
 * @return array
 *   Returns the found Solr doc, or NULL if not found.
 */
function islandora_label_maker_object_for_solr_query($query) {
  $qp = new IslandoraSolrQueryProcessor();
  $qp->buildQuery($query);
  $qp->solrParams['fq'] = array();
  $qp->solrLimit = 1;
  $qp->solrStart = 0;
  $qp->executeQuery(FALSE);
  $r = $qp->islandoraSolrResult;
  if (isset($r['response']['numFound']) && $r['response']['numFound'] > 0) {
    return $r['response']['objects'][0];
  }
  return NULL;
}
