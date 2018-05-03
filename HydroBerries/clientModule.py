#libraries
import socket
import time
import random

module_number = "B"
serverIP = "10.21.141.2"
serverPort = 8080

clientIP = "127.0.0.1"
clientPort = 8088



#send data to arduino via UDP
def sendData():

    tempVal = random.uniform(77,85)
    ecVal = random.uniform(1500,2000)
    doVal = random.uniform(5,8)
    phVal = random.uniform(5,7)
    level = random.uniform(5,50)
    flow = random.uniform(0,0.8)

    udpBuf = "%s,%f F,1,77F,85F,%f ppm,0,1500ppm,2000ppm,%f mg/L,1,5 mg/L,8 mg/L,%f,0,5,7,%f gal.,1,5 gal.,50 gal.,%f gal/min,1,0 gal/min,0.8 gal/min," % (module_number,tempVal,ecVal,doVal,phVal,level,flow)
    
    

    ack = ""

    serverSock = socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
    

    while (ack != "ack"):
        serverSock.sendto(udpBuf, (serverIP,serverPort))
        ack, addr = serverSock.recvfrom(1024)
        time.sleep(1)

    print "received ack: ", data
    serverSock.close()
    





if __name__== "__main__":
    while True:
        sendData()
        time.sleep(300)
    
