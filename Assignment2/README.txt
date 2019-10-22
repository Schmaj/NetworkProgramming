# NetProg Assignment 1
Andrew Conley (conlea@rpi.edu)
Jonathon Schmalz (schmaj@rpi.edu)

wordGame.c: A server that communicates using TCP, that supports up to 5 clients, and plays a 		word game.
	The clients try to guess a random word chosen by the server from a dictionary file.
	When first connected, clients login with a username not previously chosen, not case sensitive.
	After each incorrect guess, the number of letters correct and letters correctly placed is broadcasted to all.
	Upon a correct guess by a client, the server send a message to all clients, and disconnects all clients.

Issues: We are missing 2 points in Test 7 for an unknown error. We have no idea how to debug the 	 error. It seems to be a client-side error, however, due to the output above it saying
	"CLIENT ERROR".

Approximate Total Man Hours: 12 Hrs.

Breakdown: 60% Conley, Andrew; 40% Schmalz, Jonathon