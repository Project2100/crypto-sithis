# crypto-sithis

A client-server suite for remote file encryption-decryption.

## Authors
- Project2100
- llde

## History

This project is the resulting implementation for a Systems Programming exam in
the Computer Science course of the University of Rome "La Sapienza".

[Original project specification (Italian)](http://www.iac.rm.cnr.it/%7Emassimo/PONE2016_17.html)

## Description

The project can be compiled for Windows and Linux OSes. BSD support is tentative.

There are 3 executables: client, server and crypto. 

Crypto must be kept alongside the server, and it will assist in performing the encryption tasks;
the server and client can reside on different machines and will communicate over a TCP socket
in a custom protocol described by the specification.

A server can communicate with multiple clients, the client limit can be specified at server start with the option -u, default is 4 clients maximum.




