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


def addNode(kbuckets, newId, k):

	index = getBucket(myId, newId)

	if(len(kbuckets[index]) == k):
		kbuckets[index].pop()

	kbuckets[index].insert(0, newId)

	return 0

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

# returns id of next closest node, or none if no nodes are further than prevdist
def getNextClosest(kbuckets, prevDist, N, myId):

	# smallest legal distance we have seen, initialize to largest possible distance
	smallestDist = 2**N
	# id corresponding to that smallest distance
	smallestId = -1

	for i in range(N):
		for node in kbuckets[i]:
			dist = myId ^ node

			if(dist > prevDist and dist < smallestDist):
				smallestDist = dist
				smallestId = node

	if(smallestId != -1):
		return smallestId

	return None


#prints the k-buckets, stored in a list of lists of IDs
def printBuckets(k_buckets):
	for i in k_buckets:
		print("{}:".format(k_buckets.index(i)), end='')
		for j in i:
			if j != -1:
				print(" {}:{}".format(i.index(j), j), end='')
		print()

#connects nodes by exchanging ID, address, and port
#sends remote node FindNode(thisNode)
def bootstrap(remote_addr_string, remote_port_string):
	remote_addr = socket.gethostbyname(remote_addr_string)
	remote_port = int(remote_port_string)
	channel = grpc.insecure_channel(remote_addr + ':' + str(remote_port))

#attempts to find remote node, updates current node's k-buckets
def findNode(nodeID, kbuckets, k, N, myId):

	prevDist = 0

	# go through each bucket and iterate over the nodes until we have seen k
	for i in range(k):

		node = getNextClosest(kbuckets, prevDist, N, myId)

		if(node == None):
			break

		prevDist = node ^ myId


	
	print("After FIND_NODE command, k-buckets are:")
	printBuckets(kbuckets)




#like findNode(), but uses value instead of ID, updates k-buckets
def findValue():

#send Store RPC to single node with ID closest to key
def store():

#send Quit RPC to all nodes in k-buckets
#if received, delete sender from k-bucket
def quit():

#run the program
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
