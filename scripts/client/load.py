from common import call
import time
import sys
start = time.time()
print call({
    'cmd': 'g.load',
    'name': 'test',
    'public': {
        # 'main': 'computer (<lights> | <door> | <cat> | <laundry> | <camera> | <weather> | <mail> | <hvac>)',
        'lights': '(<lightaction> (lights in [the] <room> | [the] <room> [lights]) | <room> <lightaction>)',
        'doors': '(open | lock | close) ([the] <door> door | all doors)',
        'cat': 'feed [the] (cat | kitty)',
        'laundry': '(is [the] laundry done | do my laundry)',
        'camera': 'show (outside camera | camera one | camera two)',
        'hvac': '(<hvac2> in [the] <room> | <hvac2>)',
        'weather': '(what will | what\'s | what is) the weather [be] [at] <dgndictation>',
        'mail': '(has the mailman come [yet] | has mail arrived | is [the] mail here)',
    },
    'private': {
        'door': '{door}',
        'room': '{room}',
        # 'door': '(front | side | garage | back | patio | french)',
        # 'room': '(kitchen | bedroom | living room | dining room)',
        'lightaction': '(lights (on | off) | dim)',
        'hvac2': '(cool | (raise | lower) [the] (heat | temperature))',
    },
})
print call({'cmd': 'g.list.set', 'name': 'test', 'list': 'door', 'items': ['front', 'side', 'garage', 'back', 'patio', 'french']})
print call({'cmd': 'g.list.set', 'name': 'test', 'list': 'room', 'items': ['kitchen', 'bedroom', 'living room', 'dining room']})
# print call({'cmd': 'g.list.set', 'name': 'test', 'list': 'list', 'items': sys.argv[1:]})
print time.time() - start
