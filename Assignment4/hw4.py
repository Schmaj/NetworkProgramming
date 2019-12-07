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
import threading # for threading

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

	N = 4

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


	return index - 1

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
# returns only nodelist from response
def rpcFindNode(recipient, requestedId, meNode):

	remote_addr = recipient.address
	remote_port = recipient.port
	with grpc.insecure_channel(remote_addr + ':' + str(remote_port)) as channel:
		idKey = csci4220_hw4_pb2.IDKey(node = meNode, idKey = requestedId)
		try:
			stub = csci4220_hw4_pb2_grpc.KadImplStub(channel)
			response = stub.FindNode(idKey)
		except:
			print("Try Failed in rpcFindNode")
			return

	return response.nodes

def rpcFindVal(recipient, requestedKey, meNode):

	remote_addr = recipient.address
	remote_port = recipient.port
	with grpc.insecure_channel(remote_addr + ':' + str(remote_port)) as channel:
		idKey = csci4220_hw4_pb2.IDKey(node = meNode, idkey = requestedKey)
		try:
			stub = csci4220_hw4_pb2_grpc.KadImplStub(channel)
			response = stub.FindValue(idKey)
		except:
			print("Try Failed in rpcFindValue")
			return

	return response

#prints the k-buckets, stored in a list of lists of Nodes
def printBuckets(k_buckets):

	for i in range(len(k_buckets)):
		print("{}:".format(i), end='')

		for j in k_buckets[i]:
			print(" {}:{}".format(j.id, j.port), end='')
		print()

#connects nodes by exchanging ID, address, and port
#sends remote node FindNode(thisNode)
def bootstrap(remote_addr_string, remote_port_string, myId, meNode, k_buckets, k):
	remote_addr = socket.gethostbyname(remote_addr_string)
	remote_port = int(remote_port_string)

	with grpc.insecure_channel(remote_addr + ':' + str(remote_port)) as channel:
		try:
			stub = csci4220_hw4_pb2_grpc.KadImplStub(channel)
			request = csci4220_hw4_pb2.IDKey(node=meNode, idkey=myId)
			response = stub.FindNode(request)
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
			addNode(k_buckets, responding_node, myId, k)

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

		makeMostRecent(responding_node, k_buckets)
		print("After BOOTSTRAP({}), k_buckets now look like:".format(
			response.responding_node.id))
		printBuckets(k_buckets)


#attempts to find remote node, updates current node's k-buckets
def findNode(nodeID, kbuckets, k, N, meNode):

	myId = meNode.id

	prevDist = -1

	sPrime = []

	# go through each bucket and iterate over the nodes until we have seen k
	for i in range(k):

		node = getNextClosest(kbuckets, prevDist, N, nodeID)

		if(node == None):
			break

		# update prevDist for next getNextClosest call
		prevDist = node.id ^ nodeID

		sPrime.append(node)


	# flag designating whether or not we have found the desired node
	done = False

	for node in sPrime:

		result = rpcFindNode(node, nodeID, meNode)

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
# ends as soon as value is found, even if we haven't updated all buckets
def findValue(key, kbuckets, k, N, meNode, storedDict):

	# data found locally
	if(key in storedDict):
		print("Found data \"%s\" for key %d" % (storedDict[key], key))
		return

	myId = meNode.id

	prevDist = -1

	sPrime = []

	# go through each bucket and iterate over the nodes until we have seen k
	for i in range(k):

		node = getNextClosest(kbuckets, prevDist, N, key)

		if(node == None):
			break

		# update prevDist for next getNextClosest call
		prevDist = node.id ^ key

		sPrime.append(node)

	for node in sPrime:

		result = rpcFindVal(node, key, meNode)

		makeMostRecent(node, kbuckets)

		# if value was found and returned
		if(result.mode_kv == True):
			print("Found value \"%s\" for key %d" % (result.kv.value, result.kv.key))
			return


		# add node returned in result list if we do not have it in our kbuckets
		for resNode in result.nodes:
			foundFlag = False

			for nodeList in kbuckets:
				if(resNode in nodeList):
					foundFlag = True
					break

			if(foundFlag == False):
				addNode(kbuckets, resNode, myId, k)

	print("Could not find key %d" % key)


#send Store RPC to single node with ID closest to key
def store(key, value, k_buckets, k, meNode, storedDict):

	minDist = meNode.id ^ key
	minNode = meNode

	# iterate over all known nodes to find node closest to the key
	for bucket in k_buckets:
		for node in bucket:
			dist = node.id ^ key
			if(dist < minDist):
				minDist = dist
				minNode = node

	# if this node is the closest node, store the value locally
	if(minNode.id == meNode.id):
		print("Storing key %d at node %d" % (key, meNode.id))

		storedDict[key] = value
		return


	print("Storing key %d at node %d" % (key, minNode.id))

	remote_addr = minNode.address
	remote_port = minNode.port
	with grpc.insecure_channel(remote_addr + ':' + str(remote_port)) as channel:
		keyVal = csci4220_hw4_pb2.KeyValue(node = meNode, key = key, value = value)
		try:
			stub = csci4220_hw4_pb2_grpc.KadImplStub(channel)
			response = stub.Store(keyVal)
		except:
			print("Try Failed in store")
			return

	return

#send Quit RPC to all nodes in k-buckets
#if received, delete sender from k-bucket
def quit(myId, meNode, k_buckets):

	for lst in k_buckets:
		for node in lst:
			print("Letting {} know I'm quitting.".format(node.id))

			with grpc.insecure_channel(node.address + ':' + str(node.port)) as channel:
				try:
					stub = csci4220_hw4_pb2_grpc.KadImplStub(channel)
					request = csci4220_hw4_pb2.IDKey(node=meNode, idkey=myId)
					response = stub.Quit(request)
				except:
					#print("Try Failed in quit")
					pass

	print("Shut down node {}".format(myId))



class KadImpl(csci4220_hw4_pb2_grpc.KadImplServicer):

	def __init__(self, k_buckets, dictionary, meNode, k):
		self.k_buckets = k_buckets
		self.dictionary = dictionary
		self.meNode = meNode
		self.k = k

	def FindNode(self, request, context):
		print("Serving FindNode(%d) request for %d" % (request.idkey, request.node.id))

		nodeID = request.idkey

		prevDist = -1
		N = 4
		nodeList = []

		# go through each bucket and iterate over the nodes until we have seen k
		for i in range(self.k):

			node = getNextClosest(self.k_buckets, prevDist, N, nodeID)

			# add "or node.id == request.idkey" for only sending good node
			if(node == None):
				break

			# update prevDist for next getNextClosest call
			prevDist = node.id ^ nodeID

			nodeList.append(node)


		found = False

		for bucket in self.k_buckets:
			for node in bucket:
				if(node.id == request.node.id):
					found = True
					break

		if(found == True):
			makeMostRecent(request.node, self.k_buckets)
		else:
			addNode(self.k_buckets, request.node, self.meNode.id, self.k)


		return csci4220_hw4_pb2.NodeList(responding_node = self.meNode, nodes = nodeList)
		# nodeID, kbuckets, k, N, meNode
		# return nodeList


	def FindValue(self, request, context):
		key = request.idkey
		print("Serving FindKey(%d) request for %d" % (key, request.node.id))

		# if we have the value, return it
		if(key in self.dictionary):
			keyVal = csci4220_hw4_pb2.KeyValue(node = self.meNode, key = key, value = self.dictionary[key])
			return csci4220_hw4_pb2.KV_Node_Wrapper(responding_node = self.meNode, mode_kv = True, kv = keyVal, nodes = [])


		nodeID = request.idkey

		prevDist = -1
		N = 4
		nodeList = []

		# go through each bucket and iterate over the nodes until we have seen k
		for i in range(self.k):

			node = getNextClosest(self.k_buckets, prevDist, N, nodeID)

			# add "or node.id == request.idkey" for only sending good node
			if(node == None):
				break

			# update prevDist for next getNextClosest call
			prevDist = node.id ^ nodeID

			nodeList.append(node)


		found = False

		for bucket in self.k_buckets:
			for node in bucket:
				if(node.id == request.node.id):
					found = True
					break

		if(found == True):
			makeMostRecent(request.node, self.k_buckets)
		else:
			addNode(self.k_buckets, request.node, self.meNode.id, self.k)


		return csci4220_hw4_pb2.NodeList(responding_node = self.meNode, nodes = nodeList)


	def Store(self, request, context):
		print("Storing key %d value \"%s\"" % (request.key, request.value))
		self.dictionary[request.key] = request.value

		# loop over nodes in k_buckets to see if this node is already there
		foundNode = False
		for bucket in self.k_buckets:
			for node in bucket:
				if(node.id == request.node.id):
					foundNode = True
					break

		# if node is in buckets, make it most recent
		if(foundNode == True):
			makeMostRecent(request.node, self.k_buckets)
		#else add to buckets
		else:
			addNode(self.k_buckets, request.node, self.meNode.id, self.k)
		IDKey = csci4220_hw4_pb2.IDKey(node=self.meNode, idkey=self.meNode.id)
		return IDKey

	def Quit(self, request, context):
		quittingNode = request.node
		for i in range(len(self.k_buckets)):
			if quittingNode in self.k_buckets[i]:
				print("Evicting quitting node {} from bucket {}".format(quittingNode.id, i))
				self.k_buckets[i].remove(quittingNode)
				IDKey = csci4220_hw4_pb2.IDKey(node=quittingNode, idkey=quittingNode.id)
				return IDKey
		print("No record of quitting node {} in k-buckets.".format(quittingNode.id))
		IDKey = csci4220_hw4_pb2.IDKey(node=quittingNode, idkey=quittingNode.id)
		return IDKey


def serve(listener_port, k_buckets, dictionary, meNode, k):
	server = grpc.server(futures.ThreadPoolExecutor(max_workers=10))
	csci4220_hw4_pb2_grpc.add_KadImplServicer_to_server(
		KadImpl(k_buckets, dictionary, meNode, k), server)
	server.add_insecure_port('[::]:' + listener_port)
	server.start()
	server.wait_for_termination()

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

	k_buckets = [[],[],[],[]]
	dictionary = dict()
	meNode = csci4220_hw4_pb2.Node(id=local_id, port=int(my_port), address=my_address)

	t = threading.Thread(target=serve, args=(my_port, k_buckets, dictionary, meNode, k,), daemon = True)
	t.start()
	#serve(my_port, k_buckets, dictionary, meNode, k)

	while(True):
		command = input('').split()

		#bootstrap(remote_addr_string, remote_port_string, myId, meNode, k_buckets, k):
		if command[0] == "BOOTSTRAP":
			remote_addr_string = command[1]
			remote_port_string = command[2]
			bootstrap(remote_addr_string, remote_port_string, meNode.id, meNode, k_buckets, k)

		elif command[0] == "FIND_NODE":
			print("Before FIND_NODE command, k-buckets are:")
			printBuckets(k_buckets)
			findNode(int(command[1]), k_buckets, k, 4, meNode)
			#nodeID, kbuckets, k, N, meNode

		elif command[0] == "FIND_VALUE":
			print("Before FIND_VALUE command, k-buckets are:")
			printBuckets(k_buckets)

			findValue(int(command[1]), k_buckets, k, 4, meNode, dictionary)
			#key, kbuckets, k, N, meNode, storedDict

			print("After FIND_VALUE command, k-buckets are:")
			printBuckets(k_buckets)

		#store(key, value, k_buckets, k, meNode, storedDict)
		elif command[0] == "STORE":
			key = int(command[1])
			value = command[2]
			store(key, value, k_buckets, k, meNode, dictionary)

		elif command[0] == "QUIT":
			quit(meNode.id, meNode, k_buckets)
			sys.exit()
			break

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
