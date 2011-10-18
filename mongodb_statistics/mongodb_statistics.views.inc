<?php
/**
 * @file
 * Provide views data and handlers for mongodb_statistics.module
 * It's basically a copy of statistics.view.inc from views module
 * As we use the same tables as statistics module but fill them only
 * in asynchronous cron jobs if configured to do so.
 *
 * WARNING: this module does not provide a views access on access_log,
 * only node_counter table is synchonised with an SQL table.
 * If you want to query the access_log mongodb collection then write
 * mongodb modules and do not use views.
 */

/**
 * @defgroup views_mongodb_statistics_module mongodb_statistics.module handlers
 *
 * Includes the ability to create views of just the statistics table.
 * @{
 */

/**
 * Implements hook_views_data()
 */
function mongodb_statistics_views_data() {
  // Basic table information.
  if (variable_get('mongodb_statistics_count_content_views', 0)
    && variable_get('mongodb_statistics_sync_db_tables_count_content_views', 0) ) {
    // ----------------------------------------------------------------
    // mongodb_node_counter table

    $data['mongodb_node_counter']['table']['group']  = t('Content statistics');

    $data['mongodb_node_counter']['table']['join'] = array(
      // ...to the node table
      'node' => array(
        'left_field' => 'nid',
        'field' => 'nid',
      ),
    );

    // totalcount
    $data['mongodb_node_counter']['totalcount'] = array(
      'title' => t('Total views'),
      'help' => t('The total number of times the node has been viewed.'),

      'field' => array(
        'handler' => 'views_handler_field_numeric',
        'click sortable' => TRUE,
       ),
      'filter' => array(
        'handler' => 'views_handler_filter_numeric',
      ),
      'sort' => array(
        'handler' => 'views_handler_sort',
      ),
    );

    // daycount
    $data['mongodb_node_counter']['daycount'] = array(
      'title' => t('Views today'),
      'help' => t('The total number of times the node has been viewed today.'),

      'field' => array(
        'handler' => 'views_handler_field_numeric',
        'click sortable' => TRUE,
       ),
      'filter' => array(
        'handler' => 'views_handler_filter_numeric',
      ),
      'sort' => array(
        'handler' => 'views_handler_sort',
      ),
    );

    // timestamp
    $data['mongodb_node_counter']['timestamp'] = array(
      'title' => t('Most recent view'),
      'help' => t('The most recent time the node has been viewed.'),

      'field' => array(
        'handler' => 'views_handler_field_date',
        'click sortable' => TRUE,
      ),
      'filter' => array(
        'handler' => 'views_handler_filter_date',
      ),
      'sort' => array(
        'handler' => 'views_handler_sort',
      ),
    );
  }

  return $data;
}

/**
 * @}
 */
