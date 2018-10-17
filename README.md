# Islandora Label Maker
===========================

## Introduction
Islandora label maker generates new labels for objects based on Solr fields.
This can be useful when you want to have the label of the objects based on the
metadata from a datastream.
This module allows you to define the label based on Solr fields and queries.

## Configuration
The configuration page is located at `admin/islandora/tools/islandora_label_maker`.
Here you can define when the label should be generated and what it will look like.
You can define multiple label configurations, for example for every content model,
based on a Solr query.
If the label is too long, you can truncate it however you want.
At the bottom of the configuration page, more help is available.

### Make a new label when datastreams change
On the configuration page there is a checkbox to automatically generate a new label
when any of the configured datastreams change. When this checkbox is unchecked, you
can generate new labels manually (See "Generate labels per object" below) or by
using drush.
When the checkbox is enabled, the objects that need a new label will be queued.
This is because the label can only be generated when Solr is up-to-date. So it may
take sometime before the label is generated. This depends on the configuration of
the drupal cron job, see Status report when cron ran last.

### Pending labels
The pending labels tab shows the number of pending labels, labels left in the queue.
It is possible to make the labels for the pending items directly, so there is no need
to wait for the next cron run. Note: if Solr is not up-to-date, the label cannot be
generated.

## Generate labels per object
At the properties page of an object, there is a new button to 'Generate new labels...'.
When clicked, a list of one or more new labels is presented. Mark the labels that you
want to change, optionally make some adjustments and press the button to change the
labels to their new values.

## drush

You can list, update or add to the queue based on a list of item ids.

### `islandora_label_maker_list`

List the new labels based on a list of item ids.

This command needs an `ids_file`, and optionally a `format`, an `include_related` option to also include related items.

### `islandora_label_maker_queue`

Add items to the queue based on a list of item ids, so new labels will be generated later.

This command needs an `ids_file`, and optionally an `include_related` option to also include related items.

### `islandora_label_maker_update`

Update labels based on a list of item ids.

This command needs an `ids_file`, and optionally an `include_related` option to also include related items.

## License

[GPLv3](LICENSE.txt)
Copyright 2018 Leiden University Library
