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

def addNode(kbuckets, newNode, myId, k):

	index = getBucket(myId, newNode.id)

	if(len(kbuckets[index]) == k):
		kbuckets[index].pop()

	kbuckets[index].insert(0, newNode)

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

# updates node node to be the most recent node in its k-bucket
def makeMostRecent(node, kbuckets):
	for lst in kbuckets:
		if node in lst:
			lst.remove(node)
			lst.insert(0,node)
			break

# returns id of next closest node, or none if no nodes are further than prevdist
def getNextClosest(kbuckets, prevDist, N, myId):

	# smallest legal distance we have seen, initialize to largest possible distance
	smallestDist = 2**N
	# id corresponding to that smallest distance
	smallestNode = -1

	for i in range(N):
		for node in kbuckets[i]:
			dist = myId ^ node.id

			if(dist > prevDist and dist < smallestDist):
				smallestDist = dist
				smallestNode = node

	if(smallestNode != -1):
		return smallestNode

	return None


# Takes an ID (use shared IDKey message type) and returns k nodes with
# distance closest to ID requested
#
# sends FindNode rpc to recipient looking for id requestedId
def rpcFindNode(recipient, requestedId):
	return 0

#prints the k-buckets, stored in a list of lists of Nodes
def printBuckets(k_buckets):

	for i in k_buckets:
		print("{}:".format(k_buckets.index(i)), end='')

		for j in i:
			print(" {}:{}".format(j.id, j.port), end='')
		print()

#connects nodes by exchanging ID, address, and port
#sends remote node FindNode(thisNode)
def bootstrap(remote_addr_string, remote_port_string, myId, k_buckets, k):
	remote_addr = socket.gethostbyname(remote_addr_string)
	remote_port = int(remote_port_string)

	with grpc.insecure_channel(remote_addr + ':' + str(remote_port)) as channel:
		try:
			stub = csci4220_hw4_pb2_grpc.KadImplStub(channel)
			response = stub.FindNode(myId)
		except:
			print("Try Failed in bootstrap")
			return

		responding_node = response.responding_node
		found = False
		for lst in k_buckets:
			if responding_node in lst:
				found = True
				break
		if not found:
			addNode(responding_node)

		nodeList = response.nodes
		for node in nodeList:

			if node.id == myId:
				continue

			found = False
			for lst in k_buckets:
				if node in lst:
					found = True
					break
			if found:
				continue

			addNode(k_buckets, node, myId, k)

		makeMostRecent(responding_node)
		print("After BOOTSTRAP({}), k_buckets now look like:".format(response.responding_node.id))
		printBuckets(k_buckets)


#attempts to find remote node, updates current node's k-buckets
def findNode(nodeID, kbuckets, k, N, myId):

	prevDist = 0

	sPrime = []

	# go through each bucket and iterate over the nodes until we have seen k
	for i in range(k):

		node = getNextClosest(kbuckets, prevDist, N, myId)

		if(node == None):
			break

		# update prevDist for next getNextClosest call
		prevDist = node.id ^ myId

		sPrime.append(node)


	# flag designating whether or not we have found the desired node
	done = False

	for node in sPrime:

		result = rpcFindNode(node, nodeID)

		makeMostRecent(node, kbuckets)


		# add node returned in result list if we do not have it in our kbuckets
		for resNode in result:
			foundFlag = False
			# if a node in the result is the node we are looking for, we found it
			# don't stop here because we still want to update our k-buckets
			if(resNode.id == nodeID):
				done = True

			for nodeList in kbuckets:
				if(resNode in nodeList):
					foundFlag = True
					break

			if(foundFlag == False):
				addNode(kbuckets, resNode, myId, k)



	print("After FIND_NODE command, k-buckets are:")
	printBuckets(kbuckets)

	if(done):
		print("Found destination id %s" % nodeID)

	else:
		print("Could not find destination id %s" % nodeID)





#like findNode(), but uses value instead of ID, updates k-buckets
def findValue():

#send Store RPC to single node with ID closest to key
def store(key, value, k_buckets, k):

#send Quit RPC to all nodes in k-buckets
#if received, delete sender from k-bucket
def quit(myId, k_buckets):

	for lst in k_buckets:
		for node in lst:
			print("Letting {} know I'm quitting.".format(node.id))

			with grpc.insecure_channel(node.addr + ':' + str(node.port)) as channel:
				try:
					stub = csci4220_hw4_pb2_grpc.KadImplStub(channel)
					response = stub.Quit(myId)
				except:
					print("Try Failed in quit")
					pass

	print("Shut down node {}".format(myId))

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
