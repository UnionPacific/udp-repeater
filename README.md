# UDPrepeater

UDPrepeater is a general purpose, configurable UDP forwarding/repeating daemon for Linux. It is useful for repeating one-way streams of data from a single sender to multiple receivers, and for forwarding UDP traffic to different receivers based upon source or destination IP addresses or UDP ports.

## Usage

There are two ways you can use the repeater

* Use the makefile to build the project, create a json configuration file with your rules
* Integrate repeater.c into your own C program using the API defined below

### Building with the Makefile

This is the typical usage if you wish to use the repeater as-is.
1. Download the project
2. Run make from the src directory to create the binary file bin/repeater
3. Start the repeater with two arguments: the .json configuration file, and the name of the log file the program should create

```
$ cd src/
$ make
$ cd ../
$ bin/repeater conf/example_rules.json repeater.log
```

This forks off a child process which will run in the background, silently forwarding packets based on your .json rules. If you wish to kill the process, you can use `pkill repeater`

### Testing the Build

A test script is included at test/test_example_rules.pl

**You can test the repeater as follows**

* Start the repeater with the example rules provided
    * `./bin/repeater ./conf/example_rules.json test.log`
* Run the test script
    * `./test/test_example_rules.pl`

Please note the test script does not kill the repeater process. You must run `pkill repeater` (kills all repeater processes), or `kill pid` where pid is the process ID of the repeater running the example config, in order to kill the repeater once you are done testing.

### Programmers API

repeater.c provides an API which can be called directly by a C application to set up and start the repeater daemon. Simply drop src/repeater.c into your source directory and `#include "repeater.h"`. You will also need to be sure repeater.h and uthash.h are in your include paths.

**Setting up the repeater**
* `void create_listener(int id, uint32_t address, uint16_t port);`
    * id: The ID of the listener
    * address: the 32-bit IP address to bind the listening socket to (host byte order). Can be `0` to specify listening on all interfaces of the machine.
    * port: the 16-bit UDP port to bind the listening socket to (host byte order)
* `void create_transmitter(int id, uint32_t address, uint16_t port);`
    * id: the ID of the transmitter
    * address: the 32-bit IP address to bind the transmitting socket to (host byte order). Can be `0` to specify using any interface. Useful for binding to localhost to ensure traffic is not sent outside the machine.
    * port: the 16-bit UDP port to bind the transmitting socket to (host byte order). Can be `0` to use any open port.
* `void create_target(int id, uint32_t address, uint16_t port, int transmitter_id);`
    * id: The ID of the target
    * address: The 32-bit IP destination address (host byte order)
    * port: The 16-bit UDP destination port (host byte order)
    * transmitter_id: The ID of the transmitter to use to send packets to this target
* `void create_map(int listener_id, uint32_t src_address, uint16_t src_port, int target_id);`
    * Packets received on "listener_id" from "src_address:src_port" will be sent using "target_id"
    * (all parameters in host byte order)

**Starting the repeater**
* `int start_repeater(char* logfile);`
    * logfile: String with path to the logfile to open
    * This forks off a new process to start forwarding packets using the rules you have set up

## Example

The example_rules.json file is included to provide an example configuration file for use when building the repeater as a standalone program. The file creates rules that will perform the following translations:

* **Packets sent from 127.0.0.1:2000 to UDP port 8001**
    * Forwarded to 127.0.0.1:9000 from an unspecified UDP port (could be any that was open on the machine)
* **Packets sent from 127.0.0.1:2001 to UDP port 8002**
    * Forwarded to 127.0.0.1:9000 from an unspecified UDP port (could be any that was open on the machine)
    * Forwarded to 127.0.0.1:9001 from UDP port 6000

### JSON Format

The JSON object should be made up of four arrays, titled "listen", "transmit", "target", and "map". The objects contained in each array should be as follows.

* "listen" object
    * "id" : Number
    * "address" : String ("*" for any, or IPv4 address for specific interface to listen on)
    * "port" : String (UDP port to bind listener to)
* "transmit" object
    * "id" : Number
    * "address" : String ("*" for any, or IPv4 address for specific interface to transmit from)
    * "port" : String (UDP port to bind transmitter to)
* "target" object
    * "id" : Number
    * "address" : String (IPv4 destination address)
    * "port" : String (UDP destination port number)
    * "transmitter" : Number (ID of the transmitter to use)
* "map" object
    * "source" : Number (Incoming listener ID number)
    * "address" : String (IPv4 source address)
    * "port" : String (UDP source port number)
    * "target" : Array of numbers (List of target IDs to use for forwarding packets which match source/address/port)

## Authors

* [Thomas Coe](https://github.com/thomascoe) - Initial creation for Union Pacific Railroad internal use

## Acknowledgements

This project utilizes two external libraries. If you are using the programmers API, you only depend on uthash. Thank you to the creators and contributors of the following

* [json-parser](https://github.com/udp/json-parser/)
* [uthash](http://troydhanson.github.io/uthash/)

## License

* Apache 2.0
