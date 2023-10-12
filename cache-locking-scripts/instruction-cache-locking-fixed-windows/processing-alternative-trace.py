import re
import matplotlib.pyplot as plt
import re
import pydot
from collections import deque
import time 
from collections import Counter
import sys
import math 
import pulp


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


def update_duration(dictionary, previous_window):
    for address in dictionary:
        dictionary[address] = max(dictionary[address], previous_window.get(address, 0))
        

def update_duration_max(dictionary, previous_window):
    for address in dictionary:
        dictionary[address] = max(dictionary[address], previous_window.get(address, 0))

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


def calculate_page_boundary(virtual_address, page_size):
    page_offset = virtual_address % page_size
    page_start_address = virtual_address - page_offset
    page_end_address = page_start_address + page_size
    return page_start_address, page_end_address

def check_conflict(address,conflict_set):
    set_number = calculate_set(cache_size, number_of_ways, cache_line_size, address)

    for element in conflict_set:
        set_number_address = calculate_set(cache_size, number_of_ways, cache_line_size, element)

        if(set_number == set_number_address):
            return True


def check_conflict_page(address,conflict_set):
    way_number = (int(address,16) // page_size) % number_of_ways

    for element in conflict_set:
        way_number_address = (int(element,16) // page_size) % number_of_ways

        if(way_number == way_number_address):
            return True


# File containing instructions trace
#insns_file = 'disparity/cif/disparity-sim-exec.txt'
#virt_to_phys_file = 'disparity/cif/disparity-cif-virt-to-phys.txt'
insns_file = 'dijkstra/dijkstra-exec.txt'
mmu_file = './dijkstra/dijkstra-mmu.txt'
address_mask = ~(0x3F)
# Extract instructions from the trace file
translation_dict={}
inverse_translation_dict={}
with open(mmu_file, 'r') as file:
    for line in file:
        if "Translating:" in line:
            parts = line.strip().split("Translating: ")
            if len(parts) == 2:
                address, value = parts[1].split("->")
                translation_dict[address.strip()] = value.strip()
                inverse_translation_dict[hex((int(value.strip(),16)&~3)&address_mask)] =hex((int(address.strip(),16)&~3)&address_mask)
#Cache organizaiton
number_of_ways = 4
cache_size = 4*1024
#page_size = 4 *1024
page_size= 64
#page_size = 1
cache_line_size = 64
number_of_sets = int((cache_size)/(cache_line_size*number_of_ways))

print(number_of_sets)
#Window size
window_size = 100000

#shift_value = 12
#shift_value = 12
shift_value=6
conflict_graph={}

mask = ~(0x3F)

# Dictionary keeping track of durations throughout windows
locked_pages_durations = {}

# Setting up the Conflict grpah
for i in range(0,number_of_sets):
    conflict_graph[i]=[set()]


#print(calculate_set(1024,2,64,translation_dict['0x73340']))
# Place the instructions in insn_dict dictionary with the pc being the key and the instruction being the value
insns_dict = {}
all_tokens = []
page_weights_dict = {}
insns_list = []
locked_lines_durations = {}
hotness_dict = {}

window_set = set()

start_time_file_read = time.time()
# window Counter
current_insn = 0
temp_window_counter = {}
address_mask = ~(0x3F)
with open(insns_file) as f:
    for line in f:
        if 'T0' not in line:
            continue
        else:
            #program_counter = line.split(':')[3].split(' @')[0]
            #print(line)
            cache_line = int(translation_dict[hex((int(line.split('T0 : ')[1].split(' @')[0],16)&~3))],16)&(address_mask)
            #cache_line = translation_dict((int(line.split('T0 : ')[1].split(' @')[0],16)&~3)&address_mask)
            #cache_line = int(translation_dict(((line.split('T0 : ')[1].split(' @')[0])&~3)&address_mask),16)
            #print(cache_line)
            # Extracting cache line from the program counter value by masking the last 6 bits
            #cache_line = int(instruction_address,16) & mask

            # Keeping track of cache line hotness
            if hex(cache_line) not in hotness_dict:
                hotness_dict[hex(cache_line)] = 1
            else:
                hotness_dict[hex(cache_line)] = hotness_dict[hex(cache_line)]+1


            # Constructing the conflict Graph
            accessed_set =  calculate_set(cache_size, number_of_ways, 64, cache_line)        
            conflict_graph[accessed_set][-1].add(hex(cache_line))


            if hex(cache_line) not in locked_lines_durations:
                #locked_pages_durations[hex(translated_page)] = 0
                locked_lines_durations[hex(cache_line)] = [1,1]

            if hex(cache_line) not in temp_window_counter:
                temp_window_counter[hex(cache_line)] = 1
            else:
                temp_window_counter[hex(cache_line)] = temp_window_counter[hex(cache_line)] +1

            if current_insn % window_size == 0:
                for line in temp_window_counter.keys():
                    if temp_window_counter[line] != 0:
                        locked_lines_durations[line][0] = temp_window_counter[line]+locked_lines_durations[line][0]
                        locked_lines_durations[line][1] = locked_lines_durations[line][1] + 1 

                temp_window_counter = {}
                
                for index in range(0,number_of_sets):
                    conflict_graph[index].append(set())
        
            current_insn = current_insn + 1


print(hotness_dict)
print(conflict_graph)
selected_nodes, selected_weights = maximize_weight_with_conflict_sets(conflict_graph,hotness_dict)

print(selected_nodes)
print(selected_weights)

for node in selected_nodes.keys():
    print("lockingDurationsTable[" + inverse_translation_dict[node] + "]=" + str(locked_lines_durations[node][0]//locked_lines_durations[node][1]) + ";")
print("######################################################################\n")
for node in selected_nodes.keys():
    print("lockingDurationsTable[" + node + "]=" + str(locked_lines_durations[node][0]//locked_lines_durations[node][1]) + ";")


print(locked_lines_durations)



            
                        