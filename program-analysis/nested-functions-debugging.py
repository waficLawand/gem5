import networkx as nx
import matplotlib.pyplot as plt
import re
import pydot
from collections import deque
from IPython.display import Image
import time 
from collections import Counter
import sys
import math
import pulp

# File containing instructions trace
insns_file = './bubble-sort/bubble-sort-exec.txt'
mmu_file = './bubble-sort/bubble-sort-mmu.txt'


# List including all conditional branch instructions
branch_insns = ['c_bnez','bgeu','beq','c_beqz','bge','blt','bltu','bne']
jump_insns = ['c_jalr','c_j','j','jal']


#cfg = nx.DiGraph()
##cfg_readable = pydot.Dot(graph_type='digraph')

prev_branch = False
prev_jump = False

curr_pc = 0
branch_imm = 0
visited_pc = set()
added_edge = set()
cfg_dict = {}
program_insns = []
insn_accesses={}
insns_dict = {}

def calculate_set(cache_size_bytes, associativity, cache_line_size_bytes, address):
    # Convert cache size to number of sets
    num_sets = cache_size_bytes // (associativity * cache_line_size_bytes)
    
    # Calculate the number of bits for the offset and index
    offset_bits = 6  # Since the cache line size is 64 bytes
    index_bits = int(math.log2(num_sets))
    if index_bits == 0:
        set_number = 0
        return set_number 
    #("INDEX IS: ",index_bits)
    # Convert the hexadecimal address to binary
    binary_address = bin(address)[2:].zfill(32)  # Assuming 32-bit address
    
    # Extract the offset and index bits from the binary address
    offset = binary_address[-offset_bits:]
    index = binary_address[-(offset_bits + index_bits):-offset_bits]
    # Convert index bits back to integer to get the set number
    set_number = int(index, 2)
    
    return set_number

############################################################ CFG CONSTRUCTION ##########################################################################
function_calls_dict = {}

with open(insns_file) as f:
    lines = f.readlines()
    for i,line in enumerate(lines):
        tokens = line.split(':')
        array_of_tokens = [token.strip() for token in tokens]
        if(len(array_of_tokens) == 7 or len(array_of_tokens) == 5):
            
            split_pc = array_of_tokens[3].split(' ')
            split_insn = array_of_tokens[4].split(' ')
            if(len(array_of_tokens)==7):
                array_of_tokens = [array_of_tokens[0],array_of_tokens[1],array_of_tokens[2],split_pc[0],split_pc[1],split_insn[0],array_of_tokens[4],array_of_tokens[5],array_of_tokens[6]]
            else:
                array_of_tokens = [array_of_tokens[0],array_of_tokens[1],array_of_tokens[2],split_pc[0],split_pc[1],split_insn[0],array_of_tokens[4]]



            curr_function = array_of_tokens[4].split('+')[0]
            
            program_insns.append(array_of_tokens)
            #print(array_of_tokens)
            
            if (int(array_of_tokens[3],16)) not in insns_dict:
                insns_dict[array_of_tokens[3]] = array_of_tokens

            if curr_function not in cfg_dict:
                cfg_dict[curr_function] = [nx.DiGraph(),0,0,set()]
                #cfg_readable[curr_function] = [pydot.Dot(graph_type='digraph'),0,0]
            
            curr_pc = (int(array_of_tokens[3],16))
            cfg_dict[curr_function][1] = curr_pc

            if hex(curr_pc) not in insn_accesses:
                insn_accesses[hex(curr_pc)] = 1
            else:
                insn_accesses[hex(curr_pc)] += 1
            #cfg_readable[curr_function][1] = curr_pc
            
            # FIX CURR PC AND PREV PC
                    
            
            if array_of_tokens[6] in ['c_jr ra','jalr ra']:
                node = hex(cfg_dict[curr_function][1])
                cfg_dict[curr_function][0].add_node(node)

                edge = (hex(cfg_dict[curr_function][2]),hex(cfg_dict[curr_function][1]))
                cfg_dict[curr_function][0].add_edge(*edge)
                added_edge.add(str(str(cfg_dict[curr_function][1])+str(cfg_dict[curr_function][2])))

                cfg_dict[curr_function][2] = 0
                cfg_dict[curr_function][1] = 0
                continue




            elif array_of_tokens[5] in ['jal','c_jalr']:
                node = hex(cfg_dict[curr_function][1])
                cfg_dict[curr_function][0].add_node(node)
                
                #node_readable = pydot.Node(hex(cfg_dict[curr_function][1]),label=str("pc: "+hex(cfg_dict[curr_function][1])+ " insn: "))
                #cfg_readable[curr_function][0].add_node(node_readable)

                #edge_readable = pydot.Edge(hex(cfg_dict[curr_function][2]),hex(cfg_dict[curr_function][1]))
                #cfg_readable[curr_function][0].add_edge(edge_readable)

                edge = (hex(cfg_dict[curr_function][2]),hex(cfg_dict[curr_function][1]))
                cfg_dict[curr_function][0].add_edge(*edge)
                added_edge.add(str(str(cfg_dict[curr_function][1])+str(cfg_dict[curr_function][2])))

                cfg_dict[curr_function][2] = cfg_dict[curr_function][1]
                #cfg_readable[curr_function][1] = cfg_dict[curr_function][1]

                #cfg_dict[curr_function][2] = 0

                function_name = '@'+lines[i+1].split('@')[1].split(' ')[0].split('+')[0]


                #print(function_name)
                cfg_dict[curr_function][3].add(function_name)
                function_calls_dict[node] = function_name



                continue
            #print(hex(cfg_dict[curr_function][2]),hex(cfg_dict[curr_function][1]))
            


            if prev_branch:
                #edge_readable = pydot.Edge(hex(cfg_dict[curr_function][2]),hex(cfg_dict[curr_function][2]+branch_imm))
                ##cfg_readable.add_edge(edge_readable)

                edge = (hex(cfg_dict[curr_function][2]),hex(cfg_dict[curr_function][2]+branch_imm))
                #cfg.add_edge(*edge)
                #if(cfg_dict[curr_function][2]+branch_imm != cfg_dict[curr_function][1]) and int(cfg_dict[curr_function][1]+cfg_dict[curr_function][2]) not in added_edge:

                    #cfg.add_edge(edge)
                
                prev_branch = False

            #all_nodes = [node.get_name() for node in #cfg_readable.get_nodes()]

            #print(all_nodes)
            
            if array_of_tokens[5] in branch_insns:
                #print(hex(cfg_dict[curr_function][1]))
                
                prev_branch = True
                
                if array_of_tokens[5] == 'c_bnez' or array_of_tokens[5] == 'c_beqz':
                    branch_imm = (int(array_of_tokens[6].split(', ')[1]))
                else:
                    branch_imm = (int(array_of_tokens[6].split(', ')[2]))
    
                #node_readable = pydot.Node(hex(cfg_dict[curr_function][1]),label=str("pc: "+hex(cfg_dict[curr_function][1])+ " insn: "))
                #cfg_readable[curr_function][0].add_node(node_readable)

                #print(all_nodes)
                node = hex(cfg_dict[curr_function][1])
                #edge = pydot.Edge(hex(cfg_dict[curr_function][2]),hex(cfg_dict[curr_function][2]+branch_imm))
                cfg_dict[curr_function][0].add_node(node)
                

            #elif array_of_tokens[5] in jump_insns:
                #prev_jump = True
                #node = pydot.Node(hex(cfg_dict[curr_function][1]),label=str("pc: "+hex(cfg_dict[curr_function][1])+ " insn: "))
                #cfg.add_node(node)

            else:
                if cfg_dict[curr_function][1] not in visited_pc:
                    node_readable = pydot.Node(hex(cfg_dict[curr_function][1]),label=str("pc: "+hex(cfg_dict[curr_function][1])+ " insn: "))
                    #cfg_readable[curr_function][0].add_node(node_readable)

                    node = hex(cfg_dict[curr_function][1])
                    cfg_dict[curr_function][0].add_node(node)
                
            if cfg_dict[curr_function][2] != 0:
                if str(str(cfg_dict[curr_function][1])+str(cfg_dict[curr_function][2])) not in added_edge:
                    #print("WHYYYYY")
                    #print(hex(cfg_dict[curr_function][2]),hex(cfg_dict[curr_function][1]))
                    edge_readable = pydot.Edge(hex(cfg_dict[curr_function][2]),hex(cfg_dict[curr_function][1]))
                    #cfg_readable[curr_function][0].add_edge(edge_readable)

                    edge = (hex(cfg_dict[curr_function][2]),hex(cfg_dict[curr_function][1]))
                    cfg_dict[curr_function][0].add_edge(*edge)
                    added_edge.add(str(str(cfg_dict[curr_function][1])+str(cfg_dict[curr_function][2])))
                #else:
                #    print("NOT HEREEE!")

            visited_pc.add(cfg_dict[curr_function][1])
            
            prev_pc = (int(array_of_tokens[3],16))
            
            cfg_dict[curr_function][2] = prev_pc
            #cfg_readable[curr_function][2] = prev_pc
print("Length of insns dictionary is: ",len(insns_dict))

def find_loop_entries(graph):
    loop_dict = {}

    def detect_loop(node, visited, recursion_stack):
        visited.add(node)
        recursion_stack.add(node)

        for adjacent_node in graph.neighbors(node):
            if adjacent_node not in visited:
                if detect_loop(adjacent_node, visited, recursion_stack):
                    if adjacent_node in loop_dict:
                        if adjacent_node == node:
                            continue
                        loop_dict[adjacent_node].add(node)
                    else:
                        if adjacent_node == node:
                            continue
                        loop_dict[adjacent_node] = {node}
            elif adjacent_node in recursion_stack:
                if adjacent_node in loop_dict:
                    if adjacent_node == node:
                        continue
                    loop_dict[adjacent_node].add(node)
                else:
                    if adjacent_node == node:
                        continue
                    loop_dict[adjacent_node] = {node}

        recursion_stack.remove(node)

    visited = set()
    recursion_stack = set()

    for node in graph.nodes:
        if node not in visited:
            detect_loop(node, visited, recursion_stack)

    return loop_dict



#exit_dict = find_exit_nodes(cfg,{'0x10ae6': ['0x10ae2']})
#print(exit_dict)

def find_loop_exit(cfg, loop_info):
    def dfs(node, visited, path):
        visited.add(node)
        path.append(node)

        for loop_entry, wrap_around_nodes in loop_info.items():
            if node in wrap_around_nodes and node != loop_entry:
                return path

        if node in loop_entry_successors:
            for successor in cfg.successors(node):
                if successor not in visited:
                    result = dfs(successor, visited, path)
                    if result:
                        return result

        path.pop()
        return None

    visited = set()

    loop_exits = {}
    for loop_entry, wrap_around_nodes in loop_info.items():
        loop_entry_successors = set(cfg.successors(loop_entry))

        for successor in loop_entry_successors:
            path = dfs(successor, visited, [])
            if path:
                loop_exits[loop_entry] = path[-1]
                break  # Exit when a loop exit is found for this entry

    return loop_exits

def is_reachable(graph, start_node, target_node):
    visited = set()
    stack = [start_node]

    while stack:
        node = stack.pop()
        if node == target_node:
            return True
        if node not in visited:
            visited.add(node)
            stack.extend(graph.successors(node))
    
    return False

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

def find_loop_exit(cfg, start_node, target_node, forbidden_list):
    def has_valid_path(cfg, start_node, target_node, forbidden_list):
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

    visited = set()
    stack = [start_node]

    while stack:
        current_node = stack[-1]

        # Check if the current node has multiple successors
        successors = list(cfg.neighbors(current_node))
        if len(successors) > 1:
            # Check if all paths from the current node to the target are valid
            for successor in successors:
                if not(has_valid_path(cfg, successor, target_node, forbidden_list)):
                    #print("SUCCESSOR IS: ",successor)
                    return successor
            #if not all_paths_valid:
            #    return current_node  # This is the exit of the loop

        visited.add(current_node)
        stack.pop()

        # Add unvisited neighbors to the stack
        for neighbor in successors:
            if neighbor not in visited and neighbor not in stack:
                stack.append(neighbor)

    return None  # If no exit is found

def dfs_all_paths(graph, start_node, target_node):
    def dfs_recursive(node, path):
        if node == target_node:
            for n in path:
                unique_nodes.add(n)  # Add unique nodes to the set
            return
        visited.add(node)
        for successor in graph.successors(node):
            if successor not in visited:
                dfs_recursive(successor, path + [successor])
        visited.remove(node)

    visited = set()
    unique_nodes = set()  # Initialize a set to store unique nodes
    dfs_recursive(start_node, [start_node])
    return unique_nodes



loop_entry_dict_all = {}

################################################ CFG CONSTRUCTION #######################################################################################
loop_exits_dict = {}
# Extracting loop entries and exits
for cfg in cfg_dict:
    loop_entry_dict = find_loop_entries(cfg_dict[cfg][0])
    loop_entry_dict_all.update(loop_entry_dict)

    for loop_entry in loop_entry_dict:
        for exit_node in loop_entry_dict[loop_entry]:
            if loop_entry not in loop_exits_dict:
                loop_exits_dict[loop_entry] = [[find_loop_exit(cfg_dict[cfg][0], loop_entry, exit_node, [])], cfg]
            else:
                loop_exits_dict[loop_entry][0].append(find_loop_exit(cfg_dict[cfg][0],loop_entry,exit_node,[]))
################################################# Static Analysis Based on Loop entries and Exits ########################################################




#print(loop_entry_dict_all)


def find_called_functions(graph, function_name, visited=None):
    if visited is None:
        visited = set()
    
    if function_name not in visited:
        visited.add(function_name)
        called_functions = set()
        
        for called_function in graph.get(function_name, []):
            if called_function != function_name:
                called_functions.add(called_function)
                called_functions.update(find_called_functions(graph, called_function, visited))
        
        return called_functions
    else:
        return set()



cache_line_size = 64
cache_size = 2*1024
number_of_ways = 4
number_of_sets = int((cache_size)/(cache_line_size*number_of_ways))
address_mask = ~(0x3F)
outermost_loop = "" 

outermost_loop_counter = 0

opened_window = False
conflict_graph = {}
locked_lines_durations = {}
temp_window_counter = {}
hotness_dict = {}

loop_entry_occurence = {}

loop_paths = {}
for entry in loop_entry_dict_all:
    for wraparound in loop_entry_dict_all[entry]:    
        if loop_exits_dict[entry][0] is not None:
            if entry not in loop_paths:
                path = dfs_all_paths(cfg_dict[loop_exits_dict[entry][1]][0], entry, wraparound)
                loop_paths[entry] = path # Use a set instead of a list

            else:
                path = dfs_all_paths(cfg_dict[loop_exits_dict[entry][1]][0], entry, wraparound)
                loop_paths[entry] = loop_paths[entry] | path


call_stack_dict = {}
print(cfg_dict)

for cfg in cfg_dict:
    call_stack_dict[cfg] = cfg_dict[cfg][3]

print("Call Stack is ",call_stack_dict)
#print(function_calls_dict)


print("All functions of main: ",find_called_functions(call_stack_dict,'@main'))



for loop_entry in loop_paths:
    for node in loop_paths[loop_entry]:
        if node in function_calls_dict:
            function_called = function_calls_dict[node]
            all_nested_fucntions = find_called_functions(call_stack_dict, function_called)

            for function in all_nested_fucntions:
                #print(function)
                #print(set(cfg_dict[function][0].nodes()))
                loop_paths[loop_entry] = loop_paths[loop_entry].union(set(cfg_dict[function][0].nodes()))