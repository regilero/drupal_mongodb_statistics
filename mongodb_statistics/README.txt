Content
=======

This module provides a replacement for core statistics module.

*********************************
* WARNING : DEVELOPMENT VERSION *
* features may vary             *
* data loss could happen        *
*********************************

You should disable the core statistics module when activating this one.
The module provide a migration process (linked in the configuration page
of the module) to transfer all previously recorded node counters.

The core statistics module provides 2 types of statistics. Node counters
and Access Logs.

Node counters are used to generate :
- a block of most viewed nodes (day and all-time), a more recently viewed ones
- a counter of views for node content
- some counters on the node track tab
- a views connector, allowing for custom views fields on these node counters

Theses contents are still available in this module:
- the block is the same, except we add the possibility to make a time-based cache
of this block. We also provide it in the admin interface with all other 'top pages'
- views connectors may be available if you enable in configuration a sql backport
of all mongodb node counters via cron. This way the SQL database is aware of node
 ounters but only in an asynchronous way (that means synchronous per-request write
operations are done in mongodb and sometimes a summary of the results are pushed in
MySQL)

The AccessLog part of core statistics is mostly done also. We still lack custom sorts
 on tables and we did not rebuold the "Block IP" action on the reports (if someone 
 really wants that...).
We do not provide an SQL backport of the access_log statistics, so you cannot use views.
We do not provide a migration batch for old access log statistics gathered, it's in the
TODO list.

Check the TODO.txt file.

Install
=======
- Disable the core statistics module if you have it already (we provide a migration
 for node counters statistics, not yet for access log ones).
- Enable and configure the main MongoDB module, you will need a running mongodb server
- Enable the mongodb statistics module 
- Configure this module
- Read the TODO.txt file, enjoy and contribute

Configuration
=============
See the configuration page of this module (linked in the module list or in system->Statistics)