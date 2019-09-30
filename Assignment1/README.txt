# NetProg Assignment 1
Andrew Conley (conlea@rpi.edu)
Jonathon Schmalz (schmaj@rpi.edu)

tftpserv.c: A TFTP server according to RFC 1350, using fork() calls for client handling.
	Transfers are in octet mode only, with resends every second until timeout after 10 seconds.
	The range of TIDs is specified as command line arguments.

Issues: Miscommunication and misunderstanding of the specifications lead to most of the problems.
	We were under the impression that once a transfer was complete, the server disconnected from the client and would not listen for more operations. Therefore, we failed many tests in the beginning.
	Submitty also caused problems in the debugging stage, due to it crashing or otherwise being unable to autograde our submissions. 

