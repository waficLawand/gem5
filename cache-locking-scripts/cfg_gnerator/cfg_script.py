import networkx as nx
import matplotlib.pyplot as plt
import re
import pydot
from collections import deque
from IPython.display import Image
import time 
from collections import Counter

# File containing register values trace
reg_values_file = './double-loop-reg.txt'
# File containing instructions trace
insns_file = './double-loop.txt'

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

start_insn_filling = time.time()
# Creating a CFG dictionary for function calls
cfg = pydot.Dot(graph_type='digraph')
cfg_dict['start'] = cfg



pc=[]
insn=[]
jmp_imm=[]
branch_imm=[]
mem_addr=[]
mem_data=[]
mem_read_write=[]

unique_insns = set([token[5] for token in all_tokens])

sorted_keys = sorted(insns_dict.keys())


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
for i,key in enumerate(sorted_keys):
   # if i == considered_lines:
       # break



    if i != 0:
        # This condition checks for new functions 
        if(insns_dict[key][4].split('+')[0] != insns_dict[pc[i-1]][4].split('+')[0]):
            cfg = pydot.Dot(graph_type='digraph')
            cfg_dict[str(insns_dict[key][4].split('+')[0])] = cfg


    if insn[i] in branch_insns:        
        node = pydot.Node(hex(pc[i]),label=str("pc: "+hex(pc[i])+ " insn: "+insn[i]))
        edge = pydot.Edge(hex(pc[i]),hex(pc[i]+branch_imm[i]))

        cfg.add_node(node)

         # Add branch edge, other edges will be added by nonbranch nodes
        if(pc[i]+branch_imm[i] != pc[i+1]):
            if(pc[i]+branch_imm[i] in insns_dict.keys()):
                cfg.add_edge(edge)

        
    elif insn[i] in jmp_direct_insns:
        node = pydot.Node(hex(pc[i]),label=str("pc: "+hex(pc[i])+ " insn: "+insn[i]))
        cfg.add_node(node)
        #print(len(jmp_imm),len(pc))
        print(jmp_imm[i],pc[i])
        if(pc[i]+jmp_imm[i] in insns_dict.keys()):
            edge = pydot.Edge(hex(pc[i]),hex(pc[i]+jmp_imm[i]))
            cfg.add_edge(edge)  
        
    elif insn[i] in jmp_reg_insns or insn[i] in jmp_imm_insns:
        node = pydot.Node(hex(pc[i]),label=str("pc: "+hex(pc[i])+ " insn: "+insn[i]))
        cfg.add_node(node)

    elif insn[i] in mem_insns:
        node = pydot.Node(hex(pc[i]),label=str("pc: "+hex(pc[i])+ " insn: "+insn[i]+ "address: "+mem_addr[i]))
        cfg.add_node(node)
        
    else:
        node = pydot.Node(hex(pc[i]),label=str("pc: "+hex(pc[i])+ " insn: "+insn[i]))
        cfg.add_node(node)


    if i != 0:
        if (insns_dict[key][4].split('+')[0] == insns_dict[pc[i-1]][4].split('+')[0]): 
           if insn[i-1] not in jmp_direct_insns:
                edge = pydot.Edge(hex(pc[i-1]),hex(pc[i]))
                cfg.add_edge(edge)
end_cfg_construction = time.time()
print(insns_dict)
print("CFG construction time: "+str(end_cfg_construction-start_cfg_construction))
cfg_dict['@fillArray'].write_pdf('cfg1.pdf')


def get_entrances_exits(node,graph):
    #start_get_entrance_exits = time.time()
    entrances = set()
    exits = set()
    for edge in graph.get_edges():
        source_node_name = edge.get_source()
        dest_node_name = edge.get_destination()

        #print(dest_node_name,node.get_name())

        if dest_node_name == node.get_name() and source_node_name not in entrances:
            entrances.add(source_node_name)

        if source_node_name ==  node.get_name() and dest_node_name not in exits:
            exits.add(dest_node_name)

    entrance_nodes = [graph.get_node(node_name)[0] for node_name in entrances]
    exit_nodes = [graph.get_node(node_name)[0] for node_name in exits]

    #end_get_entrance_exits = time.time()
    #print("Exit Entrance time is : "+ str(end_get_entrance_exits-start_get_entrance_exits))

    return entrance_nodes, exit_nodes


    #end_get_entrance_exits = time.time()
    #print("Exit Entrance time is:", end_get_entrance_exits - start_get_entrance_exits)

    return entrance_nodes, exit_nodes

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
        
def generate_regions_refined_take_1(graph,insn_dict):

    curr_region = Region('start','multiple-entry')
    curr_region_2 = Region('start','multiple-entry')

    curr_region.parent = curr_region
    
    region_bottom_stack = deque()
    region_top_stack = deque()
    
    region_top_stack.append(curr_region)
    region_bottom_stack.append(curr_region_2)

    # loop over al the nodes
    for node in graph.get_node_list():

        mem_node = insn_dict[int(node.get_name().strip('"'),0)][5] in mem_insns
        if mem_node:
            mem_address = insns_dict[int(node.get_name().strip('"'),0)][8].split(' ')[1].split('A=')[1]

        start_comp = time.time()
        entrance_nodes,exit_nodes = get_entrances_exits(node,graph)
        end_comp = time.time()

        start_comp_2= time.time()
        # ==================== DEBUG SECTION==============================================
        print("--------- HEREE -----------------------")
        for element in region_top_stack:
            if element.start_node == "start":
                for regions in element.nested_regions:
                    print(regions.start_node)
        print("--------- HEREE -----------------------")
        print("---------------stack elements --------------------------")
        for element in region_top_stack:
            print(element.start_node)
        print("---------------stack elements --------------------------")
        # ==================== DEBUG SECTION==============================================

        if len(entrance_nodes) == 0:
         #   print("entry")
            region = Region(node.get_name(),"single-entry")
            region.parent = region_top_stack[-1]
            print(region.parent.loop_bound)
            region.loop_bound = int(loop_bounds_dict[int(node.get_name().strip('"'),0)] / region.parent.loop_bound) 
            region_top_stack.append(region)
            
            if(mem_node):
                region_top_stack[-1].memory_nodes.append(mem_address)
                
            region_top_stack[-1].nodes.append(node)
        
        elif len(entrance_nodes) >= 2:
            print("ENTRANCE NODES: ",len(entrance_nodes))
            if region_top_stack[-1].region_type == "single-entry":
                popped_region = region_top_stack.pop()
                region_top_stack[-1].nested_regions.append(popped_region)

            if region_bottom_stack[-1].start_node in [node.get_name() for node in entrance_nodes]:

                for i in range(len(entrance_nodes)-1):
                
                    # ==================== DEBUG SECTION==============================================
                    #print("inverted!")
                    print("=======new===")
                    print(region_bottom_stack[-1].start_node)
                    print(region_top_stack[-1].start_node)
                    for region in region_top_stack[-1].nested_regions:
                        print("region:"+region.start_node)
                    print("last node: "+node.get_name())
                    print("====new====")
                    # ==================== DEBUG SECTION==============================================


                    temp_region = region_top_stack.pop()
                    temp_region.nodes.append(node)

                    
                    if(mem_node):
                        temp_region.memory_nodes.append(mem_address)

                    region_top_stack[-1].nested_regions.append(temp_region)

                    #temp_region2 = region_bottom_stack.pop()
                    #temp_region2.nodes.append(node)

                    
                    #if(mem_node):
                        #temp_region2.memory_nodes.append(mem_address)

                    #curr_region = region_bottom_stack[-1]
                    #region_bottom_stack[-1].nested_regions.append(temp_region2)
                region_bottom_stack.pop()

        

            else:
                region = Region(node.get_name(),"multiple-entry")
                region.nodes.append(node)
                region.parent = region_top_stack[-1]
                region.loop_bound = int(loop_bounds_dict[int(node.get_name().strip('"'),0)] / region.parent.loop_bound) 
                region_top_stack.append(region)

        elif len(exit_nodes)  >= 2:
            if region_top_stack[-1].region_type == "single-entry":
                popped_region = region_top_stack.pop()
                region_top_stack[-1].nested_regions.append(popped_region)

            if region_top_stack[-1].start_node in [node.get_name() for node in exit_nodes]:
                temp_region = region_top_stack.pop()
                
                if(mem_node):
                    temp_region.memory_nodes.append(mem_address)

                temp_region.nodes.append(node)
                #curr_region = region_top_stack[-1]
                region_top_stack[-1].nested_regions.append(temp_region)

            else:
                region = Region(node.get_name(),"multiple-entry")
                region.nodes.append(node)
                
                if(mem_node):
                    region.memory_nodes.append(mem_address)

                

                region.parent = region_top_stack[-1]
                region.loop_bound = int(loop_bounds_dict[int(node.get_name().strip('"'),0)] / region.parent.loop_bound) 
                region_top_stack.append(region)
                
                
                region_bottom_stack.append(region)

        elif len(exit_nodes) == 0:
            region_top_stack[-1].nodes.append(node)
            
            if(mem_node):
                region_top_stack[-1].memory_nodes.append(mem_address)

            if region_top_stack[-1].region_type == "single-entry":
                temp_region = region_top_stack.pop()
                region_top_stack[-1].nested_regions.append(temp_region)

            elif region_top_stack[-1].region_type == "multiple-entry":
                region = Region(node.get_name(),"single-entry")
                region.nodes.append(node)

                if(mem_node):
                    region.memory_nodes.append(mem_address)
                
                region_top_stack[-1].nested_regions.append(region)

        else:
            if region_top_stack[-1].region_type == "multiple-entry":
                region = Region(node.get_name(),"single-entry")
                region.parent = region_top_stack[-1]
                print(region.parent.loop_bound)
                print(region.start_node)
                print(loop_bounds_dict[int(node.get_name().strip('"'),0)])

                # CHANGE LATER VERY IMPORTANT!!!!!!!
                if region.parent.loop_bound == 0:
                    region.loop_bound
                else:
                    region.loop_bound = int(loop_bounds_dict[int(node.get_name().strip('"'),0)] / region.parent.loop_bound) 
                region_top_stack.append(region)

            region_top_stack[-1].nodes.append(node)

            if(mem_node):
                region_top_stack[-1].memory_nodes.append(mem_address)

        end_comp2 = time.time()

        #print("--------------------------------------------------")
        #print("Entrance exit time is : "+ str(end_comp-start_comp))
        #print("Actual logic time is : "+ str(end_comp2-start_comp_2))

        #print("Length of stack is: ",len(region_top_stack))
        #print("Length of bottom stack is: ",len(region_bottom_stack))
    return region_top_stack[-1]


region_mem_elements = []

locked_lines={}

def cache_locking_heuristic(region,cache_ways,merged_children,all_nodes,locked_nodes_per_region,locked_nodes_per_program):
    if len(region.nested_regions) == 0:
        #print("Merged Children: "+str(merged_children))
        #print(region.start_node)
        mem_dict={}

        for node in region.memory_nodes:
            if node not in mem_dict:
                mem_dict[node] = 1
            else:
                mem_dict[node] = mem_dict[node]+1

        mem_dict = dict(sorted(mem_dict.items(), key=lambda item: item[1],reverse=True))

        if mem_dict is None:
            mem_dict = {}

        #region_mem_elements.append(mem_dict)
        #region_mem_elements.append(list(mem_dict.items()) [:2])

        locked_nodes_per_region[0][region.start_node] = dict(list(mem_dict.items())[:cache_ways])
        return mem_dict
    else:

        for nested_region in region.nested_regions:
            #print("REGION: "+nested_region.start_node)
            temp = cache_locking_heuristic(nested_region,cache_ways,merged_children,all_nodes,locked_nodes_per_region,locked_nodes_per_program)

            if temp is None:
                temp = {}
            

            merged_children[0][nested_region.start_node] = temp
            #locked_nodes_per_region[0][nested_region.start_node] = dict(list(temp.items())[:cache_ways])
        
        # Filling the all nodes dictionary that includes all regions with all nodes and their corresponding loop bounds
        if region.start_node not in all_nodes[0]:
             all_nodes[0][region.start_node] = {}

        

        for nested in region.nested_regions:

            if nested.start_node in all_nodes[0]:
                all_nodes[0][region.start_node] =  dict(Counter(all_nodes[0][region.start_node]) + Counter(all_nodes[0][nested.start_node]))
            else:
                all_nodes[0][region.start_node] =  dict(Counter(all_nodes[0][region.start_node]) + Counter(merged_children[0][nested.start_node]))

            #locked_nodes_per_region[0][nested.start_node] = dict(list(merged_children[0][nested.start_node].items())[:cache_ways])
            #if nested.start_node not in locked_nodes_per_region:
                #locked_nodes_per_region[0][nested.start_node] = list(all_nodes[0][nested.start_node].items() [:cache_ways])


            print("AFTER STARNODE: "+ str(all_nodes[0][region.start_node]))
            print("AFTER: "+str(all_nodes))
            #del all_nodes[0][nested.start_node]
            print("-----------------------MERGING-----------------------------")
            

        for mem_blks in all_nodes[0][region.start_node]:
            all_nodes[0][region.start_node][mem_blks] = all_nodes[0][region.start_node][mem_blks]*region.loop_bound

   
        for nested in region.nested_regions:
            print(nested.start_node)

        print("PANIC: "+str(dict(list(all_nodes[0][region.start_node].items())[:cache_ways])))
        print("REGION START NODE: "+str(region.start_node))

        sorted_dict = dict(sorted(all_nodes[0][region.start_node].items(), key=lambda item: item[1],reverse=True))

        locked_nodes_per_region[0][region.start_node] = dict(list(sorted_dict.items())[:cache_ways])
        print("LOCKED REGION: "+str(locked_nodes_per_region[0]))
        print("-----------------------MERGING-----------------------------")

        
        for nested in region.nested_regions:
            for address in locked_nodes_per_region[0][nested.start_node]:
                if address not in locked_nodes_per_program[0].keys():
                    locked_nodes_per_program[0][address] = locked_nodes_per_region[0][nested.start_node][address]
                else:
                    if locked_nodes_per_program[0][address] <= locked_nodes_per_region[0][nested.start_node][address]:
                        locked_nodes_per_program[0][address] = locked_nodes_per_region[0][nested.start_node][address]

        if region.start_node == 'start':
            for address in locked_nodes_per_region[0][region.start_node]:
                #print("ADDRESS IS: ",address, "START NODE IS: ",region.start_node)    
                if locked_nodes_per_program[0][address] <= locked_nodes_per_region[0][region.start_node][address]:
                    locked_nodes_per_program[0][address] = locked_nodes_per_region[0][region.start_node][address]
                    #locked_nodes_per_program[0][address] = locked_nodes_per_region[0][region.start_node][address]
                    #print("WHY!: "+str(locked_nodes_per_program))
                #else:
                    #print("JDWOKJDOWJDO")
                #else:
                    #locked_nodes_per_program[0][address] = locked_nodes_per_region[0][region.start_node][address]
        
        #merged_children = [{}]  
        
        #for mem_address in merged_children[0][region.start_node].keys():
            #print(region)
            #print(merged_children[0][region.parent.start_node][mem_address])
        #    merged_children[0][region.start_node][mem_address] = merged_children[0][region.start_node][mem_address] * region.loop_bound

        #all_blocks.append(merged_children)
        #merged_children= [{}]

        
        
        #print("Merged children after callin: "+str(merged_children))
        
        #print(region_mem_elements)
        
        #print(region.start_node)

        
        #print("Merged Children: "+str(merged_children))
        

            #print("BEFORE: "+str(all_nodes))
            #print("PARENT NODE: "+str(region.start_node))
            #print("NESTED NODE: "+str(nested.start_node))
            #print("NODE TO ADD: "+str(all_nodes[0][nested.start_node]))
            #print("INITIAL STARNODE: "+ str(all_nodes[0][region.start_node]))
            #print("MERGED NESTED NODE: "+str(merged_children[0][nested.start_node]))



            #print("Children start node: "+region.start_node+"  mems: "+str(temp))
            #print("Merged children before callin: "+str(merged_children))

            #if nested_region.parent.start_node in merged_children[0]:
            #    merged_children[0][nested_region.parent.start_node] = dict(Counter(merged_children[0][nested_region.parent.start_node]) + Counter(temp))
            #else:
            #    merged_children[0][nested_region.parent.start_node] = temp
            #print("TEMP: "+str(temp))
            #print("REGION: "+region.start_node)
            #print("PARENT: "+region.parent.start_node)
                    
            #print("----------------------------------")
            #print(merged_children[0])
            #print("----------------------------------")
            


            #print("Merged children after callin: "+str(merged_children))

        #print("-------------------Region Elements-------------------------------")
        #for node in region.nodes:
        #    print(node.get_name())
        #print("-------------------Region Elements-------------------------------")





start_region_generation = time.time()
regions = generate_regions_refined_take_1(cfg_dict['@main'],insns_dict)

print("WHY! ",len(regions.nested_regions))
#print(regions.nested_regions[1].parent.start_node)
end_region_generation = time.time()
print(regions.nested_regions)   
dict_temp = [{}]
all_nodes=[{}]

locked_per_region = [{}]
locked_per_program = [{}]

cache_locking_heuristic(regions,10,dict_temp,all_nodes,locked_per_region,locked_per_program)
#print(dict_temp)
#print(all_nodes)
print(locked_per_region)
print(locked_per_program)
#print(dict_temp)
#print(all_blocks_1)
#print(region_mem_elements)
#print_nested_regions(regions)
#all_nodes = create_memory_nodes_dictionary(regions,1,mem_dict2)


print("Region generation time: "+ str(end_region_generation-start_region_generation))

#with open('./test_cfg.txt','w') as f:
 #   for key in sorted(insns_dict.keys()):
  #      f.write(insns_dict[key][3]+" "+insns_dict[key][4]+" "+insns_dict[key][5]+" "+insns_dict[key][6]+"\n")

#print(regions.nested_regions[0].memory_nodes) 
for node in regions.nested_regions:
    print(node.loop_bound)


#while(regions != None):
    #print(regions.parent.start_node)
    #regions = regions.nested_regions

#for node in regions:
    #print(loop_bounds_dict[int('0x10600',0)])
 #   print(node.get_name())


dict1 = {'a': 12, 'for': 25, 'c': 9}
dict2 = {'Geeks': 100, 'geek': 200, 'for': 300}
 
 
# adding the values with common key
         
Cdict = Counter(dict1) + Counter(dict2)
print(dict(Cdict))