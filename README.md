mod_bunny: distribute Nagios checks with RabbitMQ
=================================================

**mod_bunny** is a Nagios Event Broker (NEB) module that publishes host & service checks through a [RabbitMQ](http://www.rabbitmq.com/) AMQP broker, allowing you to distribute the checking load over multiple workers. It is similar to and heavily inspired from [Mod-Gearman](https://labs.consol.de/nagios/mod-gearman/). The official worker implementation is [bunny](http://github.com/cloudwatt/bunny) (written in Go).

Requirements
------------

**mod_bunny** relies on the following software:

* RabbitMQ C AMQP client [library](http://github.com/alanxz/rabbitmq-c) (<= 0.3.0)
* Jansson C JSON [library](http://www.digip.org/jansson/) (>= 2.2)

Installation
------------

Currently the installation method a little rough on the edges, since it needs the Nagios source tree to be compiled. The Nagios sources have to be "prepared" because the required `config.h` header is only available once the `./configure` script has been successfully executed (no need to compile the sources). Once the sources are ready to be used, build the module as follows:

Note: you have to use the **exact** same sources version as the Nagios daemon binary.

```
NAGIOS_SOURCES=/usr/src/nagios-3.2.3 make
```

Once compiled, copy the binary module `mod_bunny.o` to Nagios's modules directory (usually `/usr/lib/nagios3/modules`).

Configuration
-------------

In Nagios main configuration file (usually `/etc/nagios3/nagios.cfg`), add the following line (replace `/usr/lib/nagios3/modules` with the exact location of the compiled module on your system):

```
broker_module=/usr/lib/nagios3/modules/mod_bunny.o /etc/nagios3/mod_bunny.conf
```

Note: the parameter is the absolute path to **mod_bunny**'s configuration file; not specifying this parameter will make **mod_bunny** only use its default settings, which will probably won't work for you. Also, if you use multiple broker modules, declare `mod_bunny.o` first as it needs to intercept some events early.

The configuration file is using JSON format. Here are the supported settings and their default value:

* `"host": "localhost"` Broker hostname or address
* `"port": 5672` Broker port
* `"vhost": "/"` Broker virtual host
* `"user": "guest"` Broker account username
* `"password": "guest"` Broker account password
* `"publisher_exchange": "nagios_checks"` Broker exchange to connect to for publishing checks messages
* `"publisher_exchange_type": "direct"` Broker publisher exchange type*
* `"publisher_routing_key": ""` Routing key to apply when publishing check messages
* `"consumer_exchange": "nagios_results"` Broker exchange to connect to for consuming checks result messages
* `"consumer_exchange_type": "direct"` Broker consumer exchange type
* `"consumer_queue": "nagios_results"` Queue to bind to for consuming check result messages
* `"consumer_binding_key": ""` Binding key to use to consume check result messages
* `"local_hostgroups": []` Hostgroups** for which __mod_bunny__ won't override checks (Nagios-local checks)
* `"local_servicegroups": []` Servicegroups** for which __mod_bunny__ won't override checks (Nagios-local checks)
* `"retry_wait_time": 3` Time to wait (in seconds) before trying to reconnect to the broker
* `"debug": false` Debugging flag (useful for troubleshooting error)

\* : To benefit from the _round-robin_ load-balancing RabbitMQ feature, the publisher exchange **MUST** be of type _direct_. Read [this](http://www.rabbitmq.com/tutorials/amqp-concepts.html#exchange-direct) to understand why.

\*\* : `local_hostgroups` and `local_servicegroups` array elements are strings describing [shell patterns](http://www.gnu.org/software/findutils/manual/html_node/find_html/Shell-Pattern-Matching.html), e.g. `["*-servers", "nagios_local"]`

Basic configuration example:

```
{
  "host": "some.amqp.broker.example.net",
  "user": "bunny",
  "password": "S3curEP4$$w0rd!"
}
```

Compatibility
-------------

**mod_bunny** has been tested with Nagios versions 3.2.3, 3.4.1 and 3.5.0 on Ubuntu Linux. Let me know if you successfully made it work on other platforms/versions.

Bugs
----

Probably. The documentation related to NEB development is almost nonexistent, and the Nagios source code is a nightmare. I heavily relied on Mod-Gearman source code to understand how Nagios internals work, but I might have got or done some things wrong.

Currently **mod_bunny** only handles host/service checks events. I have no plans to support other events at the moment, but contributions are welcome.

License / Copyright
-------------------

This software is released under the MIT License.

Copyright (c) 2013 Marc Falzon / Cloudwatt

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
