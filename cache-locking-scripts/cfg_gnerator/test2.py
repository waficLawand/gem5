
def parse_asm(asm):
    # create a directed graph for the CFG
    cfg = nx.DiGraph()

    # a dictionary to store the labels and their corresponding instruction index
    labels = {}

    # split the assembly code into lines
    lines = asm.split('\n')

    # iterate over the lines
    i = 0
    instruction = 0
    for i in range(len(lines)):
        line = lines[i].strip()
        #print(line)

        if line.endswith(':'):
            label = line[:-1]
            print(label)
            labels[label] = instruction+1
            #print(instruction)
        else:
            instruction+=1

    """while i < len(lines):
        line = lines[i].strip()
        #print(line)
        
        # ignore empty lines and comments
        if not line or line.startswith('#'):
            i += 1
            continue

        # if the line starts with a label, add it to the dictionary
        if line.endswith(':'):
            label = line[:-1]
            labels[label] = i
            i += 1
            continue
        else:
            i+=1
"""

    print(labels)

    i =0
    instruction = 0
    #print(operands)
    while i < len(lines):
        line = lines[i].strip()
        #print(line)
        
        #print(line)
        # ignore empty lines and comments

        if line.endswith(':'):
            i+=1
            continue

        print(line)
        # split the line into instruction and operands
        instr, operands = re.split(r'\s+',line,maxsplit=1)
        operands = operands.split(',')
        
        print(operands)
        # add the current instruction to the CFG
        cfg.add_node(instruction, label=instr)

        # handle the different instruction types
        if instr == 'j':
            target = operands[0]
            cfg.add_edge(i, target)
            i += 1

        elif instr.startswith('b'):
            rs1 = operands[0]
            rs2 = operands[1]
            target = labels[operands[2]]

            # add the edges for the next instruction and the jump target
            cfg.add_edge(instruction, instruction+1)
            cfg.add_edge(instruction, target)

            # add the edge for the branch condition
            #cfg.add_edge(i, rs1)
            #cfg.add_edge(i, rs2)
            i += 1
            instruction+=1
        else:
            # add the edge for the next instruction
            cfg.add_edge(instruction, instruction+1)
            instruction += 1

    return cfg


#G = parse_asm(asm)

#pos = nx.spring_layout(G)
#nx.draw_networkx_nodes(G, pos)
#nx.draw_networkx_edges(G, pos)
#nx.draw_networkx_labels(G, pos)
#plt.show()