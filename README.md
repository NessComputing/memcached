Ness Computing memcached
========================

The Ness Computing memcached adds a couple of things to memcached:

- add zookeeper announcements (see below)
- add compile option to compile static on Fedora (RedHat) Linux.
  See README.NESSCOMPUTING for details.
- add long command line options
- galaxification for deployment with galaxy


Zookeeper Announcements for memcached
-------------------------------------

This fork adds zookeeper announcements to memcached. Our internal
service discovery system uses ephemeral nodes on Zookeeper to publish
the coordinates of a service (ip, port, type) to the platform.

The format of the announcements is hard coded and very Ness
specific. However, we do plan to open source the actual discovery
system soon.


This requires the zookeeper_st library from zookeeper.apache.org (we
use zookeeper-3.3.3), the json-c library from
http://oss.metaparadigm.com/json-c/ and libuuid from
http://www.ossp.org/pkg/lib/uuid/. The latter two are bundled with
Fedora Linux.

Configuration options
---------------------

* --with-zookeeper=<path>  - Path to the zookeeper installation (or "yes" to use a systemwide installation)
* --with-libjson=<path>    - Path to the libjson installation (system wide installed on fedora)
* --with-libuuid=<path>    - Path to the libuuid installation (system wide installed on fedora)


Command line options
--------------------

* -Z or --zookeeper-connect-string <connect>  - The Zookeeper connect string (<ip>:<port>[,<ip2>:<port2>])
* -z or --zookeeper-node-path <path>          - The base path on Zookeeper for announcements (default: /ness/srvc)
* -N or --ness-service-name <name>            - The service name to announce (default: memcached)
* -T or --ness-service-type <type>            - The service type to announce (not announced by default)

* -v enables zookeeper INFO logging
* -vv enables zookeeper DEBUG logging

Legal
-----

    (C) 2011 Ness Computing, Inc. 
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are
    met:

        * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

        * Redistributions in binary form must reproduce the above
    copyright notice, this list of conditions and the following disclaimer
    in the documentation and/or other materials provided with the
    distribution.
    
        * Neither the name of the Ness Computing nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.
    
    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
    OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

----

This product includes software developed at
The Apache Software Foundation (http://www.apache.org/).

Apache ZooKeeper
Copyright 2009 The Apache Software Foundation

