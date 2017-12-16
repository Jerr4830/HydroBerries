#import the socket library
from socket import*
import time

address = ('129.21.141.23', 12345)
# next create a socket object
s = socket(AF_INET, SOCK_DGRAM)





# next bind to the port
# Empty string in the ip field
# makes the server listen to requests
# comming from other computers on the network
#s.connect(('129.21.141.23',port))

#send data from the server
while (1):
    data = "Module: B\nSending sensor data"
    s.sendto(data,address)
    time.sleep(30)
    


