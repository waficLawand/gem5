import networkx as nx
import matplotlib.pyplot as plt
import re
import pydot
from collections import deque
from IPython.display import Image
import time 
from collections import Counter
import sys

# File containing register values trace
reg_values_file = './coremark_out_reg.txt'
# File containing instructions trace
insns_file = './coremark_out.txt'

# Number of instructions cconsidered for analysis
considered_lines = 100

# Extract instructions from the trace file
with open(insns_file) as f:
    lines = f.readlines()

# Loop bounds dictionary
loop_bounds_dict = {}


# List holding all the jump immediate instructions
jmp_imm_insns = ['jal']

jmp_direct_insns = ['c_j','j','c_jr']

# List holding all jump register instructions
jmp_reg_insns = ['c_jr','c_jalr']

# List including all conditional branch instructions
branch_insns = ['c_bnez','bgeu','beq','c_beqz','bge','blt','bltu','bne']

mem_insns = ['ld','lw','sw','lb','sd','c_sdsp']

functions_to_consider = []


included_insns = branch_insns+jmp_imm_insns+jmp_reg_insns+mem_insns


# Place the instructions in insn_dict dictionary with the pc being the key and the instruction being the value
insns_dict = {}
all_tokens = []


start_time_file_read = time.time()

for line in lines:
    tokens = line.split(':')
    array_of_tokens = [token.strip() for token in tokens]
    if(len(array_of_tokens) == 7 or len(array_of_tokens) == 5):
        
        split_pc = array_of_tokens[3].split(' ')
        split_insn = array_of_tokens[4].split(' ')
        if(len(array_of_tokens)==7):
            array_of_tokens = [array_of_tokens[0],array_of_tokens[1],array_of_tokens[2],split_pc[0],split_pc[1],split_insn[0],array_of_tokens[4],array_of_tokens[5],array_of_tokens[6]]
        else:
            array_of_tokens = [array_of_tokens[0],array_of_tokens[1],array_of_tokens[2],split_pc[0],split_pc[1],split_insn[0],array_of_tokens[4]]
        

        all_tokens.append(array_of_tokens)

        # If the instruction was encountered more than once increment its counter
        if int(array_of_tokens[3],16) not in loop_bounds_dict:
            loop_bounds_dict[int(array_of_tokens[3],16)] = 1
        else:
            loop_bounds_dict[int(array_of_tokens[3],16)] = loop_bounds_dict[int(array_of_tokens[3],16)] + 1
        
        if int(array_of_tokens[3],16) not in insns_dict:
            #if array_of_tokens[5] in included_insns:
                insns_dict[int(array_of_tokens[3],16)] = array_of_tokens

sorted_tokens = sorted(all_tokens, key=lambda x: int(x[3],0))

print(loop_bounds_dict)
keys_with_greatest_values = sorted(loop_bounds_dict, key=loop_bounds_dict.get, reverse=True)[:4]
hex_values = [hex(val) for val in keys_with_greatest_values]
print(hex_values)

# Extract register values that correspond to the instructions
with open(reg_values_file) as f2:
    lines_regvals = f2.readlines()
reg_vals_dict = {}
# Filling reg vals dictionary
for line in lines_regvals:
    reg_val_token = line.split(':')
    if len(reg_val_token) == 4:

        if int(reg_val_token[0]) in reg_vals_dict:
            reg_vals_dict[int(reg_val_token[0])].append(reg_val_token)
        else:
            reg_vals_dict[int(reg_val_token[0])] = [reg_val_token]

regs_dict={}
for insn in insns_dict.keys():
    if int(insns_dict[insn][0]) in reg_vals_dict.keys():
        regs_dict[insn] = reg_vals_dict[int(insns_dict[insn][0])]
    else:
        regs_dict[insn] = ''
        continue
end_time_file_read = time.time()

print("Time for file read is: "+str(end_time_file_read - start_time_file_read))


# CFG dictionary that holds a seperate cfg for every function, can be referenced using the function name
cfg_dict = {}
cfg_dict_readable = {}

start_insn_filling = time.time()
# Creating a CFG dictionary for function calls
cfg = nx.DiGraph()
cfg_dict['start'] = cfg

cfg_readable = pydot.Dot(graph_type='digraph')
cfg_dict_readable['start'] = cfg


pc=[]
insn=[]
jmp_imm=[]
branch_imm=[]
mem_addr=[]
mem_data=[]
mem_read_write=[]

unique_insns = set([token[5] for token in all_tokens])

sorted_keys = sorted(insns_dict.keys())

with open('test.txt','w') as f:
    for key in sorted_keys:
        f.write(str(insns_dict[key])+"\n")
    
    f.close()
for i,key in enumerate(sorted_keys):

    #if insns_dict[key][5] not in included_insns:
     #   continue

    #if i == considered_lines:
     #   break

    # Extract program counter value from the insn dictionary
    pc.append(int(insns_dict[key][3],16))

    # Extract insn name from the insn directory
    insn.append(insns_dict[key][5])

    # Check if the insn is a jump with immediate value
    if insn[i] in ['c_j','jal','j']:
        if insn[i] == 'jal': 
            # Extract immediate value from jal insn
            jmp_imm.append(int(insns_dict[key][6].split(' ')[2]))

        elif insn[i] == 'c_j' or insn[i] =='j':
            # Extract immediate value from j insn
            jmp_imm.append(int(insns_dict[key][6].split(' ')[1]))
    elif insn[i] in jmp_reg_insns:
        # Extract value of registers in jalr and jr
        jmp_imm.append(int(regs_dict[key][0][3].split(' ')[7].split('.\n')[0],16))
    else:
        jmp_imm.append('')


    if insn[i] in branch_insns:
        # Extract immediate value from the branch insn
        if insn[i] == 'c_bnez' or insn[i] == 'c_beqz':
            branch_imm.append(int(insns_dict[key][6].split(', ')[1]))
        else:
            branch_imm.append(int(insns_dict[key][6].split(', ')[2]))
    else:
        branch_imm.append('')

    if len(insns_dict[key]) == 9:
        if(insns_dict[key][7] == "MemRead" or insns_dict[key][7] == "MemWrite"):
            # Extract memory addresses and data for each block
            mem_addr.append(insns_dict[key][8].split(' ')[1].split('A=')[1])
            mem_data.append(insns_dict[key][8].split(' ')[0].split('D=')[1])
            mem_read_write.append(insns_dict[key][7])
        else:
            mem_addr.append('')
            mem_data.append('')
            mem_read_write.append('')
    else:
        mem_addr.append('')
        mem_data.append('')
        mem_read_write.append('')

end_insn_filling = time.time()

print("Instruction filling: "+str(end_insn_filling-start_insn_filling))

start_cfg_construction = time.time()
for i, key in enumerate(sorted_keys):
    # if i == considered_lines:
    # break

    if i != 0:
        # This condition checks for new functions
        if insns_dict[key][4].split('+')[0] != insns_dict[pc[i-1]][4].split('+')[0]:
            cfg = nx.DiGraph()
            cfg_dict[str(insns_dict[key][4].split('+')[0])] = cfg

            cfg_readable = pydot.Dot(graph_type='digraph')
            cfg_dict_readable[str(insns_dict[key][4].split('+')[0])] = cfg_readable

    if insn[i] in branch_insns:
        node = hex(pc[i])
        cfg.add_node(node)
        
        #Readable Version
        node = pydot.Node(hex(pc[i]),label=str("pc: "+hex(pc[i])+ " insn: "+insn[i]))
        
        cfg_readable.add_node(node)


        # Add branch edge, other edges will be added by nonbranch nodes
        if pc[i] + branch_imm[i] != pc[i + 1]:
            if pc[i] + branch_imm[i] in insns_dict.keys():
                edge = (hex(pc[i]), hex(pc[i] + branch_imm[i]))
                cfg.add_edge(*edge)
                edge = pydot.Edge(hex(pc[i]),hex(pc[i]+branch_imm[i]))
                cfg_readable.add_edge(edge)

    elif insn[i] in jmp_direct_insns:
        node = hex(pc[i])
        cfg.add_node(node)
        #Readable Version
        node = pydot.Node(hex(pc[i]),label=str("pc: "+hex(pc[i])+ " insn: "+insn[i]))
        cfg_readable.add_node(node)

        
        print(jmp_imm[i], pc[i])
        if pc[i] + jmp_imm[i] in insns_dict.keys():

            edge = pydot.Edge(hex(pc[i]),hex(pc[i]+jmp_imm[i]))
            cfg_readable.add_edge(edge)  
            
            
            edge = (hex(pc[i]), hex(pc[i] + jmp_imm[i]))
            cfg.add_edge(*edge)

    elif insn[i] in jmp_reg_insns or insn[i] in jmp_imm_insns:
        node = hex(pc[i])
        cfg.add_node(node)

        node = pydot.Node(hex(pc[i]),label=str("pc: "+hex(pc[i])+ " insn: "+insn[i]))
        cfg_readable.add_node(node)

    elif insn[i] in mem_insns:
        node = hex(pc[i])
        cfg.add_node(node)

        node = pydot.Node(hex(pc[i]),label=str("pc: "+hex(pc[i])+ " insn: "+insn[i]+ "address: "+mem_addr[i]))
        cfg_readable.add_node(node)

    else:
        node = hex(pc[i])
        cfg.add_node(node)

        node = pydot.Node(hex(pc[i]),label=str("pc: "+hex(pc[i])+ " insn: "+insn[i]))
        cfg_readable.add_node(node)

    if i != 0:
        if insns_dict[key][4].split('+')[0] == insns_dict[pc[i - 1]][4].split('+')[0]:
            if insn[i - 1] not in jmp_direct_insns:
                edge = (hex(pc[i - 1]), hex(pc[i]))
                cfg.add_edge(*edge)

                edge_readable = pydot.Edge(hex(pc[i-1]),hex(pc[i]))
                cfg_readable.add_edge(edge_readable)

end_cfg_construction = time.time()
#print(insns_dict)
print("CFG construction time: "+str(end_cfg_construction-start_cfg_construction))
#cfg_dict['@fillArray'].write_pdf('cfg1.pdf')

#print("CFG construction time: "+str(end_cfg_construction-start_cfg_construction))

cfg_dict_readable['@core_bench_list'].write_pdf('coremark_bench_list.pdf')

def find_loop_entries(graph):
    loop_dict = {}

    def detect_loop(node, visited, recursion_stack):
        visited.add(node)
        recursion_stack.add(node)

        for adjacent_node in graph.neighbors(node):
            if adjacent_node not in visited:
                if detect_loop(adjacent_node, visited, recursion_stack):
                    if adjacent_node in loop_dict:
                        loop_dict[adjacent_node].append(node)
                    else:
                        loop_dict[adjacent_node] = [node]
            elif adjacent_node in recursion_stack:
                if adjacent_node in loop_dict:
                    loop_dict[adjacent_node].append(node)
                else:
                    loop_dict[adjacent_node] = [node]

        recursion_stack.remove(node)

    visited = set()
    recursion_stack = set()

    for node in graph.nodes:
        if node not in visited:
            detect_loop(node, visited, recursion_stack)

    return loop_dict

dict_test = find_loop_entries(cfg_dict['@core_bench_list'])
print("DICT TEST: ",dict_test)



def has_valid_path(cfg,start_node, target_node, forbidden_list):
    visited = set()

    def dfs(node):
        visited.add(node)

        if node == target_node:
            return True

        for neighbor in cfg.neighbors(node):
            if neighbor not in forbidden_list and neighbor not in visited:
                if dfs(neighbor):
                    return True

        return False

    return dfs(start_node)

new_list = [key for key in dict_test.keys() if key != '0x1067c']
print(new_list)
# It must pass through the last element of the loop before looping back so if the path does not have the last node it should return a false
#print("TEST IMPORTANT!: ",has_valid_path(cfg_dict['@main'],'0x106ba','0x106a8',[]))

class Region:
    def __init__(self, start_node,region_type):
        self.start_node = start_node
        self.region_type = region_type
        self.end_node = None
        self.nodes = []
        self.loop_bound = 1
        self.nested_regions = []
        self.memory_nodes = []
        self.parent = None
        

class Loop:
    def __init__(start_node):
        self.start_node = start_node
        self.nested_loops = []
        self.exit_node = None




#valid_path_1 = has_valid_path(cfg_dict['@main'],'0x10640','branch_test',['branch_test'])

#print("IS IT VALID: ", valid_path_1)
def find_loop_exits(cfg,loop_exits_dict,loop_entries):
    visited = set()
    loop_stack = []

    def traverse(node,loop_exits_dict,loop_entries):
        print(node)
        if node in visited:
            return

        visited.add(node)

        pred_len = len(list(cfg.predecessors(node))) 
        succ_len = len(list(cfg.successors(node)))


        curr_node = node
        basic_block_nodes = []
        start = curr_node



        while succ_len < 2 and pred_len < 2:
        
            visited.add(curr_node)

            if(succ_len == 0):
                return
        
            curr_node = list(cfg.successors(curr_node))[0]        
        
            pred_len = len(list(cfg.predecessors(curr_node))) 
            succ_len = len(list(cfg.successors(curr_node)))
            

        if pred_len >= 2:
            if curr_node not in visited:
                visited.add(curr_node)
            else:
                return

            if(curr_node in loop_entries):
                if(len(loop_stack)>0):
                    print("CURRENT NODE: ",curr_node)
                    loop_exits_dict[curr_node] = {'exit':'','parent':loop_stack[-1]}
                else:
                    loop_exits_dict[curr_node] = {'exit':'','parent':'start'}
                
                loop_stack.append(curr_node)

            curr_node = list(cfg.successors(curr_node))[0]  
            traverse(curr_node,loop_exits_dict,loop_entries)

        if succ_len == 2:

            if len(loop_stack)>0:
                #print(loop_entries.keys())
                #print(visited)
                right_child = list(cfg.successors(curr_node))[0]  
                left_child = list(cfg.successors(curr_node))[1]
                
                for loopback_path in loop_entries[loop_stack[-1]]:
                   
                    forbidden_list = [key for key in loop_entries.keys() if key in visited and key != loop_stack[-1]]
                
                    print("Top stack is: ",loop_stack[-1])
                    print("Forbidden list: ",forbidden_list)
                    print("Visited: ",visited)


                    valid_path_1 = has_valid_path(cfg,right_child,loopback_path,[left_child])
                    valid_path_2 = has_valid_path(cfg,left_child,loopback_path,[right_child])

                    print("Right child: ",right_child)
                    print("Left Child: ",left_child)
                    print("Dictionary value: ",loopback_path)
                    print("Current branch node is: ",curr_node)
                    print("Right child valid path: ",valid_path_1)
                    print("Left Child Valid path: ",valid_path_2,"\n")

                    if not valid_path_1:
                        #loop_exits_dict[loop_stack[-1]] = {'exit':right_child,'parent':''}
                        if loop_exits_dict[loop_stack[-1]]['exit'] == '':
                            loop_exits_dict[loop_stack[-1]]['exit'] = [right_child]
                        elif right_child not in loop_exits_dict[loop_stack[-1]]['exit']:
                            print("APPEND RIGHT CHILD: ",right_child)
                            loop_exits_dict[loop_stack[-1]]['exit'].append(right_child)

                    elif not valid_path_2:
                        #loop_exits_dict[loop_stack[-1]] = {'exit':left_child, 'parent': ''}
                        if loop_exits_dict[loop_stack[-1]]['exit'] == '':
                            loop_exits_dict[loop_stack[-1]]['exit'] = [left_child]
                        elif left_child not in loop_exits_dict[loop_stack[-1]]['exit']:
                            print("APPEND LEFT CHILD: ",left_child)
                            loop_exits_dict[loop_stack[-1]]['exit'].append(left_child)
                    


                    print(loop_exits_dict)

                traverse(right_child,loop_exits_dict,loop_entries)
                traverse(left_child,loop_exits_dict,loop_entries)
            
    traverse(list(cfg.nodes())[0],loop_exits_dict,loop_entries)



def find_loop_exits_2(cfg,loop_exits_dict,loop_entries):
    visited = set()
    loop_stack = []

    def traverse(node,loop_exits_dict,loop_entries,loopback_path,loop_entry):
        print(node)

        #if node == loopback_path:
           #return

        if node in visited:

            return

        visited.add(node)

        pred_len = len(list(cfg.predecessors(node))) 
        succ_len = len(list(cfg.successors(node)))


        curr_node = node
        basic_block_nodes = []
        start = curr_node

        while succ_len < 2 and pred_len < 2:
        
            visited.add(curr_node)

            if(succ_len == 0):
                return
        
            curr_node = list(cfg.successors(curr_node))[0]        
        
            pred_len = len(list(cfg.predecessors(curr_node))) 
            succ_len = len(list(cfg.successors(curr_node)))
            
        pred_len = len(list(cfg.predecessors(curr_node))) 
        succ_len = len(list(cfg.successors(curr_node)))
        
        if pred_len >= 2:
            #loop_stack.append(loop_exits_dict[curr_node])
            if curr_node not in visited:
                visited.add(curr_node)

            if loop_entry not in loop_exits_dict:
                print("CURRENT NODE: ",curr_node)
                loop_exits_dict[loop_entry] = {'exit':''}
                
            loop_stack.append(curr_node)

            curr_node = list(cfg.successors(curr_node))[0]
            
            traverse(curr_node,loop_exits_dict,loop_entries,loopback_path,loop_entry)
            

        if succ_len == 2:
            print("CURR NODE ERROR!: ",curr_node)
            print("Actual SUCCESSOR SIZE!: ",succ_len)
            print("ERROR: ",len(list(cfg.successors(curr_node))))
            right_child = list(cfg.successors(curr_node))[0]  
            left_child = list(cfg.successors(curr_node))[1]
            
            
            #valid_path_1 = has_valid_path(cfg,right_child,loopback_path,[left_child])
            #valid_path_2 = has_valid_path(cfg,left_child,loopback_path,[right_child])

            valid_path_1 = has_valid_path(cfg,right_child,loopback_path,visited)
            valid_path_2 = has_valid_path(cfg,left_child,loopback_path,visited)

            # Most recently visited loop
            valid_path_topmost_1 = has_valid_path(cfg,right_child,loop_stack[-1],visited)
            valid_path_topmost_2 = has_valid_path(cfg,left_child,loop_stack[-1],visited)
            print("Current Node:",curr_node)
            print("Right child: ",right_child)
            print("Left Child: ",left_child)
            print("Right child valid path: ",valid_path_1)
            print("Left Child Valid path: ",valid_path_2,"\n")

            print("PRINTING TEST: ",right_child,loop_entry)
            if not valid_path_1 and (right_child != loop_entry and left_child != loop_entry):
            #if not valid_path_1:
                #loop_exits_dict[loop_stack[-1]] = {'exit':right_child,'parent':''}
                print("ENTERING NOTVALID 1")
                if loop_exits_dict[loop_entry]['exit'] == '':
                    loop_exits_dict[loop_entry]['exit'] = [right_child]
                elif right_child not in loop_exits_dict[loop_entry]['exit'] and valid_path_topmost_1:
                    print("APPEND RIGHT CHILD: ",right_child)
                    loop_exits_dict[loop_entry]['exit'].append(right_child)
                #return
                traverse(left_child,loop_exits_dict,loop_entries,loopback_path,loop_entry)
            
            elif not valid_path_2 and (left_child != loop_entry and right_child != loop_entry):
                print("ENTERING NOTVALID 2")
            #elif not valid_path_2:
                #loop_exits_dict[loop_stack[-1]] = {'exit':left_child, 'parent': ''}
                if loop_exits_dict[loop_entry]['exit'] == '':
                    loop_exits_dict[loop_entry]['exit'] = [left_child]
                elif left_child not in loop_exits_dict[loop_entry]['exit'] and valid_path_topmost_2:
                    print("APPEND LEFT CHILD: ",left_child)
                    loop_exits_dict[loop_entry]['exit'].append(left_child)
                #return
                traverse(right_child,loop_exits_dict,loop_entries,loopback_path,loop_entry)
            
            elif right_child == loop_entry:
                print("CONDITION RIGHT CHILD REACHED!!")
                if loop_exits_dict[loop_entry]['exit'] == '':
                    loop_exits_dict[loop_entry]['exit'] = [left_child]
                elif left_child not in loop_exits_dict[loop_entry]['exit'] and valid_path_topmost_2:
                    print("APPEND LEFT CHILD: ",left_child)
                    loop_exits_dict[loop_entry]['exit'].append(left_child)
                return

            elif left_child == loop_entry:
                print("CONDITION LEFT CHILD REACHED!!")
                if loop_exits_dict[loop_entry]['exit'] == '':
                    loop_exits_dict[loop_entry]['exit'] = [right_child]
                elif right_child not in loop_exits_dict[loop_entry]['exit'] and valid_path_topmost_1:
                    print("APPEND RIGHT CHILD: ",right_child)
                    loop_exits_dict[loop_entry]['exit'].append(right_child)
                return

            else:
        
                print("EXIT DICT: ",loop_exits_dict)
                traverse(right_child,loop_exits_dict,loop_entries,loopback_path,loop_entry)
                traverse(left_child,loop_exits_dict,loop_entries,loopback_path,loop_entry)


    for loop in loop_entries:
        print("CURRENT LOOP IS: ",loop)
        for loopback_path in loop_entries[loop]:
            print("CURRENT LOOPBACK PATH IS: ",loopback_path)
            visited = set()
            traverse(loop,loop_exits_dict,loop_entries,loopback_path,loop)



loop_exits_dict = {}

find_loop_exits_2(cfg_dict['@core_bench_list'],loop_exits_dict,find_loop_entries(cfg_dict['@core_bench_list']))

print("Exits Dictionary: ",loop_exits_dict)

def find_loop_path(graph, start_node, end_node):
    # Check if the end node is connected to the start node
    print(graph[start_node])
    if end_node not in graph.predecessors(start_node):
        return None

    # Depth-first search to find a loop path
    visited = set()
    stack = [(start_node, [start_node])]

    while stack:
        current_node, path = stack.pop()

        # If we reach the end node and it's connected to the start node, we have a loop path
        if current_node == end_node:
            return path

        # Mark the current node as visited
        visited.add(current_node)

        # Explore neighbors
        for neighbor in graph.successors(current_node):
            if neighbor not in visited:
                stack.append((neighbor, path + [neighbor]))

    return None  # No loop path found


#path = find_loop_path(cfg_dict['@manipulateArray'],'0x10660','0x1065c')
#print("PATH IS: ",path)

def detect_loops(cfg):
    
    loop_stack = []
    visited = set()
    loop_dict = {}


def create_regions(cfg,loop_exits_dict):
    visited = set()
    top_stack = []
    curr_region = Region("start","")
    top_stack.append(curr_region)

    def traverse(node,curr_region,loop_exits_dict):

        print("==============================================STACK==========================================================")
        
        for elements in top_stack:
            print(elements.start_node)

        print("TOP NODE IS: ",top_stack[-1].start_node)
        print("==============================================END STACK======================================================")

        if node in visited:
            temp = top_stack.pop()
            top_stack[-1].nested_regions.append(temp)
            return

        visited.add(node)

        pred_len = len(list(cfg.predecessors(node))) 
        succ_len = len(list(cfg.successors(node)))


        curr_node = node
        basic_block_nodes = []
        start = curr_node



        while succ_len < 2 and pred_len < 2:
            
            visited.add(curr_node)
            basic_block_nodes.append(curr_node)

            if(succ_len == 0):
                basic_region = Region(start,'')
                basic_region.nodes = basic_block_nodes
                print("START: ",start)
                #top_stack[-1].nested_regions.append(basic_region)
                curr_region.nodes = basic_block_nodes
                return
            else:
                curr_node = list(cfg.successors(curr_node))[0]        
            
            pred_len = len(list(cfg.predecessors(curr_node))) 
            succ_len = len(list(cfg.successors(curr_node)))

            
#            print(curr_node)

        curr_region.nodes = basic_block_nodes
        #basic_region = Region(start,'')
        #basic_region.nodes = basic_block_nodes
        #print("START: ",start)
        #top_stack[-1].nested_regions.append(basic_region)
        #top_stack[-1].nested_regions.append(curr_region)


        if pred_len >= 2:
            print("Current Node: ",curr_node)
            print("Visited Nodes: ",visited)
            if curr_node not in visited:
                new_region = Region(curr_node,"")
                new_region.nodes.append(curr_node)
            
                top_stack.append(new_region)
                visited.add(curr_node)
            else:
                temp = top_stack.pop()
                top_stack[-1].nested_regions.append(temp)
                return
            
            curr_node = list(cfg.successors(curr_node))[0] 
            traverse(curr_node,new_region,loop_exits_dict)


            #print("AFTER FINISHING FROM TRAVERSAL: ",top_stack[-1].nested_regions[0].start_node)
            
            #temp = top_stack.pop()
            #top_stack[-1].nested_regions.append(temp)

        if succ_len == 2:
            node_1 = list(cfg.successors(curr_node))[0]
            node_2 = list(cfg.successors(curr_node))[1]

            top_region = Region(curr_node,'')
            top_stack.append(top_region)

            child_region_1 = Region(node_1,'')
            child_region_2 = Region(node_2,'')

            
            #top_region.nested_regions.append(child_region_1)
            #top_region.nested_regions.append(child_region_2)
                
            #traverse(node_1,child_region_1)
            
            top_stack.append(child_region_1)

            traverse(node_1,child_region_1,loop_exits_dict)
            #top_region.nested_regions.append(child_region_1)

            temp = top_stack.pop()
            top_stack[-1].nested_regions.append(temp)
            
            #top_stack[-1].nested_regions.append(child_region_1)
            #top_stack[-1].nested_regions.append(child_region_1)
            
            
            top_stack.append(child_region_2)

            traverse(node_2,child_region_2,loop_exits_dict)
            #top_region.nested_regions.append(child_region_2)

            temp = top_stack.pop()
            top_stack[-1].nested_regions.append(temp)

            print("TOP REGION: ",top_region.start_node)
            for region in top_region.nested_regions:
                print(region.start_node)
            print("======================================")

            temp = top_stack.pop()
            top_stack[-1].nested_regions.append(temp)

            #top_region.nested_regions.append(child_region_1)
            #top_region.nested_regions.append(child_region_2)
            
            #top_stack[-1].nested_regions.append(child_region_2)
            #top_stack[-1].nested_regions.append(child_region_2)

            #top_region.nested_regions.append(child_region_1)
            #top_region.nested_regions.append(child_region_2)

            #temp = top_stack.pop()

            #top_stack[-1].nested_regions.append(temp)

    traverse(list(cfg.nodes())[0],curr_region)


    print("STACK TOP: ",top_stack[-1].start_node)

    for region in top_stack[-1].nested_regions:
        print(region.start_node)
        print("WE ARE HERE!")




def create_regions_2(cfg,loop_exits_dict):
    visited = set()
    top_stack = []
    curr_region = Region("start","")
    top_stack.append(curr_region)

    def traverse(node,curr_region,loop_exits_dict):
        print("==============================================STACK==========================================================")
        for elements in top_stack:
            print(elements.start_node)
        print("TOP NODE IS: ",top_stack[-1].start_node)
        print("==============================================END STACK======================================================")
        


        if node in visited:
            temp = top_stack.pop()
            top_stack[-1].nested_regions.append(temp)
            return

        visited.add(node)

        pred_len = len(list(cfg.predecessors(node))) 
        succ_len = len(list(cfg.successors(node)))


        curr_node = node
        basic_block_nodes = []
        start = curr_node



        while succ_len < 2 and pred_len < 2:
            
            visited.add(curr_node)
            basic_block_nodes.append(curr_node)

            if(succ_len == 0):
                basic_region = Region(start,'')
                basic_region.nodes = basic_block_nodes
                print("START: ",start)
                #top_stack[-1].nested_regions.append(basic_region)
                curr_region.nodes = basic_block_nodes
                #temp = top_stack.pop()
                #top_stack[-1].nested_regions.append(temp)
                return
            else:
                curr_node = list(cfg.successors(curr_node))[0]        
            
            pred_len = len(list(cfg.predecessors(curr_node))) 
            succ_len = len(list(cfg.successors(curr_node)))

            
#            print(curr_node)

        #curr_region.nodes = basic_block_nodes
        basic_region = Region(start,'')
        basic_region.nodes = basic_block_nodes
        #print("START: ",start)
        top_stack[-1].nested_regions.append(basic_region)
        #top_stack[-1].nested_regions.append(curr_region)


        if pred_len >= 2:
            print("Current Node: ",curr_node)
            print("Visited Nodes: ",visited)
            if curr_node not in visited:
                new_region = Region(curr_node,"")
                new_region.nodes.append(curr_node)
            
                top_stack.append(new_region)
                visited.add(curr_node)
            else:
                #temp = top_stack.pop()
                #top_stack[-1].nested_regions.append(temp)
                return
            
            curr_node = list(cfg.successors(curr_node))[0] 
            loop_region = Region(curr_node,'')
            traverse(curr_node,new_region,loop_exits_dict)


            #print("AFTER FINISHING FROM TRAVERSAL: ",top_stack[-1].nested_regions[0].start_node)
            
            #temp = top_stack.pop()
            #top_stack[-1].nested_regions.append(temp)

        if succ_len == 2:
            node_1 = list(cfg.successors(curr_node))[0]
            node_2 = list(cfg.successors(curr_node))[1]

            print(list(loop_exits_dict.keys()))
            print(top_stack[-1])
            if top_stack[-1].start_node in list(loop_exits_dict.keys()):
                print("IM HERE1")
                if node_2 in loop_exits_dict[top_stack[-1].start_node]['exit']:
                    
                    child_region_1 = Region(node_1,'')

                    top_stack.append(child_region_1)
                    traverse(node_1,child_region_1,loop_exits_dict)

                    print("==============================================STACK==========================================================")
                    for elements in top_stack:
                        print(elements.start_node)
                    print("TOP NODE IS: ",top_stack[-1].start_node)
                    print("==============================================END STACK======================================================")
                    
                    # Pop loop body
                    temp = top_stack.pop()
                    top_stack[-1].nested_regions.append(temp)
                    
                    # Pop the whole loop
                    temp = top_stack.pop()
                    top_stack[-1].nested_regions.append(temp)

                    # Traverse the exit portion
                    child_region_2 = Region(node_2,'')
                    top_stack.append(child_region_2)
                    traverse(node_2,child_region_2,loop_exits_dict)
                    
                    temp = top_stack.pop()
                    top_stack[-1].nested_regions.append(temp)

                elif node_1 in loop_exits_dict[top_stack[-1].start_node]['exit']:
                    print("IM HERE2")
                    child_region_1 = Region(node_2,'')

                    top_stack.append(child_region_1)
                    traverse(node_2,child_region_1,loop_exits_dict)

                    print("==============================================STACK==========================================================")
                    for elements in top_stack:
                        print(elements.start_node)
                    print("TOP NODE IS: ",top_stack[-1].start_node)
                    print("==============================================END STACK======================================================")
                    
                    # Pop loop body
                    temp = top_stack.pop()
                    top_stack[-1].nested_regions.append(temp)
                    
                    # Pop the whole loop
                    temp = top_stack.pop()
                    top_stack[-1].nested_loops.append(temp)

                    # Traverse the exit portion
                    child_region_2 = Region(node_1,'')
                    top_stack.append(child_region_2)
                    traverse(node_1,child_region_2,loop_exits_dict)
                    
                    temp = top_stack.pop()
                    top_stack[-1].nested_regions.append(temp)
                else:
                    child_region_1 = Region(node_1,'')
                    
                    top_stack.append(child_region_1)
                    traverse(node_1,child_region_1,loop_exits_dict)
                    temp = top_stack.pop()
                    top_stack[-1].nested_regions.append(temp)
                    
                    child_region_2 = Region(node_2,'')
                    top_stack.append(child_region_2)
                    traverse(node_2,child_region_2,loop_exits_dict)
                    temp = top_stack.pop()
                    top_stack[-1].nested_regions.append(temp)


            else:
                child_region_1 = Region(node_1,'')

                
                top_stack.append(child_region_1)
                traverse(node_1,child_region_1,loop_exits_dict)
                temp = top_stack.pop()
                print("TESTING: ",temp.start_node)
                top_stack[-1].nested_regions.append(temp)

                child_region_2 = Region(node_2,'')
                top_stack.append(child_region_2)
                traverse(node_2,child_region_2,loop_exits_dict)
                temp = top_stack.pop()
                top_stack[-1].nested_regions.append(temp)




    traverse(list(cfg.nodes())[0],curr_region,loop_exits_dict)
    print("STACK TOP: ",top_stack[-1].start_node)



#    for region in top_stack[-1].nested_regions[1].nested_regions[1].nested_regions[1].nested_regions[1].nested_regions[2].nested_regions[0].nodes:
 #       print(region)

    
create_regions_2(cfg_dict['@core_bench_list'],loop_exits_dict)
    

print("dsdsdsdsd")