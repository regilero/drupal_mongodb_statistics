<?php

/**
 * hook_mongodb_statistics_firstvisits
 *
 * Hook used in the cron synchonisation of user counters
 * 
 * We are giving you the list of nodes visited by some users since last synchonisations.
 * 
 * @param $visits is an array indexed by uid, containing for each uid the list (array) of newly visited nodes (nid)
 * @return unknown_type
 */
function hook_mongodb_statistics_firstvisits($visits) {
  
  foreach($visits as $uid => $nid_array) {
    foreach($nid_array as $k => $nid) {
      $node = node_load($nid);
      // Award the points.
      userpoints_userpointsapi(array(
        'uid' => $uid,
        'points' => variable_get("labs_vis_default", 0),
        'moderate' => FALSE,
        'time_stamp' => REQUEST_TIME,
        'operation' => 'userpoints_1st_visit',
        'entity_id' => $nid,
        'entity_type' => 'node',
        'description' => t('visited : @title', array('@title' => $node->title)),
        //'tid' => variable_get('my_point_category'),
        'display' => FALSE,
      ));
    }
  }
  
}