memstashed
==========

A multithreaded memcached server

Memstashed Design
-----------------

Salient Features
----------------

An unordered-map stores the entries according to the key. This allows constant time retrieval of entries using the key on average. A set of keys, ordered according to the timestamp of the entries, allows constant time retrieval of the least recently used entry.

Each of these data structures is subdivided into a configurable number of sub-structures, each one with a fine-grained lock. Distribution into these buckets is based on the key, and there is no assurance that the buckets will all be of the same size. If insertion fails due to lack of memory, first, eviction is tried from the same bucket. If there is no element already in the same bucket, eviction is tried from the next bucket and so on. Calculating LRU over all the buckets will lock all the sub-structures one by one, which is not what we want, so eviction is done bucket by bucket.

Within a bucket, eviction is based on LRU. A slight modification has been done to take data size into account. Instead of directly evicting the LRU, the 2 least recently used entries in the bucket are compared by size. The larger one is evicted. If there is only one element in the bucket, it is evicted unconditionally.

A feature of the design is that a live entry may get pushed out of the cache even though the same bucket contains an expired entry. Eviction depends on the timestamps, and so, if an expired entry has been accessed/ modified recently, it will have a newer timestamp. It is not possible to get expired entries from the timestamp-key pair because that will not be constant time. This is not as bad as it sounds because frequently accessed items will have new timestamps, and a deleted element will get to the end of the queue very quickly.


Tests Performed
---------------

The memcapable suite from libmemcached is used for verifying correctness. All the ascii test cases of the test suite have been run and they all pass. Only one test case needed to be modified – the cas value received by the test suite was changed into an unsigned long long int from an unsigned long int in order for it to work properly on x86 machines.


Instructions for Use
--------------------

The source can be found at https://github.com/guptha-/memstashed.

Once the source has been obtained, use make in the memstashed directory to make bin/memstashed.

bin/memstashed is the server executable and can be used directly.

To make the tests, give “make test” in the memstashed directory. The tests can be run by going into memstashed/libmemcached/clients and running “./memcapable –a”. Of course, bin/memstashed should be running while the tests are being performed.

Note that the stats command has very limited support. A few of the stats are sent back, but not all of them. I haven’t filled up the remaining places with dummy values because the memcapable test suite does not test stats too much.

