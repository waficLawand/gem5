import networkx as nx
import matplotlib.pyplot as plt
import re
import pydot
from collections import deque
from IPython.display import Image

asm = '''main:
    addi    sp,sp,-64
    j       2
.L2:
    lw      a4,44(sp)
    li      a5,9
    bne     a4,a5,2
    sw      zero,44(sp)
    jal     8
    j   0
.sum:
    add     r1,r2,r1
    sub     r1,r2,r3
    xori    r0,r1,r2
    jr      ra'''

cfg = nx.DiGraph()

graph = pydot.Dot(graph_type='digraph')


lines = asm.split('\n')
print(lines)
line_number = 0
insn_number = 0

label_to_node = {}  # dictionary to map instruction labels to node IDs

# Stack to keep track of function calls 
stack = deque()

while line_number < len(lines):
    line = lines[line_number].strip()

    if not line or line.startswith('#'):
        line_number+=1
        continue

    if line.endswith(':'):
        line_number+=1
        continue
 


    
    instr, operands = re.split(r'\s+',line,maxsplit=1)
    operands = operands.split(',')

    node = pydot.Node(insn_number,label=instr)
    graph.add_node(node)

    if instr == 'j':
        edge= pydot.Edge(insn_number,operands[0])
        graph.add_edge(edge)
    elif instr.startswith('b'):
        edge1 = pydot.Edge(insn_number,operands[2])
        edge2 = pydot.Edge(insn_number,insn_number+1)
        graph.add_edge(edge1)
        graph.add_edge(edge2)

    elif instr == 'jal':
        stack.append(insn_number+1)
        edge= pydot.Edge(insn_number,operands[0])
        graph.add_edge(edge)

    elif instr == 'jr':
        print(stack)
        edge= pydot.Edge(insn_number,stack.pop())
        graph.add_edge(edge)

    else:
        edge= pydot.Edge(insn_number,insn_number+1)
        graph.add_edge(edge)

    insn_number+=1
    line_number+=1

graph.write_png('cfg.png')

# Display the graph
Image(filename='cfg.png')

"""
G=cfg
pos = nx.spring_layout(G)
nx.draw_networkx_nodes(G, pos)
nx.draw_networkx_edges(G, pos)
nx.draw_networkx_labels(G, pos)
plt.show()
print(insn_number)
"""
