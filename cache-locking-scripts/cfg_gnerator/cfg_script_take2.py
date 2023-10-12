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
reg_values_file = './pointer-example-loop-reg.txt'
# File containing instructions trace
insns_file = './pointer-example-loop.txt'

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
print(insns_dict)
print("CFG construction time: "+str(end_cfg_construction-start_cfg_construction))
#cfg_dict['@fillArray'].write_pdf('cfg1.pdf')

#print("CFG construction time: "+str(end_cfg_construction-start_cfg_construction))
cfg_dict_readable['@manipulateArray'].write_pdf('cfg_readable.pdf')

#pos = nx.spring_layout(cfg_dict['@fillArray'])
#nx.draw(cfg_dict['@fillArray'], pos, with_labels=True, node_size=1000, font_size=10, node_color='lightblue', edge_color='gray')
#plt.show()


#pos = nx.spring_layout(cfg_dict['@main'])
#nx.draw(cfg_dict['@fillArray'],with_labels=True)

#plt.show()





#print(list(cfg_dict['@fillArray'].predecessors('0x10656')))
#print(list(cfg_dict['@fillArray'].successors('0x10656')))
#print(cfg_dict['@fill_Array'].nodes()[0])
#print(cfg_dict['@fillArray'].nodes())

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
        

class BasicBlock:
        def __init__(self, start_node):
            self.start_node = start_node
            self.end_node = None
            self.nodes = []
            self.next_block = []
            



def create_regions(cfg):
    visited = set()
    top_stack = []
    curr_region = Region("start","")

    top_stack.append(curr_region)

    def traverse(node,curr_region):
        
        print("==============================================STACK==========================================================")
        
        for elements in top_stack:
            print(elements.start_node)

        print("TOP NODE IS: ",top_stack[-1].start_node)
        print("==============================================END STACK======================================================")
        if node in visited:

            return


        visited.add(node)

        pred_len = len(list(cfg.predecessors(node))) 
        succ_len = len(list(cfg.successors(node)))


        curr_node = node
        basic_block_nodes = []
        start=curr_node

        

        while succ_len < 2 and pred_len < 2:
            basic_block_nodes.append(curr_node)

            if(succ_len == 0):
                return
            else:
                curr_node = list(cfg.successors(curr_node))[0]        
            
            pred_len = len(list(cfg.predecessors(curr_node))) 
            succ_len = len(list(cfg.successors(curr_node)))

            visited.add(curr_node)
            print(curr_node)
        
        basic_region = Region(start,'')
        basic_region.nodes = basic_block_nodes

        top_stack[-1].nested_regions.append(basic_region)

        #print(basic_block.nodes)
        #new_cfg.add_node(start)
        #new_cfg.add_edge(start,curr_node)

        if pred_len >= 2:
            #print("CURR NODE IS: ",curr_node)
            if top_stack[-1].start_node == curr_node:
                temp = top_stack.pop()
                top_stack[-1].nested_regions.append(temp)
            else:
                new_region = Region(curr_node,"")
                new_region.nodes.append(curr_node)
                
                if any(pred not in visited for pred in list(cfg.predecessors(curr_node))): 
                    top_stack.append(new_region)


                curr_node = list(cfg.successors(curr_node))[0] 
                traverse(curr_node,new_region)

        if succ_len == 2:
            
           
            node_1 = list(cfg.successors(curr_node))[0]
            node_2 = list(cfg.successors(curr_node))[1]
                
            traverse(node_1,node_1)
            traverse(node_2,node_2)
    
    traverse(list(cfg.nodes())[0],curr_region)

    print("HELLO ",top_stack[-1])

create_regions(cfg_dict['@manipulateArray'])



def create_basic_blocks(cfg):
    visited = set()
    new_cfg = nx.DiGraph()

     

    def traverse(node,curr_block):


        if node in visited:
            return

        print(node)
        visited.add(node)

        pred_len = len(list(cfg.predecessors(node))) 
        succ_len = len(list(cfg.successors(node)))


        curr_node = node
        basic_block_nodes = []
        start=curr_node

        #curr_block = BasicBlock(start)

        while succ_len < 2 and pred_len < 2:
            basic_block_nodes.append(curr_node)

            if(succ_len == 0):
                return
            else:
                curr_node = list(cfg.successors(curr_node))[0]        
            
            pred_len = len(list(cfg.predecessors(curr_node))) 
            succ_len = len(list(cfg.successors(curr_node)))

            visited.add(curr_node)
            #print(curr_node)
        
        curr_block.nodes = (basic_block_nodes)

        #print(basic_block.nodes)
        #new_cfg.add_node(start)
        #new_cfg.add_edge(start,curr_node)

        if pred_len >= 2:
            #print("dwdwdwdwd")
            new_block = BasicBlock(curr_node)
            new_block.nodes.append(curr_node)

            curr_block.next_block.append(new_block)
            curr_node = list(cfg.successors(curr_node))[0] 
            traverse(curr_node,new_block)

        if succ_len == 2:
            root_block = BasicBlock(curr_node)
            root_block.nodes.append(curr_node)

            curr_block.next_block.append(root_block)

            node_1 = list(cfg.successors(curr_node))[0]
            node_2 = list(cfg.successors(curr_node))[1]
            
            child1_block = BasicBlock(node_1)
            child2_block = BasicBlock(node_2)

            root_block.next_block.append(child1_block)
            root_block.next_block.append(child2_block)
            
            traverse(node_1,root_block.next_block[0])
            traverse(node_2,root_block.next_block[1])


            
    start_block = BasicBlock(list(cfg.nodes())[0])
    traverse(list(cfg.nodes())[0],start_block)
    
    print(start_block.nodes)
    print(start_block.next_block[0].next_block[0].next_block[0].next_block[0].next_block[0].next_block[1].next_block[0].nodes)
    return new_cfg
    #print(basic_block_list[0].nodes)
    #print(basic_block_list[0].end_node)            

#create_basic_blocks(cfg_dict['@fillArray'])



#cfg_new = create_basic_blocks(cfg_dict['@fillArray'])
#plt.figure(figsize=(10, 6))
#pos = nx.spring_layout(cfg_new, seed=42)  # Layout algorithm for node positioning
#nx.draw(cfg_new, pos, with_labels=True, node_size=1500, node_color='lightblue', edge_color='gray', font_weight='bold', font_size=12)

# Set plot title and display the plot
#plt.title("Control Flow Graph (CFG)")

#plt.show()


def generate_code_regions(cfg):
    regions = []
    visited = set()

    def dfs(node, parent=None):
        visited.add(node)
        region_start = node
        region_type = "Region"

        successors = list(cfg.successors(node))
        while len(successors) == 1:
            successor = successors[0]
            if successor in visited:
                break
            visited.add(successor)
            region_start = successor
            region_type = "Loop"  # Assuming single successors are loop heads
            successors = list(cfg.successors(successor))

        region = Region(region_start, region_type)
        region.parent = parent

        for successor in successors:
            if successor not in visited:
                visited.add(successor)
                nested_region = dfs(successor, parent=region)
                region.nested_regions.append(nested_region)

        region.end_node = region_start
        region.nodes = list(nx.dfs_preorder_nodes(cfg, region_start))

        regions.append(region)
        return region

    start_node = list(cfg.nodes())[0]
    dfs(start_node)

    return regions

def traverse_paths(graph, start_node, visited,dict_temp,stack_nodes):
    stack = [(start_node, [start_node])]

    while stack:
        node, path = stack.pop()

        dict_temp[start_node].append(node)
        


        for neighbor in graph.neighbors(node):
            if neighbor == start_node or len(stack_nodes) == 0:
                # Reached back to the starting node, return the path
                #return path + [start_node]
                stack_nodes.pop()
                return
            

            if neighbor not in path and neighbor not in visited:
                stack.append((neighbor, path + [neighbor]))

    # No path found
    return None



def traverse_paths_2(graph, start_node):
    visited = set()
    paths = {}

    def dfs(node, path):
        visited.add(node)

        entry_edges = 0  # Count the number of entry edges

        for neighbor in graph.neighbors(node):
            if neighbor == start_node:
                # Reached back to the starting node, add the path to the dictionary
                paths[start_node] = path + [start_node]
            elif neighbor not in visited:
                entry_edges += 1
                dfs(neighbor, path + [neighbor])

        # If the node has 2 entry edges, add the path to the dictionary
        if entry_edges == 2:
            paths[node] = path

    dfs(start_node, [start_node])

    return paths



def generate_basic_block_graph(cfg_graph):
    basic_block_graph = nx.DiGraph()

    for node in cfg_graph.nodes():
        # Check if the node has a single entry and single exit
        if len(list(cfg_graph.predecessors(node))) == 1 and len(list(cfg_graph.successors(node))) == 1:
            # Get the single predecessor and successor of the node
            predecessor = next(iter(cfg_graph.predecessors(node)))
            successor = next(iter(cfg_graph.successors(node)))

            # Add the edge between the predecessor and successor as basic block flow
            basic_block_graph.add_edge(predecessor, successor)

    return basic_block_graph

def generate_regions(graph,insn_dict):
    curr_region = Region('start','single-entry')
    
    curr_region.parent = curr_region
    
    region_bottom_stack = deque()
    region_top_stack = deque()
    
    region_top_stack.append(curr_region)
    region_bottom_stack.append(curr_region)

    start_node = list(graph.nodes())[0]
    curr_node = start_node
    print(curr_node)
  
    while (len(region_top_stack) >= 1):
        if(len(list(graph.predecessors(curr_node))) == 0 and len(list(graph.successors(curr_node))) == 1 ):
            region = Region(node.get_name(),"single-entry")
            region.parent = region_top_stack[-1]
            print(region.parent.loop_bound)
            region.loop_bound = int(loop_bounds_dict[int(node.get_name().strip('"'),0)] / region.parent.loop_bound) 
            region_top_stack.append(region)

            # Create a region for nodes that come under the parent node until we hit a node with 2 entries or 2 exits
            while (len(list(graph.successors(curr_node))) < 2 and len(list(graph.predecessors(curr_node))) < 2 ):
                region.nodes.append(curr_node)
                curr_node = list(graph.successors(curr_node))[0]
                print(curr_node[0])
            
            # Apppend region to top region
            temp_region = region_top_stack.pop()
            region_top_stack[-1].nested_regions.append(temp_region)
            
            return region_top_stack[-1]


def print_regions(regions, indent=""):
    for region in regions:
        print(f"{indent}Region Type: {region.region_type}")
        print(f"{indent}Start Node: {region.start_node}")
        print(f"{indent}End Node: {region.end_node}")
        print(f"{indent}Nodes: {region.nodes}")
        print(f"{indent}Loop Bound: {region.loop_bound}")
        print(f"{indent}Memory Nodes: {region.memory_nodes}")
        print(f"{indent}Parent: {region.parent.start_node if region.parent else None}")
        print(f"{indent}Nested Regions:")
        print_regions(region.nested_regions, indent=indent + "  ")
        print()


#regions = generate_code_regions(cfg_dict['@fillArray'])
#print_regions(regions)

#regions = build_region_hierarchy(cfg_dict['@fillArray'])
#print(regions)
#print_regions(regions)
#print(regions)


paths={}
visited=[]
dict_temp={}
stack_node = ['0x10656']

dict_temp['0x10656'] = []
#list1 = traverse_paths(cfg_dict['@fillArray'],'0x10656',[],dict_temp,stack_node)
#list1 = traverse_paths_2(cfg_dict['@fillArray'],'0x10656')
#print(list1)
#result = generate_regions(cfg_dict['@fillArray'],[])
#print(result.nested_regions[0].nodes)
#basic_blocks = extract_basic_blocks(cfg_dict['@fillArray'])
#visualize_basic_blocks(cfg_dict['@fillArray'],basic_blocks)
#basic_blocks_cfg = create_basic_blocks(cfg_dict['@fillArray'])

#for i, block in enumerate(basic_block_graph):
 #   print(f"Basic Block {i + 1}: Entry: {block.entry}, Exits: {block.exits}")


# Create a layout for better visualization (optional)
#pos = nx.spring_layout(basic_block_graph)

# Draw the graph
#nx.draw(basic_block_graph, pos, with_labels=True, node_color='lightblue', node_size=500, font_size=10)

# Add a title
#plt.title("Basic Block Graph")

# Display the graph
#plt.show()



# Assuming you have created the basic blocks CFG using the 'create_basic_blocks' function

# Plot the basic blocks CFG
plt.figure(figsize=(8, 6))
pos_bb = nx.spring_layout(basic_blocks_cfg)
nx.draw(basic_blocks_cfg, pos_bb, with_labels=True, node_size=1000, node_color='lightgreen', font_size=10, font_weight='bold', edge_color='gray')
plt.title('CFG with Basic Blocks')
plt.show()