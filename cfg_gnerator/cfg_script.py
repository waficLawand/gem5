import networkx as nx
import matplotlib.pyplot as plt
import re
import pydot
from collections import deque
from IPython.display import Image


with open('./tracefile.txt') as f:
    lines = f.readlines()

insns_dict = {}

#print(lines[1])
all_tokens = []

for line in lines:
    tokens = line.split(':')

    
    array_of_tokens = [token.strip() for token in tokens]
    #print(array_of_tokens)


    if(len(array_of_tokens) == 7 or len(array_of_tokens) == 5):
        
        split_pc = array_of_tokens[3].split(' ')
        split_insn = array_of_tokens[4].split(' ')
        if(len(array_of_tokens)==7):
            array_of_tokens = [array_of_tokens[0],array_of_tokens[1],array_of_tokens[2],split_pc[0],split_pc[1],split_insn[0],array_of_tokens[4],array_of_tokens[5],array_of_tokens[6]]
        else:
            array_of_tokens = [array_of_tokens[0],array_of_tokens[1],array_of_tokens[2],split_pc[0],split_pc[1],split_insn[0],array_of_tokens[4]]
        

        all_tokens.append(array_of_tokens)
        
        if int(array_of_tokens[3],16) not in insns_dict:
            insns_dict[int(array_of_tokens[3],16)] = array_of_tokens

    else:
        print("linelinelinelinelinelinelineline")

sorted_tokens = sorted(all_tokens, key=lambda x: int(x[3],0))

#print(all_tokens[0])
#print(len(insns_dict.keys()))

#print(sorted_tokens[38])

with open('./tracefile_regvals.txt') as f2:
    lines_regvals = f2.readlines()

reg_vals_dict = {}

for line in lines_regvals:
    reg_val_token = line.split(':')
    #print(reg_val_token)
    if len(reg_val_token) == 4:

        if int(reg_val_token[0]) in reg_vals_dict:
            reg_vals_dict[int(reg_val_token[0])].append(reg_val_token)
        else:
            reg_vals_dict[int(reg_val_token[0])] = [reg_val_token]
            #else:
                #print(reg_val_token[0])

    #else:
        #print(line)


#print(insns_dict[0x102a4])

regs_dict={}



for insn in insns_dict.keys():
    if int(insns_dict[insn][0]) in reg_vals_dict.keys():
        regs_dict[insn] = reg_vals_dict[int(insns_dict[insn][0])]
    else:
        regs_dict[insn] = ''
        continue


#print(reg_vals_dict.keys())
print(len(regs_dict.keys()))
print(len(insns_dict.keys()))

sorting = sorted(all_tokens)
with open('reg_ticks.txt', 'w') as f:
    # Write lines to the file
    for token in sorted_tokens:
        f.write(str(token)+'\n')


cfg = pydot.Dot(graph_type='digraph')
call_stack = deque()

jmp_imm_insns = ['jal','c_j','j']
jmp_reg_insns = ['c_jr','c_jalr']
branch_insns = ['c_bnez','bgeu','beq','c_beqz','bge','blt','bltu','bne']

unique_insns = set([token[5] for token in all_tokens])

print(unique_insns)

for i,key in enumerate(insns_dict.keys()):

    if i == 40:
        break
    # Extract program counter value from the insn dictionary
    pc = int(insns_dict[key][3],16)

    # Extract insn name from the insn directory
    insn = insns_dict[key][5]

    # Check if the insn is a jump with immediate value
    if insn in jmp_imm_insns:
        if insn == 'jal': 
            # Extract immediate value from jal insn
            jmp_imm = int(insns_dict[key][6].split(' ')[2])

        elif insn == 'c_j' or insn =='j':
            # Extract immediate value from j insn
            jmp_imm = int(insns_dict[key][6].split(' ')[1])
    
    elif insn in jmp_reg_insns:
        # Extract value of registers in jalr and jr
        jmp_imm = int(regs_dict[key][0][3].split(' ')[7].split('.\n')[0],16)

    if insn in branch_insns:
        # Extract immediate value from the branch insn
        if insn == 'c_bnez' or insn == 'c_beqz':
            branch_imm = int(insns_dict[key][6].split(', ')[1])
        else:
            branch_imm = int(insns_dict[key][6].split(', ')[2])


    if len(insns_dict[key]) == 9:
        if(insns_dict[key][7] == "MemRead" or insns_dict[key][7] == "MemWrite"):
            # Extract memory addresses and data for each block
            mem_addr = insns_dict[key][8].split(' ')[1].split('A=')[1]
            mem_data = insns_dict[key][8].split(' ')[0].split('D=')[1]
            mem_read_write = insns_dict[key][7]
            
    
    # Create a node for every instruction
    node = pydot.Node(pc,label=pc)
    cfg.add_node(node)

    if insn in branch_insns:
        edge1 = pydot.Edge(pc,pc+branch_imm)
        edge2 = pydot.Edge(pc,int(insns_dict[sorted_pc[i+1]][3],16))
        cfg.add_edge(edge1)
        cfg.add_edge(edge2)
        



cfg.write_pdf('cfg.pdf')

# Display the graph
#Image(filename='cfg.pdf')






#print(all_tokens[38][6].split(', '))
# Create a directed graph to represent the control flow
G = nx.DiGraph()
for i, tokens in enumerate(sorted_tokens):

    if i == 50:
        break

    #G.add_node(i, instruction=tokens[5])
    G.add_node(tokens[5])
    # Add edges to the graph based on the control flow of the instructions
    if tokens[5] == 'jal':
        # Jump-and-Link (JAL) instruction: add an edge to the target and the next instruction

        target_addr = int(tokens[3], 16) + int(tokens[6].split(', ')[1])
        target_index = next((index for index, t in enumerate(all_tokens) if int(t[3], 16) == target_addr), None)
        if target_index is not None:
            G.add_edge(i, target_index)
        G.add_edge(i, i + 1)
    elif tokens[5] == 'jalr':
        # Jump-and-Link-Register (JALR) instruction: add an edge to the target and the next instruction
        target_reg = int(tokens[8], 16)
        target_index = next((index for index, t in enumerate(all_tokens) if int(t[3], 16) == target_reg), None)
        if target_index is not None:
            G.add_edge(i, target_index)
        G.add_edge(i, i + 1)
    """elif tokens[5] == 'j':
        # Jump (J) instruction: add an edge to the target
        target_addr = int(tokens[3], 16) + int(tokens[6])
        target_index = next((index for index, t in enumerate(all_tokens) if int(t[3], 16) == target_addr), None)
        if target_index is not None:
            G.add_edge(i, target_index)
    elif tokens[5] == 'beq' or tokens[5] == 'bne' or tokens[5] == 'blt' or tokens[5] == 'bge' or tokens[5] == 'bltu' or tokens[5] == 'bgeu':
        # Branch (BEQ, BNE, BLT, BGE, BLTU, BGEU) instruction: add an edge to the target and the next instruction
        target_addr = int(tokens[3], 16) + int(tokens[6])
        target_index = next((index for index, t in enumerate(all_tokens) if int(t[3], 16) == target_addr), None)
        if target_index is not None:
            G.add_edge(i, target_index)
        G.add_edge(i, i + 1)
    else:
        # Other instructions: add an edge to the next instruction
        G.add_edge(i, i + 1)"""

#pos = nx.spring_layout(G, k=0.15, seed=42) # Layout the graph using the spring

pos = nx.spring_layout(G)
nx.draw_networkx_nodes(G, pos)
nx.draw_networkx_edges(G, pos)
nx.draw_networkx_labels(G, pos)
plt.show()

#print(G)
"""unique_insns = set([token[5] for token in all_tokens])

print(unique_insns)

sorted_tokens = sorted(all_tokens, key=lambda x: int(x[3],0))


with open('sorted_tokens.txt', 'w') as f:
    # Write each subarray in the sorted_tokens list to a separate line in the file
    for tokens in sorted_tokens:
        f.write(' : '.join(tokens) + '\n')"""


#print(sorted_tokens[0])