# NetProg Assignment 1
Andrew Conley (conlea@rpi.edu)
Jonathon Schmalz (schmaj@rpi.edu)

wordGame.c: A server that communicates using TCP, that supports up to 5 clients, and plays a 		word game.
	The clients try to guess a random word chosen by the server from a dictionary file.
	When first connected, clients login with a username not previously chosen, not case sensitive.
	After each incorrect guess, the number of letters correct and letters correctly placed is broadcasted to all.
	Upon a correct guess by a client, the server send a message to all clients, and disconnects all clients.

Issues: Miscommunication and misunderstanding of the specifications lead to most of the problems.
	We were under the impression that once a transfer was complete, the server disconnected from the client and would not listen for more operations. Therefore, we failed many tests in the beginning.
	Submitty also caused problems in the debugging stage, due to it crashing or otherwise being unable to autograde our submissions. 

Approximate Total Man Hours: 12 Hrs.

Breakdown: 60% Conley, Andrew; 40% Schmalz, Jonathon