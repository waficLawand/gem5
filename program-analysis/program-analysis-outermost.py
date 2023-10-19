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
insns_file = './qsort/qsort-exec.txt'
mmu_file = './qsort/qsort-mmu.txt'


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
#cfg_readable = {}
def new_ILP(conflict_window, weights):
    prob = pulp.LpProblem("Maximize_Weight", pulp.LpMaximize)
    nodes = set()


    for cache_set in conflict_window:
        nodes.update(conflict_window[cache_set])

    x = pulp.LpVariable.dicts("Node", nodes, cat=pulp.LpBinary)
    
    # Define the objective function: Maximize the total weight of selected nodes
    prob += pulp.lpSum([weights[node] * x[node] for node in nodes])

    for cache_set in conflict_window:
        prob += pulp.lpSum([x[node] for node in conflict_window[cache_set]]) <= 2

    # Solve ILP
    prob.solve()

    # Extract the selected nodes and their weights
    selected_nodes = {node: x[node].value() for node in nodes if x[node].value() == 1}
    #selected_weights = {node: weights[node] for conflict_set in conflict_sets for window in conflict_sets[conflict_set] for node in window if node in selected_nodes}

    return selected_nodes




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

"""translation_dict={}
with open(mmu_file, 'r') as file:
    for line in file:
        if "Translating:" in line:
            parts = line.strip().split("Translating: ")
            if len(parts) == 2:
                address, value = parts[1].split("->")
                translation_dict[address.strip()] = value.strip()"""


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
cache_size = 4*1024
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

print(call_stack_dict)
print(function_calls_dict)

print(find_called_functions(function_calls_dict,'@main'))

print(loop_paths)
#print(find_called_functions(call_stack_dict,'@__libc_start_main'))

#print(call_stack_dict)
#def find_all_nodes(start_node,function,cfg_dict):
#print(cfg_dict['@bubbleSort'][0].nodes())


main_fn_calls = find_called_functions(call_stack_dict, '@main')
main_fn_set = set()

#for function in main_fn_calls:
#    print("Function is ",function," and nodes are: ",cfg_dict[function][0].nodes())
#    main_fn_set = main_fn_set.union(set(cfg_dict[function][0].nodes()))


print("ALL NODES ARE: ",main_fn_set)
for loop_entry in loop_paths:
    for node in loop_paths[loop_entry]:
        if node in function_calls_dict:

            function_called = function_calls_dict[node]
            print("Function Called from loop: ",function_called)

            all_nested_fucntions = find_called_functions(call_stack_dict, function_called)
            loop_paths[loop_entry] = loop_paths[loop_entry].union(set(cfg_dict[function_called][0].nodes()))
            
            for function in all_nested_fucntions:
                print("Nested function call: ",function," and nodes are: ",cfg_dict[function][0].nodes())
                #print(function)
                #print(set(cfg_dict[function][0].nodes()))
                loop_paths[loop_entry] = loop_paths[loop_entry].union(set(cfg_dict[function][0].nodes()))
            
#print("MAIN PATH IS: ",loop_paths['0x1054a'])

all_nodes_in_loop = set()

for loop_entry in loop_paths:
    for node in loop_paths[loop_entry]:
        all_nodes_in_loop.add(node)

print("Number of nodes inside loops is: ",len(all_nodes_in_loop))
print("Number of loops is: ",len(loop_paths))

loop_sizes = []
for loop_entry in loop_paths:
    loop_sizes.append(len(loop_paths[loop_entry]))

print("Loop Sizes: ",loop_sizes)

print("Average Window Size: ",sum(loop_sizes)//len(loop_sizes))

benchmark_fn_calls = ['@main','@clustering','@dequeue','@enqueue']

total_nodes = 0
benchmark_nodes = 0
syscall_nodes = 0

for fn in cfg_dict:
    if fn in benchmark_fn_calls:
        benchmark_nodes += len(list(cfg_dict[fn][0].nodes()))
    else:
        syscall_nodes += len(list(cfg_dict[fn][0].nodes()))
    
    total_nodes += len(list(cfg_dict[fn][0].nodes()))


print("Benchmark Nodes: ",benchmark_nodes)
print("Syscall Nodes: ",syscall_nodes)
print("Total Nodes: ",total_nodes)

loop_iterations = {}

for insn in program_insns:
    insn_address = int(insn[3],16)
    if hex(insn_address) in loop_exits_dict:
        if hex(insn_address) not in loop_iterations:
            loop_iterations[hex(insn_address)] = 1

        else:
            loop_iterations[hex(insn_address)] += 1

print(loop_iterations)

loop_size = {}

for loop_entry in loop_paths:
    print("Number of nodes in loop entry ",loop_entry," is ",len(loop_paths[loop_entry]))
    loop_size[loop_entry] = loop_iterations[loop_entry]*len(loop_paths[loop_entry])

print("Actual Size of loop trace is: ",loop_size)

print(loop_exits_dict)

bubble_sort_portion = 0
all_insns = 0

for insn in program_insns:
    if insn[4].split("+")[0] in ['@clustering','@dequeue','@enqueue']:
        bubble_sort_portion+=1
    
    else:
        all_insns+=1

print("Bubble sort trace is: ",bubble_sort_portion)
print("Rest of program is: ",all_insns)


print("Call stack is: ",call_stack_dict)

translation_dict={}
with open(mmu_file, 'r') as file:
    for line in file:
        if "Translating:" in line:
            parts = line.strip().split("Translating: ")
            if len(parts) == 2:
                address, value = parts[1].split("->")
                translation_dict[address.strip()] = value.strip()




cache_line_size = 64
cache_size = 4*1024
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

loop_stack = ['start']

#for i in range(0,number_of_sets):
#    conflict_graph[i]=[set()]
for loop in loop_exits_dict:
    conflict_graph[loop] = [{},[],{}]

for loop in loop_exits_dict:
    for set_nb in range(number_of_sets):
        conflict_graph[loop][0][set_nb] = set()


#print(conflict_graph)

#print(loop_paths)

for insn in program_insns:
    insn_address = int(insn[3],16)
    #print(loop_stack)


    #print(insn_address)
    #if not opened_window:
    if hex(insn_address) in loop_exits_dict and outermost_loop == "":
        if loop_exits_dict[hex(insn_address)][0] != None:
                #print("WINDOW START!: ",hex(insn_address)) 
                outermost_loop = hex(insn_address)

                opened_window = True

    if outermost_loop != "":
        translated_address = int(translation_dict[hex(insn_address & (~3))],16) &  address_mask
        accessed_set = calculate_set(cache_size,number_of_ways,cache_line_size,translated_address)

        #print(hex(translated_address))
        #print(hex((insn_address&(~3))&address_mask))
        #print(hex(insn_address))
        #conflict_graph[accessed_set][-1].add(hex(translated_address))
        conflict_graph[outermost_loop][0][accessed_set].add(hex(translated_address))
        #conflict_graph[loop_stack[-1]][0][accessed_set].add(hex((insn_address &(~3))&address_mask))
        #if hex((insn_address &(~3))&address_mask) not in hotness_dict:
        if hex(translated_address) not in hotness_dict:
            hotness_dict[hex(translated_address)] = 1
            #hotness_dict[hex((insn_address &(~3))&address_mask)] = 1
        else:
            hotness_dict[hex(translated_address)] = hotness_dict[hex(translated_address)]+1
            #hotness_dict[hex((insn_address &(~3))&address_mask)] += 1

        if hex(translated_address) not in locked_lines_durations:
            #locked_pages_durations[hex(translated_page)] = 0
            locked_lines_durations[hex(translated_address)] = [1,1]
            #locked_lines_durations[hex(translated_address)] =[]

        if hex(translated_address) not in temp_window_counter:
        #if hex((insn_address &(~3))&address_mask) not in temp_window_counter: 
            temp_window_counter[hex(translated_address)] = 1
            #temp_window_counter[hex((insn_address &(~3))&address_mask)] = 1
        else:
            temp_window_counter[hex(translated_address)] = temp_window_counter[hex(translated_address)] +1
            #temp_window_counter[hex((insn_address &(~3))&address_mask)] = temp_window_counter[hex((insn_address &(~3))&address_mask)] +1



    #if outermost_loop != '':
    #    if (outermost_loop in loop_exits_dict):

    if outermost_loop in loop_exits_dict:
    #print("Hello IM HERE!")
        if hex(insn_address) in loop_exits_dict[outermost_loop][0] or (hex(insn_address) not in loop_paths[outermost_loop]):
    #if hex(insn_address) in loop_exits_dict[outermost_loop][0] or (hex(insn_address) not in loop_paths[outermost_loop]):
    #if (hex(insn_address) not in loop_paths[outermost_loop]) or hex(insn_address) == outermost_loop:
            #print("WINDOW END!: ",hex(insn_address))

            
            for node in temp_window_counter:
                if node not in conflict_graph[outermost_loop][2]:
                    conflict_graph[outermost_loop][2][node] = temp_window_counter[node]
                else:
                    #if conflict_graph[outermost_loop][2][node] < temp_window_counter[node]:
                        conflict_graph[outermost_loop][2][node] += temp_window_counter[node]
            #for index in range(0,number_of_sets):
                #conflict_graph[index].append(set())

            #for line in temp_window_counter.keys():
                #locked_lines_durations[line].append(temp_window_counter[line])
#                            if temp_window_counter[line] != 0:
#                               locked_lines_durations[line][0] = temp_window_counter[line]+locked_lines_durations[line][0]
#                              locked_lines_durations[line][1] = locked_lines_durations[line][1] + 1 

            temp_window_counter = {}
            opened_window = False
            outermost_loop = ''
            #loop_stack.pop()




for loop in conflict_graph:
    print("Loop ",loop,": ",conflict_graph[loop])


for loop in loop_paths:
    print("Loop is ",loop," and the path includes: ",loop_paths[loop])
#for loop in conflict_graph:


print(conflict_graph)

def compute_union(dictionary, entry_key):
    entry = dictionary.get(entry_key, None)

    if entry is None:
        return set()

    loop_sets = entry[0]

    # Recursively compute the union of sets in nested loops
    nested_loop_entries = entry[1]
    nested_union = set()
    for nested_entry_key in nested_loop_entries:
        nested_union.update(compute_union(dictionary, nested_entry_key))

    # Compute the union of sets in the current loop
    current_union = set()
    for set_key in loop_sets:
        current_union.update(loop_sets[set_key])

    # Combine the union of sets in the current loop with the nested union
    current_union.update(nested_union)

    return current_union


def compute_access_counts_recursive(dictionary, entry_key):
    entry = dictionary.get(entry_key, None)

    if entry is None:
        return {}

    access_counts = entry[2]
    
    # Recursively compute the total number of accesses in nested loops
    nested_loop_entries = entry[1]
    for nested_entry_key in nested_loop_entries:
        nested_access_counts = compute_access_counts_recursive(dictionary, nested_entry_key)
        for node, count in nested_access_counts.items():
            access_counts[node] = access_counts.get(node, 0) + count

    return access_counts


durations_dict = {}
for loop in conflict_graph:
    if len(conflict_graph[loop][0]) > 0:
    #selected_nodes = new_ILP(conflict_graph[loop][0],conflict_graph[loop][2])
        selected_nodes = new_ILP(conflict_graph[loop][0],hotness_dict)

    for node in selected_nodes:
        if node not in durations_dict:
            durations_dict[node] = conflict_graph[loop][2][node]
        else:
            if durations_dict[node] > conflict_graph[loop][2][node]:
            #if durations_dict[node] < conflict_graph[loop][2][node]:
                durations_dict[node] = conflict_graph[loop][2][node]



print("LAST SET: ",durations_dict)
for node in durations_dict.keys():
    if durations_dict[node] == 1:
        continue
    print("lockingDurationsTable[" + node + "]=" + str(durations_dict[node]) + ";")



#print(selected_nodes)
#print(selected_weights)

#temp = compute_union(conflict_graph,'0x1069c')
#print(temp)

#temp2 = compute_union(conflict_graph,'0x149fe')

#print(temp2)

#temp3 = compute_access_counts_recursive(conflict_graph,'0x1069c')
#print(temp3)

#print(hotness_dict)