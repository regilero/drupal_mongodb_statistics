<?php

function mongodb_statistics_sql_migration_form() {
  $nbrecords = db_query('SELECT COUNT(nid) FROM {node_counter}')->fetchField();
  $form['help'] = array(
    '#type' => 'markup',
    '#markup' => '<div class=migration-help><p>'
        .t('<strong>Warning:</strong> By submiting this form you will empty the current node_counter table '
          .'filled by the core statistics module <strong>(:nbrecords records)</strong>. If not already done,'
          .' you should disable the core statistics module now that you are using the mongodb statistics.'
          .'<br/>Nodes counters that are not present in mongodb will be added, total counts of existing '
          .'ones will be incremented. Daily counts are by default ignored. Counters will also be synchronised'
          .' with the new mongodb_node_counter table if you have activated the asynchronous sql '
          .'synchronisation of counters.<br/>'
          .'After migration, synchronised records are removed from the core statistics node_counter table, so'
          .' you can safely run this batch several times.',
        array(':nbrecords' => $nbrecords))
        .'</p></div>',
  );
  $form['daily'] = array(
    '#type' => 'checkbox',
    '#title' => t('Include daily counts in the migration.'),
    '#default_value' => FALSE,
  );
  $form['nb_by_step'] = array(
    '#type' => 'textfield',
    '#title' => t('How many records we should try to update at each batch step? (too much rows to handle may generate timeouts)'),
    '#default_value' => 50,
    '#size' => 4,
  );
  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Migrate'),
  );
  return $form;
}
/**
 * launch the core API when form is submited
 * @param $form
 * @param $form_state
 * @return unknown_type
 */
function mongodb_statistics_sql_migration_form_submit(&$form, &$form_state) {
  $batch = array(
    'title' => t('Migration from core sql statistics to mongodb node counter'),
    'operations' => array(
      array(
        'mongodb_statistics_sql_migration_loop', array(
          'daily' => $form_state['values']['daily'],
          'nb_by_step' => $form_state['values']['nb_by_step'],
        ),
      ),
    ),
    'finished' => 'mongodb_statistics_migration_end_callback',
    'file' => drupal_get_path('module', 'mongodb_statistics') . '/mongodb_statistics.migration.inc',
  );
  batch_set($batch);
}

/**
 * one batch task, taking 20 records and syncing them
 */
function mongodb_statistics_sql_migration_loop($with_daily_counts, $nb_by_step,array &$context) {
  if (empty($context['sandbox'])) {
    $max = db_query('SELECT COUNT(nid) FROM {node_counter}')->fetchField();
    $context['sandbox'] = array();
    $context['sandbox']['progress'] = 0;
    $context['sandbox']['max'] = $max;
    $context['sandbox']['current_nid'] = 0;
  }
  
  $use_async_sql_tables = variable_get('mongodb_statistics_sync_db_tables_count_content_views',0);
  
  $daycount = ($with_daily_counts)? ',daycount' : '';
  // get nb_by_step records
  $limit = $nb_by_step;
  $core_records = db_query_range(
   'SELECT nid,totalcount'.$daycount.',timestamp'
   .' FROM {node_counter}'
   .' WHERE nid > :nid'
   .' ORDER BY nid ASC',
   0,
   $limit,
   array(
     ':nid' => $context['sandbox']['current_nid']
   )
  );
  $last_nid = 0;
  // do things
  foreach($core_records as $core_record) {
    // first push the core stat record into mongodb
    $merged = mongodb_statistics_sql_migration_pushToMongo($core_record,$with_daily_counts);
    if ($use_async_sql_tables) {
      // if we track records in SQL table for views compatibility,
      // then do it now as the cron job may never catch the old
      // timestamps of newly inserted records
      mongodb_statistics_sql_migration_pushToSQL($merged);
    }
    // and remove rows that are now synced
    mongodb_statistics_sql_migration_delete($core_record);
    $context['results']['count']++;
    $last_nid = $core_record->nid;
    // DEBUG
    //sleep(1);
  }
  $context['sandbox']['progress']= $context['results']['count'];
  $context['sandbox']['current_nid'] = $last_nid;
  $context['message'] = t('migration done for :number of :max records', array(
   ':number' => $context['sandbox']['progress'],
   ':max' => $context['sandbox']['max'],
  ));
  
  if ($context['sandbox']['progress'] != $context['sandbox']['max']) {
    // avoid problems with 0.999999985 floats converted to 1 (end)
    if ($context['sandbox']['max']==$context['sandbox']['progress']) {
      $finished = 1;
    } else {
      $finished = round(($context['sandbox']['progress'] / $context['sandbox']['max'])-0.01,2);
    }
    $context['finished'] = $finished;
  }
}

function mongodb_statistics_migration_end_callback($success, $results, $operations) {
  // The 'success' parameter means no fatal PHP errors were detected. All
  // other error management should be handled using 'results'.
  if ($success) {
    $message = format_plural($results['count'], 'One record processed.', '@count records processed.');
  }
  else {
    $error_operation = reset($operations);
    drupal_set_message(t('An error occurred while processing @operation with arguments : @args',
      array(
        '@operation' => $error_operation[0],
        '@args' => print_r($error_operation[0], TRUE)
      )
    ));
  }
  drupal_set_message($message);
  // Providing data for the redirected page is done through $_SESSION.
  foreach ($results as $result) {
    $items[] = t('Loaded node %title.', array('%title' => $result));
  }
  $_SESSION['my_batch_results'] = $items;
}

/**
 *
 * @param $core_record from core stat sql
 * @param $with_daily_counts bool
 * @return mongodb updated record
 */
function mongodb_statistics_sql_migration_pushToMongo($core_record,$with_daily_counts){
    $collectionname = variable_get('mongodb_node_counter_collection_name', 'node_counter');
    $collection = mongodb_collection($collectionname);
    $nid = (int) $core_record->nid;
    $insert = array(
      'nid' => $nid,
      'timestamp' => $core_record->timestamp,
      'timestampgen' => $core_record->timestamp,
    );
    $increment = array(
      'totalcount' => (int) $core_record->totalcount,
      'totalcountgen' => (int) $core_record->totalcount,
    );
    if ($with_daily_counts) {
      $increment['daycount'] = (int) $core_record->daycount;
      $increment['daycountgen'] = (int) $core_record->daycount;
    }
    $collection->update(
      array('_id' => $nid),
      array(
        '$inc' => $increment,
        '$set' => $insert,
      ),
      array('upsert' => TRUE, 'safe' => TRUE)
    );
    $record = $collection->findOne(
      array('_id' => $nid ),
      array(
        'nid',
        'timestamp',
        'totalcount',
        'daycount',
        'timestampgen',
        'totalcountgen',
        'daycountgen',
      )
    );
    return $record;
}
/**
 * Push updated stats in the mongodb_node_counter SQL tables
 *
 * This function should only be used if you want a backtrack of
 * mongodb node_counter collection in SQL which is normally performed
 * with the cron. Here it's enforced for each row as the cron will
 * only work with newest timestamps
 *
 * @param $row : the row from mongodb after stats updates
 * @return unknown_type
 */
function mongodb_statistics_sql_migration_pushToSQL($row){
  db_merge('mongodb_node_counter')
    ->key(array('nid' => $row['nid']))
    ->fields(array(
      'daycount' => $row['daycount'],
      'totalcount' => $row['totalcount'],
      'timestamp' => $row['timestamp'],
      'daycountgen' => $row['daycountgen'],
      'totalcountgen' => $row['totalcountgen'],
      'timestampgen' => $row['timestampgen'],
    ))
    ->execute();
  
}
/**
 * Now that the original node_counter record is synchronised we remove it
 *
 * @param $core_record
 * @return unknown_type
 */
function mongodb_statistics_sql_migration_delete($core_record) {
  db_delete('node_counter')
    ->condition('nid', $core_record->nid)
    ->execute();
}
