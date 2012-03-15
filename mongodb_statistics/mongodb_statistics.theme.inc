<?php


/**
 * @file
 * User & Node page callbacks for the statistics module.
 */

/**
 * Implements hook_theme().
 * 
 * List of all themable items provided by this module
 */
function mongodb_statistics_theme() {
  return array(
    'mongodb_statistics_node_counters' => array(
      'variables' => array('record' => NULL),
    ),
    'mongodb_statistics_node_access_log' => array(
      'variables' => array('rows' => NULL, 'logs' => NULL),
    ),
  );
}
/**
 * Render a node counters details
 */
function theme_mongodb_statistics_node_counters($variables) {
  $record = $variables['record'];
  $items = array(
    t('Day views count: @count', array('@count' => $record['daycount'])),
    t('Total views count: @count', array('@count' => $record['totalcount'])),
    t('Day generation: @count', array('@count' => $record['daygen'])),
    t('Total generation: @count', array('@count' => $record['totalgen'])),
    t('Last access: @date', array('@date' => format_date($record['timestamp'], 'short'))),
  );
  $build['mongodb_counters_list'] = array(
    '#type' => 'markup',
    '#markup' => theme_item_list(array(
      'items' => $items,
      'title' => t('Node Counters'),
      'type' => 'ul',
      'attributes' => array()
    ))
  );
  return drupal_render($build);
}
/**
 * Render a node statistics details
 */
function theme_mongodb_statistics_access_log($variables) {
  
  $rows = $variables['rows'];
  
  $build = array();
  $build['mongodb_statistics_table_title'] = array(
    '#type' => 'markup',
    '#markup' => '<h3>'.t('Access Log Statistics').'</h3>',
  );
  // TODO provides a lot more useful stats...
  $header = array(
    array('data' => t('Time')),
    array('data' => t('Referrer')),
    array('data' => t('User')),
    array('data' => t('Operations')));
  
  $build['mongodb_statistics_table'] = array(
    '#theme' => 'table',
    '#header' => $header,
    '#rows' => $rows,
    '#empty' => t('No statistics available.'),
  );
    
  return drupal_render($build);
}
