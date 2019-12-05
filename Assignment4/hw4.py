#!/usr/bin/env python3

# N is the number of bits in an ID

# ID is our id

# values is a list of key-value pairs, search this before sending messages for find_value command

# k-buckets is a list of N lists, each storing ids of other nodes we know about
# remember at most k other nodes across all buckets

# knownNodes is the number of nodes we have in our buckets

from concurrent import futures
import sys  # For sys.argv, sys.exit()
import socket  # for gethostbyname()

import grpc

import csci4220_hw4_pb2
import csci4220_hw4_pb2_grpc


# takes in our id and target id and returns bucket index
def getBucket(myId, otherId):
	index = 0

	distance = myId ^ otherId

	# we store our own id in bucket 0
	if(distance == 0):
		return index

	# divide by 2 repeatedly and break when distance hits 0 to get i index for the bucket
	for i in range(0,N):
		distance = distance // 2
		index += 1
		if(distance == 0):
			break


	return index



def run():
	if len(sys.argv) != 4:
		print("Error, correct usage is {} [my id] [my port] [k]".format(sys.argv[0]))
		sys.exit(-1)

	local_id = int(sys.argv[1])
	my_port = str(int(sys.argv[2])) # add_insecure_port() will want a string
	k = int(sys.argv[3])
	my_hostname = socket.gethostname() # Gets my host name
	my_address = socket.gethostbyname(my_hostname) # Gets my IP address from my hostname

	''' Use the following code to convert a hostname to an IP and start a channel
	Note that every stub needs a channel attached to it
	When you are done with a channel you should call .close() on the channel.
	Submitty may kill your program if you have too many file descriptors open
	at the same time. '''
	
	#remote_addr = socket.gethostbyname(remote_addr_string)
	#remote_port = int(remote_port_string)
	#channel = grpc.insecure_channel(remote_addr + ':' + str(remote_port))

if __name__ == '__main__':
	run()
