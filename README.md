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

For scalability, I built a simple application. It basically sets and gets in a loop. The loop can be run a variable number of times. This application can have many instances to simulate multiple clients. As the application waits for a response from the server each time, it can be used to see how fast the server is. Some of the results are as follows (all results from a virtual machine, and with other applications running, so the results are not perfectly indicative of performance):

4 buckets:

10 instances, 1000 get/set pairs: 1.227s, 1.371s, 1.169s, 1.265s, 1.359s

15 instances 1000 get/set pairs: 1.972s, 1.999s, 1.963s, 1.927s, 2.143s

10 instances 2000 get/set pairs: 2.489s, 2.495s, 2.247s, 2.350s, 2.597s

10 buckets:

10 instances 1000 get/set pairs: 1.140s, 1.218s, 1.184s, 1.170s, 1.170s

15 instances 1000 get/set pairs: 1.844s, 1.935s, 1.856s, 1.849s, 1.863s

10 instances 2000 get/set pairs: 2.354s, 2.310s, 2.258s, 2.532s, 2.424s

All numbers are from Wireshark.


Instructions for Use
--------------------

The source can be found at https://github.com/guptha-/memstashed.

Once the source has been obtained, use make in the memstashed directory to make bin/memstashed.

bin/memstashed is the server executable and can be used directly.

To make the tests, give “make test” in the memstashed directory. The correctness tests can be run by going into memstashed/libmemcached/clients and running “./memcapable –a”. For the scalability test, execute “./scalability.sh $1 $2” in the memstashed/tests folder. First option is the number of get/set pairs, and the second option is the number of instances. Of course, bin/memstashed should be running while the tests are being performed.

Note that the stats command has very limited support. A few of the stats are sent back, but not all of them. I haven’t filled up the remaining places with dummy values because the memcapable test suite does not test stats too much.

