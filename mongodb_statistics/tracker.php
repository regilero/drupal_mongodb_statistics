<?php
/**
 * Use this file to allow minimal drupal bootstrap and still get necessary module for tracking.
 * This file is greatly inspired by the drupal jstat module jstat.php file
 * 
 * The main goal of this file is to record statistics in MongoDb and to avoid SQL queries
 * Hooks ar'ent available here, but hooks are defined on synchhronisation from mongodb to SQL,
 * So you should check there to add your own modules behaviors.
 * This tracker must stay fast and short, else why using proxy servers for your pages if
 * you send back a full drupal bootstrap in ajax for every page seen...
 * 
 */


// if this module is not installed in sites/all/modules/*/mongodb_statistics then fix that relative path to the root of Drupal (where we can find the index.php file)
define('MONGODB_STATISTICS_RELATIVE_PATH_TO_ROOT','../../../../..');
 
if (0===count($_POST)) {
  echo "this should be used with POST query only, sorry.";
  exit(0);
}

// Output headers & data and close connection
ignore_user_abort(TRUE);
ob_start();
header("Content-type: text/javascript; charset=utf-8");
header("Expires: Sun, 19 Nov 1978 05:00:00 GMT");
header("Cache-Control: no-cache");
header("Cache-Control: must-revalidate");
header("Content-Length: 13");
header("Connection: close");
print("/* mstats */\n");
ob_end_flush();
ob_flush();
flush();

chdir(MONGODB_STATISTICS_RELATIVE_PATH_TO_ROOT);
define('DRUPAL_ROOT', getcwd());
require_once DRUPAL_ROOT .'/includes/bootstrap.inc';
// We bootstrap a minimal Drupal with at least the variables
// This could be a major performance decrease as it implies thes 3 prevuious steps
//DRUPAL_BOOTSTRAP_CONFIGURATION
//DRUPAL_BOOTSTRAP_PAGE_CACHE
//DRUPAL_BOOTSTRAP_DATABASE 
// like in original statistics module, but variables can be cached in
// cache_bootstrap bin, you should store this bin in APC or MongoDB to avoid
// database calls.
// check as well your lock API, using a module bypassing the classical lock API
// could avoid database calls.
// We already try to detect if we will need a Session detection on the server siode by checking the content of $_COOKIE
drupal_bootstrap(DRUPAL_BOOTSTRAP_VARIABLES);
$sessname = session_name();
$insecure_session_name = substr($sessname, 1);
if (isset($_COOKIE[$sessname]) || isset($_COOKIE[$insecure_session_name])) {
  // Avoid later warnings about session-start :
  // session_start(): Cannot send session cache limiter - headers already sent
  // by emptying the session cache headers to send
  // The problem is that DRUPAL_BOOTSRAP_CONFIGURATION does a:
  // ini_set('session.cache_limiter', 'none');
  // So we must do that after that this step is done
  session_cache_limiter(FALSE);
  // this include DRUPAL_BOOTSTRAP_VARIABLES
  drupal_bootstrap(DRUPAL_BOOTSTRAP_SESSION);
}

// Can't use module_load_include as it's not loaded yet, so fallback to
// drupal_get_filename to find our module.
$path = '/' . dirname(drupal_get_filename('module', 'mongodb'));
require_once  DRUPAL_ROOT . $path .'/mongodb.module';

// --> response is already sent, now back at work. Let's record everything
if (variable_get('mongodb_statistics_count_content_views', 0)) {
  // We are counting content views.
  
  // A node has been viewed, so update the node's counters.
  $collectionname = variable_get('mongodb_node_counter_collection_name', 'node_counter');
  $collection = mongodb_collection($collectionname);
  $nid = isset($_POST['nid']) ? (int) $_POST['nid'] : 0;
  $collection->update(
    array('_id' => $nid),
    array(
      '$inc' => array(
        'totalcount' => 1,
        'daycount' => 1,
      ),
      '$set' => array(
        'timestamp' => REQUEST_TIME,
        'nid' => $nid,
      ),
    ),
    array('upsert' => TRUE)
  );
}

if (variable_get('mongodb_statistics_enable_user_counter', 0)) {
  global $user;
  if (variable_get('mongodb_statistics_track_first_visit', 0)) {
    // A node has been viewed, maybe we'll have to record it's the first time for that user.
    // here we will act only if DRUPAL_BOOTSTRAP_SESSION was run and if the user is not anonymous
    if (isset($user) && $user->uid != 0) {
      $collectionname = variable_get('mongodb_user_counter_collection_name', 'user_counter');
      $collection = mongodb_collection($collectionname);
      $uid = (int) $user->uid;
      $nid = isset($_POST['nid']) ? (int) $_POST['nid'] : 0;
      if ($nid) {
        $find = array(
          '_id' => $uid,
          'visited' => $nid,
        );
        $user_record = $collection->findOne($find, array('_id'));
        if (!$user_record) {
          // the user never visited that node before
          // record it and update timestamp for sync operations
          $collection->update(
            array('_id' => $uid),
            array(
              '$addToSet' => array(
                'visited' => $nid, // this one is the 'all-time visited array of nid
                'newvisited' => $nid, // this one contains the new visited ones, will have to be emptied on sync operations
              ),
              '$set' => array(
                'timestamp' => REQUEST_TIME
              ),
            ),
            array('upsert' => TRUE)
          );
        }
      }
    }
  }
}
exit(0);