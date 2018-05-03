#import the socket library
import serial
#import the time library
import time
# import the socket library
from socket import*


sock_address = ('129.21.141.205', 8888)

# create socket object
sock = socket(AF_INET, SOCK_DGRAM)
UDP_IP = "129.0.0.1"
UDP_PORT = 8888
#sock.bind((UDP_IP,UDP_PORT))




data_list = []
sensorType = ''



def write_excel():
    curr_time = time.asctime(time.localtime(time.time()))
    head_tm = curr_time.split(' ')
    fileName = head_tm[0] + '_' + head_tm[1] + '_' + head_tm[2] + '_' + sensorType




if __name__ == '__main__':
    serialConn = serial.Serial('COM3',9600)
    serialConn.close()
    serialConn.open()
    while (1):         
         print 'starting'
         in_char = serialConn.readline().decode("utf-8")
         print in_char
         
         ack = ""
        
         while (ack != "received"):
             sock.sendto(in_char,sock_address)
             ack,addr = sock.recvfrom(1024)
             print ack
             time.sleep(30)
        



    






    




