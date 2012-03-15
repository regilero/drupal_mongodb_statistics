<?php

/**
 * @file
 * Admin page callbacks for the statistics module.
 */

function mongodb_statistics_all_tops() {
  $items1 = array(
    l(t('Recent hits'),'admin/reports/stats-mongodb/hits'),
    l(t('Top pages'),'admin/reports/stats-mongodb/pages'),
    l(t('Top visitors'),'admin/reports/stats-mongodb/visitors'),
    l(t('Top referrers'),'admin/reports/stats-mongodb/referrers'),
  );
  $items2 = array(
    l(t('Popular Content'),'admin/reports/stats-mongodb/popular-nodes'),
  );
  $build = array(
    'accesslist' => array(
      '#type' => 'markup',
      '#markup' => theme_item_list(array(
        'items' => $items1,
        'title' => t('Access log statistics'),
        'type' => 'ul',
        'attributes' => array()
      ))
    ),
    'nodetext' => array(
      '#type' => 'markup',
      '#markup' => theme_item_list(array(
        'items' => $items2,
        'title' => t('Node Counters statistics'),
        'type' => 'ul',
        'attributes' => array()
      ))
    ),
  );
  return $build;
}

/**
 * Menu callback; presents the "recent hits" page.
 */
function mongodb_statistics_top_nodes() {
  // TODO: provide a table, not just the final result?
  $content = mongodb_statistics_get_direct_node_counter_statistics();
  if (!variable_get('mongodb_statistics_count_content_views', 0)) {
    if (is_null($content)) $content = array();
    $content['mongodb_statistics_notice'] = array(
      '#type' => 'markup',
      '#weight' => '-5',
      '#markup' => '<div><p><strong>'
            . t('Count content views statistics are suspended, this content is not updated. '
            . 'You can activate "Count content views" statistics in the '
            . '<a href="@confurl">mongodb statistics configuration</a> zone.',
                array('@confurl' => url('admin/config/system/mongodb_statistics')))
            . '</stron></p></div>',
    );
  }
  return $content;
}

/**
 * Menu callback; presents the "recent hits" page.
 */
function mongodb_statistics_recent_hits() {
  global $pager_page_array, $pager_total, $pager_total_items, $pager_limits;
  $per_page = 30;
  $page = isset($_GET['page']) ? $_GET['page'] : '';
  $pager_page_array = explode(',', $page);
  $on_page = (int) $pager_page_array[0];
  
  $header = array(
    array('data' => t('Timestamp')),
    array('data' => t('Page')),
    array('data' => t('User')),
    array('data' => t('Operations'))
  );
  $collectionname = variable_get('mongodb_accesslog_collection_name', 'accesslog');
  $cursor = mongodb_collection($collectionname)
    ->find(
      array(),
      array('aid', 'timestamp', 'path', 'title', 'uid','name')
    )
    ->limit($per_page)
    ->skip($on_page * $per_page)
    ->sort(array('timestamp' => -1));

  $rows = array();
  foreach ($cursor as $log) {
    $rows[] = array(
      array('data' => format_date($log['timestamp'], 'short'), 'class' => array('nowrap')),
      _mongodb_statistics_format_item($log['title'], $log['path']),
      theme('username', array('account' => (object) $log)),
      l(t('details'), 'admin/reports/stats_mongodb/hits/access/'. $log['_id']));
  }
  if (!variable_get('mongodb_statistics_enable_access_log', 0)) {
    $build['mongodb_statistics_notice'] = array(
      '#type' => 'markup',
      '#markup' => '<div><p><strong>'
            . t('Acces log statistics are suspended, this content is not updated. '
            . 'You can activate access log statistics in the '
            . '<a href="@confurl">mongodb statistics configuration</a> zone.',
                array('@confurl' => url('admin/config/system/mongodb_statistics')))
            . '</stron></p></div>',
    );
  }
  $build['mongodb_statistics_table'] = array(
    '#theme' => 'table',
    '#header' => $header,
    '#rows' => $rows,
    '#empty' => t('No statistics available.'),
  );

  // Add the pager.
  if ($on_page > 0 || count($rows) >= $per_page) {
    $pager_total_items[0] = mongodb_collection($collectionname)
      ->find()
      ->count();
    $pager_total[0] = ceil($pager_total_items[0] / $per_page);
    $pager_page_array[0] = max(0, min((int) $pager_page_array[0], ((int)$pager_total[0]) - 1));
    $pager_limits[0] = $per_page;
    $build['pager'] = array(
      '#theme' => 'pager',
    );
  }
  return $build;
}

/**
 * Menu callback; presents the "top pages" page.
 */
function mongodb_statistics_top_pages() {
  
  global $pager_page_array, $pager_total, $pager_total_items, $pager_limits;
  $ttl = variable_get('mongodb_statistics_flush_accesslog_timer', 259200);
  drupal_set_title(
    t('Top pages in the past %interval',
      array('%interval' => format_interval($ttl))
    ),
    PASS_THROUGH);
  
  $per_page = 30;
  $page = isset($_GET['page']) ? $_GET['page'] : '';
  $pager_page_array = explode(',', $page);
  $on_page = (int) $pager_page_array[0];
  
  
  $header = array(
    //array('data' => t('Hits'), 'field' => 'hits', 'sort' => 'desc'),
    array('data' => t('Hits')),
    array('data' => t('Page')),
    array('data' => t('Average page generation time')),
    array('data' => t('Total page generation time'))
  );
  
  $sourcecollectionname = variable_get('mongodb_accesslog_collection_name', 'accesslog');
  $targetcollectionname = $sourcecollectionname ."_top_pages";
  $collection = mongodb_collection($targetcollectionname);
  $mdb = $collection->db;
  
  // we won't use a mongodb->group but a map/reduce as we may have a lot of backends
  // you may need to use cron tasks to perform this task instead if your logs are big
  // maybe using 'merge' instead of 'replace'
  // TODO : provide an option to do it via cron
  // TODO : store a last mapreduce collection timestamp, keep result for at least 5 minutes,
  //  maybe provide a refresh button with a lastgen date
  
  // count: number of hits (count of path)
  // timer: total of timer(total)
  // avg: average computed on finalize
  
  $map = new MongoCode("function() {"
    . " emit( this.path,"
    . " {count: 1, timer: this.timer, path: this.path, title: this.title} );"
    . "}");
  $reduce = new MongoCode("function(key, values) {"
  ."var result = {count: 0, timer: 0, path: '', title: ''};"
  ."for(var i in values) {"
  ."  result.count+=values[i].count;"
  ."  result.timer+=values[i].timer;"
  ."  result.path=values[i].path;" // used to facilitate result reads only
  ."  result.title=values[i].title;" // used to facilitate result reads only, keep only the last one
  ."}"
  ."return result;"
  ."}");
  $finalize =  new MongoCode("function(key, val) {"
    ."val.avg = val.timer / val.count;"
    ."return val;"
    ."}");
    
  $referercount = $mdb->command(array(
    "mapreduce" => $sourcecollectionname,
    "map" => $map,
    "reduce" => $reduce,
    "out" => array('replace' => $targetcollectionname),
    "finalize" => $finalize
  ));
  $cursor = mongodb_collection($targetcollectionname)
    ->find()
    ->limit($per_page)
    ->skip($on_page * $per_page)
    ->sort(array('value.count' => -1,'value.avg'=>-1));
  
  $rows = array();
  foreach ($cursor as $page) {
    $rows[] = array(
      $page['value']['count'],
      _mongodb_statistics_format_item($page['value']['title'], $page['value']['path']),
      t('%time ms', array('%time' => round($page['value']['avg']))),
      format_interval(round($page['value']['timer'] / 1000)),
    );
  }

  if (!variable_get('mongodb_statistics_enable_access_log', 0)) {
    $build['mongodb_statistics_notice'] = array(
      '#type' => 'markup',
      '#markup' => '<div><p><strong>'
            . t('Acces log statistics are suspended, this content is not updated. '
            . 'You can activate access log statistics in the '
            . '<a href="@confurl">mongodb statistics configuration</a> zone.',
                array('@confurl' => url('admin/config/system/mongodb_statistics')))
            . '</stron></p></div>',
    );
  }
  $build['mongodb_statistics_top_pages_table'] = array(
    '#theme' => 'table',
    '#header' => $header,
    '#rows' => $rows,
    '#empty' => t('No statistics available.'),
  );

  // Add the pager.
  if ($on_page > 0 || count($rows) >= $per_page) {
    $cursor = $mdb->command(array(
      'distinct' => $targetcollectionname,
      'key' => '_id'
    ));
    $pager_total_items[0] = (is_array($cursor))? count($cursor['values']) : 0;
    $pager_total[0] = ceil($pager_total_items[0] / $per_page);
    $pager_page_array[0] = max(0, min((int) $pager_page_array[0], ((int)$pager_total[0]) - 1));
    $pager_limits[0] = $per_page;
    $build['pager'] = array(
      '#theme' => 'pager',
    );
  }
  return $build;
}

/**
 * Menu callback; presents the "top visitors" page.
 */
function mongodb_statistics_top_visitors() {

  global $pager_page_array, $pager_total, $pager_total_items, $pager_limits;
  $ttl = variable_get('mongodb_statistics_flush_accesslog_timer', 259200);
  drupal_set_title(t('Top visitors in the past %interval',
    array('%interval' => format_interval($ttl))), PASS_THROUGH);

  $per_page = 30;
  $page = isset($_GET['page']) ? $_GET['page'] : '';
  $pager_page_array = explode(',', $page);
  $on_page = (int) $pager_page_array[0];
  
  $header = array(
    array('data' => t('Hits')),
    array('data' => t('Visitor')),
    array('data' => t('Total page generation time')),
    array('data' => t('Host IP')),
  );
  
  $sourcecollectionname = variable_get('mongodb_accesslog_collection_name', 'accesslog');
  $targetcollectionname = $sourcecollectionname ."_visitors";
  $collection = mongodb_collection($targetcollectionname);
  $mdb = $collection->db;
  
  // we won't use a mongodb->group but a map/reduce as we may have a lot of backends
  // you may need to use cron tasks to perform this task instead if your logs are big
  // maybe using 'merge' instead of 'replace'
  // TODO : provide an option to do it via cron
  // TODO : store a last mapreduce collection timestamp, keep result for at least 5 minutes,
  //  maybe provide a refresh button with a lastgen date
  
  // count: number of hits (count)
  // timer: sum timestamp (sum)
  
  $map = new MongoCode("function() {"
    . " emit( this.uid+'#'+this.name+'#'+this.hostname,"
    . " {count: 1, timer: this.timer, uid: this.uid, name: this.name, hostname: this.hostname} );"
    . "}");
  $reduce = new MongoCode("function(key, values) {"
  ."var result = {count: 0, timer: 0, uid: 0, name: '', hostname: ''};"
  ."for(var i in values) {"
  ."  result.count+=values[i].count;"
  ."  result.timer+=values[i].timer;"
  ."  result.uid=values[i].uid;" // used to facilitate result reads only
  ."  result.name=values[i].name;" // used to facilitate result reads only
  ."  result.hostname=values[i].hostname;" // used to facilitate result reads only
  ."}"
  ."return result;"
  ."}");
  $referercount = $mdb->command(array(
    "mapreduce" => $sourcecollectionname,
    "map" => $map,
    "reduce" => $reduce,
    "out" => array('replace' => $targetcollectionname)
  ));
  $cursor = mongodb_collection($targetcollectionname)
    ->find()
    ->limit($per_page)
    ->skip($on_page * $per_page)
    ->sort(array('value.count' => -1,'value.timer' => -1));
  
  $rows = array();
  foreach ($cursor as $visit) {
    $rows[] = array(
      $visit['value']['count'],
      ( ($visit['value']['uid'])? theme('username', array('account' => (object) $visit['value'])) : $visit['value']['hostname']),
      format_interval(round($visit['value']['timer'] / 1000)),
      $visit['value']['hostname']
    );
  }

  if (!variable_get('mongodb_statistics_enable_access_log', 0)) {
    $build['mongodb_statistics_notice'] = array(
      '#type' => 'markup',
      '#markup' => '<div><p><strong>'
            . t('Acces log statistics are suspended, this content is not updated. '
            . 'You can activate access log statistics in the '
            . '<a href="@confurl">mongodb statistics configuration</a> zone.',
                array('@confurl' => url('admin/config/system/mongodb_statistics')))
            . '</stron></p></div>',
    );
  }
  
  $build['mongodb_statistics_top_visitors_table'] = array(
    '#theme' => 'table',
    '#header' => $header,
    '#rows' => $rows,
    '#empty' => t('No statistics available.'),
  );

  // Add the pager.
  if ($on_page > 0 || count($rows) >= $per_page) {
    // TODO: a simple count of the collection should be sufficient
    $cursor = $mdb->command(array(
      'distinct' => $targetcollectionname,
      'key' => '_id'
    ));
    $pager_total_items[0] = (is_array($cursor))? count($cursor['values']) : 0;
    $pager_total[0] = ceil($pager_total_items[0] / $per_page);
    $pager_page_array[0] = max(0, min((int) $pager_page_array[0], ((int)$pager_total[0]) - 1));
    $pager_limits[0] = $per_page;
    $build['pager'] = array(
      '#theme' => 'pager',
    );
  }
  return $build;
}

/**
 * Menu callback; presents the "referrer" page.
 */
function mongodb_statistics_top_referrers() {
  global $pager_page_array, $pager_total, $pager_total_items, $pager_limits;
  $ttl = variable_get('mongodb_statistics_flush_accesslog_timer', 259200);
  drupal_set_title(t('Top referrers in the past %interval',
    array('%interval' => format_interval($ttl))), PASS_THROUGH);

  $per_page = 30;
  $page = isset($_GET['page']) ? $_GET['page'] : '';
  $pager_page_array = explode(',', $page);
  $on_page = (int) $pager_page_array[0];
  
  $header = array(
    array('data' => t('Hits')),
    array('data' => t('Url')),
    array('data' => t('Last visit')),
  );
  
  $sourcecollectionname = variable_get('mongodb_accesslog_collection_name', 'accesslog');
  $targetcollectionname = $sourcecollectionname ."_referers";
  $collection = mongodb_collection($targetcollectionname);
  $mdb = $collection->db;
  
  // we won't use a mongodb->group but a map/reduce as we may have a lot of backends
  // you may need to use cron tasks to perform this task instead if your logs are big
  // maybe using 'merge' instead of 'replace'
  // TODO : provide an option to do it via cron
  // TODO : store a last mapreduce collection timestamp, keep result for at least 5 minutes,
  //  maybe provide a refresh button with a lastgen date
  
  // count: number of hits (count)
  // timestamp: last timestamp (max)
  $map = new MongoCode("function() {emit( this.url, {count: 1, timestamp: this.timestamp} );}");
  $reduce = new MongoCode("function(key, values) {"
  ."var result = {count: 0, timestamp: 0};"
  ."for(var i in values) {"
  ."  result.count+=values[i].count;"
  ."  if(values[i].timestamp > result.timestamp) {"
  ."    result.timestamp = values[i].timestamp;"
  ."  }"
  ."}"
  ."return result;"
  ."}");
  //TODO manage https in regex as well
  $regex= new MongoRegex('/^http:\/\/'.$_SERVER['HTTP_HOST'].'/');
  $query = array('url' => array( '$not' => $regex));
  $referercount = $mdb->command(array(
    "mapreduce" => $sourcecollectionname,
    "map" => $map,
    "reduce" => $reduce,
    "query" => $query,
    "out" => array('replace' => $targetcollectionname)
  ));
  $cursor = mongodb_collection($targetcollectionname)
    ->find()
    ->limit($per_page)
    ->skip($on_page * $per_page)
    ->sort(array('value.count' => -1,'value.timestamp',-1));

  $rows = array();
  foreach ($cursor as $ref) {
    $rows[] = array(
      $ref['value']['count'],
      _mongodb_statistics_link($ref['_id']),
      t('@time ago', array(
        '@time' => format_interval(REQUEST_TIME - $ref['value']['timestamp'])
      ))
    );
  }

  if (!variable_get('mongodb_statistics_enable_access_log', 0)) {
    $build['mongodb_statistics_notice'] = array(
      '#type' => 'markup',
      '#markup' => '<div><p><strong>'
            . t('Acces log statistics are suspended, this content is not updated. '
            . 'You can activate access log statistics in the '
            . '<a href="@confurl">mongodb statistics configuration</a> zone.',
                array('@confurl' => url('admin/config/system/mongodb_statistics')))
            . '</stron></p></div>',
    );
  }
  
  $build['mongodb_statistics_top_referrers_table'] = array(
    '#theme' => 'table',
    '#header' => $header,
    '#rows' => $rows,
    '#empty' => t('No statistics available.'),
  );

  // Add the pager.
  if ($on_page > 0 || count($rows) >= $per_page) {
    $cursor = $mdb->command(array(
      'distinct' => $targetcollectionname,
      'key' => '_id'
    ));
    $pager_total_items[0] = (is_array($cursor))? count($cursor['values']) : 0;
    $pager_total[0] = ceil($pager_total_items[0] / $per_page);
    $pager_page_array[0] = max(0, min((int) $pager_page_array[0], ((int)$pager_total[0]) - 1));
    $pager_limits[0] = $per_page;
    $build['pager'] = array(
      '#theme' => 'pager',
    );
  }
  return $build;
}

/**
 * Menu callback; Displays recent page accesses.
 */
function mongodb_statistics_access_log($aid) {
  $collectionname = variable_get('mongodb_accesslog_collection_name', 'accesslog');
  $access = mongodb_collection($collectionname)->findOne(array(
        '_id' => (int) $aid
  ));
  if ($access) {
    $rows[] = array(
      array('data' => t('URL'), 'header' => TRUE),
      l(url($access['path'], array('absolute' => TRUE)), $access['path'])
    );
    // It is safe to avoid filtering $access['title'] through check_plain because
    // it comes from drupal_get_title().
    $rows[] = array(
      array('data' => t('Title'), 'header' => TRUE),
      $access['title']
    );
    $rows[] = array(
      array('data' => t('Referrer'), 'header' => TRUE),
      ($access['url'] ? l($access['url'], $access['url']) : '')
    );
    $rows[] = array(
      array('data' => t('Date'), 'header' => TRUE),
      format_date($access['timestamp'], 'long')
    );
    $rows[] = array(
      array('data' => t('User'), 'header' => TRUE),
      theme('username', array('account' => (object) $access))
    );
    $rows[] = array(
      array('data' => t('Hostname'), 'header' => TRUE),
      check_plain($access['hostname'])
    );

    $build['mongodb_statistics_table'] = array(
      '#theme' => 'table',
      '#rows' => $rows,
    );
    return $build;
  }
  else {
    drupal_not_found();
  }
}

/**
 * Form builder; Configure access logging.
 *
 * @ingroup forms
 * @see system_settings_form()
 */
function mongodb_statistics_settings_form() {
  // TODO: name of collections
  // Access log settings.
  $form['access'] = array(
    '#type' => 'fieldset',
    '#title' => t('Access log settings'),
  );
  $form['access']['mongodb_statistics_enable_access_log'] = array(
    '#type' => 'checkbox',
    '#title' => t('Enable access log'),
    '#default_value' => variable_get('mongodb_statistics_enable_access_log', 0),
    '#description' => t("Log each page access with extended informations, referrer, user and session, etc. This may generate a big amount of data. Avoid it if you can, support of this stat in this module is incomplete, it won't work with proxy-cache servers and it may never be extended to do so."),
  );
  $form['access']['mongodb_accesslog_collection_name'] = array(
    '#type' => 'textfield',
    '#title' => t('Name of the collection in MongoDB'),
    '#default_value' => variable_get('mongodb_accesslog_collection_name','accesslog'),
    '#size' => 20,
    '#description' => t('The collection is sort of table for mongodb, by default data will be stored in an access_log collection. This settings requires that you have previously enabled the "Enable access log" checkbox.'),
  );
  $form['access']['mongodb_statistics_deleted_user_behavior'] = array(
    '#type' => 'select',
    '#title' => t('How to handle deleted users ?'),
    '#default_value' => variable_get('mongodb_statistics_deleted_user_behavior', MONGODB_STATISTICS_DELETED_USER_DO_NOTHING),
    '#options' => array(
       MONGODB_STATISTICS_DELETED_USER_DO_NOTHING => t('Do nothing'),
       MONGODB_STATISTICS_DELETED_USER_TRANSFER_ANONYMOUS => t('Transfer records to anonymous'),
       MONGODB_STATISTICS_DELETED_USER_DELETE => t('Delete records'),
     ),
    '#description' => t('When deleting a user, what should we do of access log records containing this user? Note that this is not the same as disabled users where the behavior it managed in central user configuration.'),
  );
  $form['access']['mongodb_statistics_flush_accesslog_timer'] = array(
    '#type' => 'select',
    '#title' => t('Discard access logs older than'),
    '#default_value' => variable_get('mongodb_statistics_flush_accesslog_timer', 259200),
    '#options' => array(0 => t('Never')) + drupal_map_assoc(array(3600, 10800, 21600, 32400, 43200, 86400, 172800, 259200, 604800, 1209600, 2419200, 4838400, 9676800), 'format_interval'),
    '#description' => t('Older access log entries (including referrer statistics) will be automatically discarded. (Requires a correctly configured <a href="@cron">cron maintenance task</a>.)', array('@cron' => url('admin/reports/status'))),
  );

  // Content counter settings.
  $form['content'] = array(
    '#type' => 'fieldset',
    '#title' => t('Content viewing counter settings'),
  );
  $form['content']['mongodb_statistics_count_content_views'] = array(
    '#type' => 'checkbox',
    '#title' => t('Count content views'),
    '#default_value' => variable_get('mongodb_statistics_count_content_views', 0),
    '#description' => t('Increment a counter each time content is viewed. (Requires a correctly configured <a href="@cron">cron maintenance task</a>, at least once a day.)', array('@cron' => url('admin/reports/status'))),
  );
  $form['content']['mongodb_node_counter_collection_name'] = array(
    '#type' => 'textfield',
    '#title' => t('Name of the collection in MongoDB'),
    '#default_value' => variable_get('mongodb_node_counter_collection_name','node_counter'),
    '#size' => 20,
    '#description' => t('The collection is sort of table for mongodb, by default data will be stored in a node_counter collection. This settings requires that you have previously enabled the "Count content views" checkbox.'),
  );
  $form['content']['mongodb_statistics_block_popular_expires'] = array(
    '#type' => 'textfield',
    '#title' => t('How many minutes should we cache the result of the "popular content" block?'),
    '#default_value' => variable_get('mongodb_statistics_block_popular_expires',0),
    '#size' => 4,
    '#description' => t('0 means no cache, 5 minutes should be a good starting point. This settings requires that you have previously enabled the "Count content views" checkbox.'),
  );
  $form['content']['mongodb_statistics_sync_db_tables_count_content_views'] = array(
    '#type' => 'checkbox',
    '#title' => t('Synchronize db tables asynchronously'),
    '#default_value' => variable_get('mongodb_statistics_sync_db_tables_count_content_views', 0),
    '#description' => t('When the cron job will run we will synchronize classical sql tables with node counters. Allowing Views module interactions, for example. By definition data will not be updated until cron execution'),
  );
  $form['content']['mongodb_statistics_day_timestamp'] = array(
  '#type' => 'textfield',
    '#title' => t('Advanced: last synchonized timestamp for SQL synchronization'),
    '#default_value' => variable_get('mongodb_statistics_day_timestamp',''),
    '#size' => 10,
    '#description' => t('An empty string means all MongoDB contents will be pushed back into mongodb_node_counter SQL table if you activate the "Synchronize db tables asynchronously" function. This is normally an internal settings, tracking the last timestamp of synchronization.'),
  );
  if (db_table_exists('node_counter')) {
    $form['content']['mongodb_statistics_migrate_statistics'] = array(
      '#type' => 'markup',
      '#markup' => '<div class="stats_migration"><p>'
          .t('It seems you have previously installed the statistics module. This module should be disabled while the mongodb statistics module is enabled. But you can migrate all data previously collected on the statistics module node_counter table and push them in the mongodb collection by using the link below.<br/>')
          . l(
             t('Go to the migration page for old statistics module data')
              , 'admin/config/system/mongodb_statistics/migration'
          )
          .'</p></div>',
    );
  }
  // User counter settings.
  $form['user'] = array(
    '#type' => 'fieldset',
    '#title' => t('User counters settings'),
  );
  $form['user']['mongodb_statistics_enable_user_counter'] = array(
    '#type' => 'checkbox',
    '#title' => t('Enable user counters'),
    '#default_value' => variable_get('mongodb_statistics_enable_user_counter', 0),
    '#description' => t('Increment some counters each time a logged in user is vieving a countent. (Requires a correctly configured <a href="@cron">cron maintenance task</a>, at least once a day.)', array('@cron' => url('admin/reports/status'))),
  );
  $form['user']['mongodb_user_counter_collection_name'] = array(
    '#type' => 'textfield',
    '#title' => t('Name of the collection in MongoDB'),
    '#default_value' => variable_get('mongodb_user_counter_collection_name','user_counter'),
    '#size' => 20,
    '#description' => t('The collection is sort of table for mongodb, by default data will be stored in a user_counter collection. This settings requires that you have previously enabled the "Enable user counters" checkbox.'),
  );
  $form['user']['mongodb_statistics_track_first_visit'] = array(
    '#type' => 'checkbox',
    '#title' => t('Track first visit on nodes for connected users'),
    '#default_value' => variable_get('mongodb_statistics_track_first_visit', 0),
    '#description' => t('Increment a counter for each user the first time a node content is viewed, that means each node record will store an attribute for each user id having visited the node.'),
  );
  $form['user']['mongodb_statistics_user_sync_interval'] = array(
    '#type' => 'select',
    '#title' => t('Interval of user counters synchronisation'),
    '#default_value' => variable_get('mongodb_statistics_user_sync_interval', 3600),
    '#options' => array(0 => t('Never')) + drupal_map_assoc(array(120,300,600,900,1800,3600, 10800, 21600, 32400, 43200, 86400, 172800, 604800), 'format_interval'),
    '#description' => t('User counters (like first time visits of nodes are pushed into mongodb. After that interval, and if the cron is running, theses counters will get pushed into MySQL, or at least some hook function will get called where you can connect some code '),
  );
  $form['user']['mongodb_statistics_user_sync_timestamp'] = array(
  '#type' => 'textfield',
    '#title' => t('Advanced: last synchonized timestamp for SQL synchronization of user counters'),
    '#default_value' => variable_get('mongodb_statistics_user_sync_timestamp',''),
    '#size' => 10,
    '#description' => t('This is normally an internal settings, tracking the last timestamp of synchronization.'),
  );
  return system_settings_form($form);
}
