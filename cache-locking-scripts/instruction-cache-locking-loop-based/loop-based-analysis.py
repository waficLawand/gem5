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
insns_file = './dijkstra/dijkstra-exec.txt'
mmu_file = './dijkstra/dijkstra-mmu.txt'


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
#cfg_readable = {}

def maximize_weight_with_conflict_sets(conflict_sets, weights):
    # Create the LP problem instance
    prob = pulp.LpProblem("Maximize_Weight", pulp.LpMaximize)
    
    # Create binary decision variables for each node
    nodes = set()
    for conflict_set in conflict_sets:
        for window in conflict_sets[conflict_set]:
            nodes.update(window)

    x = pulp.LpVariable.dicts("Node", nodes, cat=pulp.LpBinary)

    # Define the objective function: Maximize the total weight of selected nodes
    prob += pulp.lpSum([weights[node] * x[node] for node in nodes])

     # Add constraints: Each node can be picked at most once
    #for conflict_set in conflict_sets:
    #    for node in nodes:
    #        prob += pulp.lpSum([x[node] for window in conflict_sets[conflict_set] if node in window]) <= 1

    # Add constraints: Conflicting nodes cannot be picked together
    for conflict_set in conflict_sets:
        for window in conflict_sets[conflict_set]:
            prob += pulp.lpSum([x[node] for node in window]) <= 2

     # Solve the LP problem
    prob.solve()

    # Extract the selected nodes and their weights
    selected_nodes = {node: x[node].value() for node in nodes if x[node].value() == 1}
    selected_weights = {node: weights[node] for conflict_set in conflict_sets for window in conflict_sets[conflict_set] for node in window if node in selected_nodes}

    return selected_nodes, selected_weights



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


            curr_function = array_of_tokens[4].split('+')[0]
            
            program_insns.append(array_of_tokens)
            #print(array_of_tokens)
            if curr_function not in cfg_dict:
                cfg_dict[curr_function] = [nx.DiGraph(),0,0]
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
            
            #print("Loop Entry is: ",loop_entry," Wraparound Node is: ",exit_node," Exit Node is: ",find_loop_exit(cfg_dict[cfg][0],loop_entry,exit_node,[])," For Function: ",cfg)

################################################# Static Analysis Based on Loop entries and Exits ########################################################

translation_dict={}
with open(mmu_file, 'r') as file:
    for line in file:
        if "Translating:" in line:
            parts = line.strip().split("Translating: ")
            if len(parts) == 2:
                address, value = parts[1].split("->")
                translation_dict[address.strip()] = value.strip()


print(loop_entry_dict_all)
"""loop_paths = {}
for entry in loop_entry_dict_all:
    for wraparound in loop_entry_dict_all[entry]:    
        if loop_exits_dict[entry][0] is not None:
            if entry not in loop_paths:
                print("dadada")
                path = dfs_all_paths(cfg_dict[loop_exits_dict[entry][1]][0], entry, wraparound)
                loop_paths[entry] = path # Use a set instead of a list

            else:
                path = dfs_all_paths(cfg_dict[loop_exits_dict[entry][1]][0], entry, wraparound)
                loop_paths[entry] = loop_paths[entry] | path
            
            #loop_paths[entry].append(path)  # Add paths to the set"""
#print(loop_paths)



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

for i in range(0,number_of_sets):
    conflict_graph[i]=[set()]
print(loop_exits_dict)
for insn in program_insns:
    insn_address = int(insn[3],16)


    #print(insn_address)
    #if not opened_window:
    if hex(insn_address) in loop_exits_dict:
        if opened_window:
            print("WINDOW END!: ",hex(insn_address))

            for index in range(0,number_of_sets):
                conflict_graph[index].append(set())

            for line in temp_window_counter.keys():
                    if temp_window_counter[line] != 0:
                        locked_lines_durations[line][0] = temp_window_counter[line]+locked_lines_durations[line][0]
                        locked_lines_durations[line][1] = locked_lines_durations[line][1] + 1 

            temp_window_counter = {}
            opened_window = False
        if loop_exits_dict[hex(insn_address)][0] != None:

            print("WINDOW START!: ",hex(insn_address)) 
            outermost_loop = hex(insn_address)
            opened_window = True

    if opened_window:
        translated_address = int(translation_dict[hex(insn_address & (~3))],16) &  address_mask
        accessed_set = calculate_set(cache_size,number_of_ways,cache_line_size,translated_address)
        print(hex(insn_address))
        conflict_graph[accessed_set][-1].add(hex(translated_address))

        if hex(translated_address) not in hotness_dict:
            hotness_dict[hex(translated_address)] = 1
        else:
            hotness_dict[hex(translated_address)] = hotness_dict[hex(translated_address)]+1

        if hex(translated_address) not in locked_lines_durations:
            #locked_pages_durations[hex(translated_page)] = 0
            locked_lines_durations[hex(translated_address)] = [1,1]

        if hex(translated_address) not in temp_window_counter:
            temp_window_counter[hex(translated_address)] = 1
        else:
            temp_window_counter[hex(translated_address)] = temp_window_counter[hex(translated_address)] +1

    #if outermost_loop != '':
        #if (outermost_loop in loop_exits_dict) or (loop_entry_occurence[outermost_loop] == insn_accesses[outermost_loop] -1):
            #print("Hello IM HERE!")
        if hex(insn_address) in loop_exits_dict[outermost_loop][0]:

            print("WINDOW END!: ",hex(insn_address))

            for index in range(0,number_of_sets):
                conflict_graph[index].append(set())

            for line in temp_window_counter.keys():
                    if temp_window_counter[line] != 0:
                        locked_lines_durations[line][0] = temp_window_counter[line]+locked_lines_durations[line][0]
                        locked_lines_durations[line][1] = locked_lines_durations[line][1] + 1 

            #temp_window_counter = {}
            opened_window = False





print(hotness_dict)
selected_nodes, selected_weights = maximize_weight_with_conflict_sets(conflict_graph,hotness_dict)

print(selected_nodes)
print(selected_weights)

for node in selected_nodes.keys():
    print("lockingDurationsTable[" + node + "]=" + str(locked_lines_durations[node][0]//locked_lines_durations[node][1]) + ";")


print(locked_lines_durations)