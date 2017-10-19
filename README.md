# lmdb

FLEXlm license servers, in their various incarnations as that product has exchanged hands, have never had what I would call a strong license logging or tracking mechanism.  The log lines produced by the daemons cites the hostname that the client itself provided and not something more reliable (like the IP address of the client end of the connection).  This means you get to see what a Mac user set his or her generic computer name to (e.g. *Bob's Mac*).  Retrieving simple monitoring information -- like how many of each license feature are in-use at that moment -- requires parsing lengthy output from the `lmstat` command.  And that nets you instantaneous information, not ongoing statistics that can help to inform the decision to buy more seats, etc.

The **lmdb** project encompasses a library of code and command line utilities for:
- updating an SQLite database containing FLEXlm feature definition tuples (feature name, vendor, version) and timestamped in-use and issued counts and expiration timestamps for those features
- generating reports from the SQLite database with a variety of temporal aggregation options and automated range selection
- a Nagios plugin to report on license expiration status and feature usage levels (with per-feature configurable warning/critical thresholds)
- updating round-robin database (RRD) files for each feature for easy generation of usage graphs

