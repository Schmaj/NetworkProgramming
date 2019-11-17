# NetProg Assignment 1
Andrew Conley (conlea@rpi.edu)
Jonathon Schmalz (schmaj@rpi.edu)

client.c: Connects to server to form a distributed sensor network.
	Has a range, and X and Y coordinates that represent it's location in relation to other sensors/nodes.
	Can move and send data to base stations and other sensors.

server.c: Receives connections from sensors(clients) to form a distributed sensor network.
	Reads data from a text file to initialize base station with their locations and connections to other base stations.
	Can send data messages between sensors and base stations.

Approximate Total Man Hours: 30 Hrs.

Breakdown: 60% Conley, Andrew; 40% Schmalz, Jonathon