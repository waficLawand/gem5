import networkx as nx
import matplotlib.pyplot as plt
import re
import pydot
from collections import deque
from IPython.display import Image

# File containing register values trace
reg_values_file = './riscv_insns_five_nested_loops_reg.txt'

# File containing instructions trace
insns_file = './riscv_insns_five_nested_loops.txt'

# Number of instructions cconsidered for analysis
considered_lines = 100

# Extract instructions from the trace file
with open(insns_file) as f:
    lines = f.readlines()

# Loop bounds dictionary
loop_bounds_dict = {}


# List holding all the jump immediate instructions
jmp_imm_insns = ['jal','c_j','j']

# List holding all jump register instructions
jmp_reg_insns = ['c_jr','c_jalr']

# List including all conditional branch instructions
branch_insns = ['c_bnez','bgeu','beq','c_beqz','bge','blt','bltu','bne']

mem_insns = ['ld','lw','sw','lb','sd']


included_insns = branch_insns+jmp_imm_insns+jmp_reg_insns+mem_insns


# Place the instructions in insn_dict dictionary with the pc being the key and the instruction being the value
insns_dict = {}
all_tokens = []

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



# CFG dictionary that holds a seperate cfg for every function, can be referenced using the function name
cfg_dict = {}

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


for i,key in enumerate(sorted(insns_dict.keys())):

    #if insns_dict[key][5] not in included_insns:
     #   continue

    #if i == considered_lines:
     #   break

    # Extract program counter value from the insn dictionary
    pc.append(int(insns_dict[key][3],16))

    # Extract insn name from the insn directory
    insn.append(insns_dict[key][5])

    # Check if the insn is a jump with immediate value
    if insn[i] in jmp_imm_insns:
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

for i,key in enumerate(sorted(insns_dict.keys())):
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

        
    elif insn[i] in jmp_imm_insns:
        node = pydot.Node(hex(pc[i]),label=str("pc: "+hex(pc[i])+ " insn: "+insn[i]))
        

        cfg.add_node(node)
        
        #if(pc[i]+jmp_imm[i] in insns_dict.keys()):
         #   edge = pydot.Edge(hex(pc[i]),hex(pc[i]+jmp_imm[i]))
          #  cfg.add_edge(edge)  
        
    elif insn[i] in jmp_reg_insns:
        node = pydot.Node(hex(pc[i]),label=str("pc: "+hex(pc[i])+ " insn: "+insn[i]))
        cfg.add_node(node)
        
        
    else:
        node = pydot.Node(hex(pc[i]),label=str("pc: "+hex(pc[i])+ " insn: "+insn[i]))
        cfg.add_node(node)


    if i != 0:
        if (insns_dict[key][4].split('+')[0] == insns_dict[pc[i-1]][4].split('+')[0]): 
           # if insn[i-1] not in jmp_imm_insns:
                edge = pydot.Edge(hex(pc[i-1]),hex(pc[i]))
                cfg.add_edge(edge)

cfg_dict['@main'].write_pdf('cfg.pdf')


def get_entrances_exits(node,graph):
    entrances = []
    exits = []
    for edge in graph.get_edges():
        source_node_name = edge.get_source()
        dest_node_name = edge.get_destination()

        #print(dest_node_name,node.get_name())

        if dest_node_name == node.get_name() and source_node_name not in entrances:
            entrances.append(source_node_name)

        if source_node_name ==  node.get_name() and dest_node_name not in exits:
            exits.append(dest_node_name)

    entrance_nodes = [graph.get_node(node_name)[0] for node_name in entrances]
    exit_nodes = [graph.get_node(node_name)[0] for node_name in exits]

    return entrance_nodes, exit_nodes


class Region:
    def __init__(self, start_node,region_type):
        self.start_node = start_node
        self.region_type = region_type
        self.end_node = None
        self.nodes = []
        self.loop_bound = 1
        self.nested_regions = []
        self.parent = None
        
def generate_regions_refined_take_1(graph):

    curr_region = Region('start','multiple-entry')
    curr_region_2 = Region('start','multiple-entry')

    curr_region.parent = curr_region
    
    region_bottom_stack = deque()
    region_top_stack = deque()
    
    region_top_stack.append(curr_region)
    region_bottom_stack.append(curr_region_2)

    # loop over al the nodes
    for node in graph.get_node_list():
        entrance_nodes,exit_nodes = get_entrances_exits(node,graph)

        # ==================== DEBUG SECTION==============================================
        #print("--------- HEREE -----------------------")
        #for element in region_top_stack:
         #   if element.start_node == "start":
          #      for regions in element.nested_regions:
           #         print(regions.start_node)
        #print("--------- HEREE -----------------------")
        #print("---------------stack elements --------------------------")
        #for element in region_top_stack:
         #   print(element.start_node)
        #print("---------------stack elements --------------------------")
        # ==================== DEBUG SECTION==============================================

        if len(entrance_nodes) == 0:
         #   print("entry")
            region = Region(node.get_name(),"single-entry")
            region.parent = region_top_stack[-1]
            region.loop_bound = int(loop_bounds_dict[int(node.get_name().strip('"'),0)] / region.parent.loop_bound) 
            region_top_stack.append(region)
            region_top_stack[-1].nodes.append(node)
        
        elif len(entrance_nodes) >= 2:
            if region_top_stack[-1].region_type == "single-entry":
                popped_region = region_top_stack.pop()
                region_top_stack[-1].nested_regions.append(popped_region)

            if region_bottom_stack[-1].start_node in [node.get_name() for node in get_entrances_exits(node,graph)[0]]:
                
                # ==================== DEBUG SECTION==============================================
                #print("inverted!")
                #print("=======new===")
                #print(region_bottom_stack[-1].start_node)
                #print(region_top_stack[-1].start_node)
                #for region in region_top_stack[-1].nested_regions:
                #    print("region:"+region.start_node)
                #print("last node: "+node.get_name())
                #print("====new====")
                # ==================== DEBUG SECTION==============================================

                temp_region = region_top_stack.pop()
                temp_region.nodes.append(node)
                region_top_stack[-1].nested_regions.append(temp_region)

                temp_region2 = region_bottom_stack.pop()
                temp_region2.nodes.append(node)
                #curr_region = region_bottom_stack[-1]
                region_bottom_stack[-1].nested_regions.append(temp_region2)

     

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

            if region_top_stack[-1].start_node in [node.get_name() for node in get_entrances_exits(node,graph)[1]]:
                temp_region = region_top_stack.pop()
                temp_region.nodes.append(node)
                #curr_region = region_top_stack[-1]
                region_top_stack[-1].nested_regions.append(temp_region)

            else:
                region = Region(node.get_name(),"multiple-entry")
                region.nodes.append(node)

                

                region.parent = region_top_stack[-1]
                region.loop_bound = int(loop_bounds_dict[int(node.get_name().strip('"'),0)] / region.parent.loop_bound) 
                region_top_stack.append(region)
                
                
                region_bottom_stack.append(region)

        elif len(exit_nodes) == 0:
            region_top_stack[-1].nodes.append(node)

            if region_top_stack[-1].region_type == "single-entry":
                temp_region = region_top_stack.pop()
                region_top_stack[-1].nested_regions.append(temp_region)

            elif region_top_stack[-1].region_type == "multiple-entry":
                region = Region(node.get_name(),"single-entry")
                region.nodes.append(node)
                region_top_stack[-1].nested_regions.append(region)

        else:
            if region_top_stack[-1].region_type == "multiple-entry":
                region = Region(node.get_name(),"single-entry")
                region.parent = region_top_stack[-1]
                region.loop_bound = int(loop_bounds_dict[int(node.get_name().strip('"'),0)] / region.parent.loop_bound) 
                region_top_stack.append(region)

            region_top_stack[-1].nodes.append(node)

    return region_top_stack[-1]


regions = generate_regions_refined_take_1(cfg_dict['@main'])


with open('./test_cfg.txt','w') as f:
    for key in sorted(insns_dict.keys()):
        f.write(insns_dict[key][3]+" "+insns_dict[key][4]+" "+insns_dict[key][5]+" "+insns_dict[key][6]+"\n")
    
for region in regions.nested_regions:
    print(region.start_node)


#while(regions != None):
    #print(regions.parent.start_node)
    #regions = regions.nested_regions

#for node in regions:
    #print(loop_bounds_dict[int('0x10600',0)])
 #   print(node.get_name())

        
    


        







