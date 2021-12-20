# Modified UDP Protocol

This program reliably performs the following 5 actions between a server and a client even on an unstable link.

1) get - Transfers a requested file from the server back to the client
2) put - Transfers a file from the client to the server
3) delete - Deletes a file from the server's file list
4) list - Lists the files stored in the server's file list (excludes the server c file and the compiled object)
5) exit - Terminates both the server and the client

The program uses Stop-and-wait protocol to reliably send messages, maintaining sequence numbers, timeouts and acknowledgements.

The makefile compiles both the files using the make command and can compile either the Server or Client code by using make server or make client respectively.

Once compiled, the client code can be run with the following command:
```
./client hostname portno
```
And the server code can be run with the following command:
```
./server portno
```
