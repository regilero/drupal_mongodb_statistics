<?php


/**
 * @file
 * User & Node page callbacks for the statistics module.
 */

/**
 * Tab on node page: infos about node counters and track of this node
 * in access_log mongodb collection.
 *
 * @return build array
 */
function mongodb_statistics_node_tracker() {
  global $pager_page_array, $pager_total, $pager_total_items, $pager_limits;
  $per_page = 30;
  $page = isset($_GET['page']) ? $_GET['page'] : '';
  $pager_page_array = explode(',', $page);
  $on_page = (int) $pager_page_array[0];
  
  if ($node = node_load(arg(1))) {
    
    // Node counters stats
    if ( user_access('view post access counter')
     && variable_get('mongodb_statistics_count_content_views', 0)
     && ($record = mongodb_statistics_get($node->nid))) {
      $items = array(
        t('Day count: @count', array('@count' => $record['daycount'])),
        t('Total count: @count', array('@count' => $record['totalcount'])),
        t('Last access: @date', array('@date' => format_date($record['timestamp'], 'short'))),
      );
      $build['mongodb_counters_table'] = array(
        '#type' => 'markup',
        '#markup' => theme_item_list(array(
          'items' => $items,
          'title' => t('Node Counters'),
          'type' => 'ul',
          'attributes' => array()
        ))
      );
    } else {
      $build['mongodb_counters_table_title'] = array(
        '#type' => 'markup',
        '#markup' => '<h3>'.t('Node Counters').'</h3>',
      );
      $build['mongodb_counters_table'] = array(
        '#type' => 'markup',
        '#markup' => '<div><p>'.t('No statistics available.').'</p></div>',
      );
    }
    
    // Access log stats
    $build['mongodb_statistics_table_title'] = array(
      '#type' => 'markup',
      '#markup' => '<h3>'.t('Access Log Statistics').'</h3>',
    );
    if (!variable_get('mongodb_statistics_enable_access_log', 0)) {
      $build['mongodb_statistics_table'] = array(
        '#type' => 'markup',
        '#markup' => '<div><p>'.t('No statistics available.').'</p></div>',
      );
    } else {
      // TODO provides a lot more useful stats...
      $header = array(
        array('data' => t('Time')),
        array('data' => t('Referrer')),
        array('data' => t('User')),
        array('data' => t('Operations')));
      
      $collectionname = variable_get('mongodb_accesslog_collection_name', 'accesslog');
      $collection = mongodb_collection($collectionname);
      $regex= new MongoRegex('/^node\/' . $node->nid.'(\/.*)?/');
      $query = array('path' => array( '$regex' => $regex));
      $fields = array(
          '_id',
          'timestamp',
          'url',
          'uid',
          'name'
        );
      $cursor = $collection->find($query,$fields);
      $countcursor = $collection->find($query,$fields);
      $cursor->sort(array('timestamp' => -1))
        ->limit($per_page)
        ->skip($on_page * $per_page);
      $rows = array();
      foreach ($cursor as $log) {
        $rows[] = array(
          array('data' => format_date($log['timestamp'], 'short'), 'class' => array('nowrap')),
          _mongodb_statistics_link($log['url']),
          theme('username', array('account' => (object) $log)),
          l(t('details'), 'admin/reports/stats-mongodb/hits/access/' . $log['_id']),
        );
      }
      $build['mongodb_statistics_table'] = array(
        '#theme' => 'table',
        '#header' => $header,
        '#rows' => $rows,
        '#empty' => t('No statistics available.'),
      );
      // add the pager
      if ($on_page > 0 || count($rows) >= $per_page) {
        $pager_total_items[0] = $countcursor->count();
        $pager_total[0] = ceil($pager_total_items[0] / $per_page);
        $pager_page_array[0] = max(0, min((int) $pager_page_array[0], ((int)$pager_total[0]) - 1));
        $pager_limits[0] = $per_page;
        $build['mongodb_statistics_table_pager'] = array(
          '#theme' => 'pager',
        );
      }
    }
    drupal_set_title($node->title);
    
    return $build;
  }
  else {
    drupal_not_found();
  }
}

/**
 * Tab on user page: search all activity of this user in the access_log mongodb collection.
 *
 * @return build array
 */
function mongodb_statistics_user_tracker() {
  global $pager_page_array, $pager_total, $pager_total_items, $pager_limits;
  $per_page = 30;
  $page = isset($_GET['page']) ? $_GET['page'] : '';
  $pager_page_array = explode(',', $page);
  $on_page = (int) $pager_page_array[0];
  
  if ($account = user_load(arg(1))) {
    $header = array(
        array('data' => t('Timestamp')),
        array('data' => t('Page')),
        array('data' => t('Operations')));
    
    
    $collectionname = variable_get('mongodb_accesslog_collection_name', 'accesslog');
    $collection = mongodb_collection($collectionname);
    $fields = array(
        '_id',
        'timestamp',
        'path',
        'title',
    );
    $filter = array('uid' => (int) $account->uid);
    $cursor = $collection->find($filter,$fields);
    $countcursor = $collection->find($filter,$fields);
    $cursor->sort(array('timestamp' => -1))
      ->limit($per_page)
      ->skip($on_page * $per_page);
    
    $rows = array();
    foreach ($cursor as $log) {
      $rows[] = array(
        array('data' => format_date($log['timestamp'], 'short'), 'class' => array('nowrap')),
        _mongodb_statistics_format_item($log['title'], $log['path']),
        l(t('details'), 'admin/reports/stats-mongodb/hits/access/' . $log['_id']));
    }

    drupal_set_title(format_username($account));
   
    $build['mongodb_statistics_table'] = array(
      '#theme' => 'table',
      '#header' => $header,
      '#rows' => $rows,
      '#empty' => t('No statistics available.'),
    );
    // build pager
    if ($on_page > 0 || count($rows) >= $per_page) {
      $pager_total_items[0] = $countcursor->count();
      $pager_total[0] = ceil($pager_total_items[0] / $per_page);
      $pager_page_array[0] = max(0, min((int) $pager_page_array[0], ((int)$pager_total[0]) - 1));
      $pager_limits[0] = $per_page;
      $build['mongodb_statistics_table_pager'] = array(
        '#theme' => 'pager',
      );
    }
    return $build;
  }
  else {
    drupal_not_found();
  }
}
