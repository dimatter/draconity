from common import call
import sys
print call({'cmd': 'g.list.set', 'name': 'test', 'list': sys.argv[1], 'items': sys.argv[2:]})
