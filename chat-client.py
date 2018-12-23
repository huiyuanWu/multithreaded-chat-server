# -*-encoding:utf-8-*-

from socket import *
import _thread
import threading
import os
import sys

clientSocket = socket(AF_INET, SOCK_STREAM) 

#thread focusing on reading.
def reading_thread():
	while True:
		data = clientSocket.recv(1024).decode()
		print(data)
		if data == 'GOODBYE':
			os._exit(1)
	return

#thread focusing on writing.
def writing_thread():
	#while True:
	filepath = sys.argv[1]
	with open(filepath) as fp:
		for line in fp:
			message = line
		# quit if request
			if message == "quit":
				os._exit(1)
			print(message)
		# receive message from client and decode 
			clientSocket.send(message.encode())
	return

#start the reading thread
def run_reading_thread():
	t = threading.Thread(target=reading_thread, name='reading_thread')
	# t.setDaemon(True)
	t.start()

#start the writing thread
def run_writing_thread():
	t = threading.Thread(target=writing_thread, name='writing_thread')
	# t.setDaemon(True)
	t.start()

#connect the socket
def tcp_client():
	# create and connect socket with server ip and port 
	# also get user name.
	ip = '127.0.0.1'
	serverPort = 8000
	clientSocket.connect((ip, serverPort))
	mes = "\\X23HJkd90FB8djsIJ "
	clientSocket.send(mes.encode())
	print(clientSocket.recv(1024).decode())
	
	return clientSocket

if __name__ == "__main__":
	clientSocket =  tcp_client()
	run_reading_thread()
	run_writing_thread()




