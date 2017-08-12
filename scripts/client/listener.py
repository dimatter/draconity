import zmq
import threading
import time

def multrecv(s):
    ret = [s.recv()]
    while s.getsockopt(zmq.RCVMORE):
        ret.append(s.recv())
    return ret

context = zmq.Context()

s = context.socket(zmq.SUB)
s.setsockopt(zmq.SUBSCRIBE, '')
s.setsockopt(zmq.TCP_KEEPALIVE, 1)
s.setsockopt(zmq.TCP_KEEPALIVE_IDLE, 3000)
s.setsockopt(zmq.TCP_KEEPALIVE_INTVL, 1000)
s.connect('ipc:///tmp/ml_pub')
print 'sub listening'
while True:
    print 'sub <- (%s)' % multrecv(s)
