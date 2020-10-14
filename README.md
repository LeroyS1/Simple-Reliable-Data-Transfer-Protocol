# Simple-Reliable-Data-Transfer-Protocol

## Overview
I wrote this customized reliable data transfer protocol in C. This protocol in context of server and client applications, where client transmits a file
as soon as the connection is established.
- TCP connection management, including connection setup (three-way handshake) and TCP tear-down.
- Large file transmission with pipelining.
- Loss recovery with Selective Repeat (SR).


## Instructions
- This protocol should be tested in a Linux enviroment because you can run and test it with simulated packet loss (using `tc` command).

- Step run:
1. Put all the files in the same directory.

2. To check the current parameters for a given network interface (e.g. lo0 for localhost; The interface
name of localhost may differ on your laptop, you can figure it out by command ifconfig ):
`tc qdisc show dev lo0`
3. If network emulation has not yet been setup or have been deleted, you can add a configuration called
root it to a given interface, with 10% loss without delay emulation for example:
`tc qdisc add dev lo0 root netem loss 10%`
4. To delete the network emulation:
`tc qdisc del dev lo0 root`

5. use `Make` to execute server.c and client.c.

6. `./server <PORT>`
    For example : `./server 5555`

7. `./client <HOSTNAME-OR-IP> <PORT> <FILENAME>`
    For example: `./client localhost 5555 testfile`


### Note: This project is not perfect, so I recommend you to test with small files.
