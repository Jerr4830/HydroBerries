#import the socket library
import serial
#import the time library
import time


serialConn = serial.Serial()
serialConn.baudrate = 9600
serialConn.port = 'COM3'
serialConn.open()

data_list = []
sensorType = ''


def write_excel():
    curr_time = time.asctime(time.localtime(time.time()))
    head_tm = curr_time.split(' ')
    fileName = head_tm[0] + '_' + head_tm[1] + '_' + head_tm[2] + '_' + sensorType




if __name__ == '__main__':
    while data_list != 'end':
        while True:
            in_char = serialConn.read(1);
            if in_char == '\n':
                break
            data_list.append(in_char)
    print data_list

serialConn.close()

    






    




