import zmq
import json

def multrecv(s):
    ret = [s.recv()]
    while s.getsockopt(zmq.RCVMORE):
        ret.append(s.recv())
    return ret

context = zmq.Context()
s = context.socket(zmq.REQ)
s.connect('ipc:///tmp/ml_cmd')

def call(d):
    s.send(json.dumps(d))
    return s.recv()
