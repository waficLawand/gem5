import networkx as nx
import matplotlib.pyplot as plt
import re
import pydot
from collections import deque
from IPython.display import Image
import time 
from collections import Counter
import sys

# File containing instructions trace
insns_file = './bubble-sort.txt'


# List including all conditional branch instructions
branch_insns = ['c_bnez','bgeu','beq','c_beqz','bge','blt','bltu','bne']


cfg = nx.DiGraph()


with open(insns_file) as f:
    for line in f:
        tokens = line.split(':')
        array_of_tokens = [token.strip() for token in tokens]
        if(len(array_of_tokens) == 7 or len(array_of_tokens) == 5):
            
            split_pc = array_of_tokens[3].split(' ')
            split_insn = array_of_tokens[4].split(' ')
            if(len(array_of_tokens)==7):
                array_of_tokens = [array_of_tokens[0],array_of_tokens[1],array_of_tokens[2],split_pc[0],split_pc[1],split_insn[0],array_of_tokens[4],array_of_tokens[5],array_of_tokens[6]]
            else:
                array_of_tokens = [array_of_tokens[0],array_of_tokens[1],array_of_tokens[2],split_pc[0],split_pc[1],split_insn[0],array_of_tokens[4]]


            #print(array_of_tokens)

            if array_of_tokens[5] in branch_insns:
                node = array_of_tokens[3]
                print(array_of_tokens[5])
                cfg.add_node(node)
