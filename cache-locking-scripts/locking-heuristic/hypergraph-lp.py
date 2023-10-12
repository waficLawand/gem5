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
            prob += pulp.lpSum([x[node] for node in window]) <= 1


     # Solve the LP problem
    prob.solve()

    # Extract the selected nodes and their weights
    selected_nodes = {node: x[node].value() for node in nodes if x[node].value() == 1}
    selected_weights = {node: weights[node] for conflict_set in conflict_sets for window in conflict_sets[conflict_set] for node in window if node in selected_nodes}

    return selected_nodes, selected_weights



alphabet_dict = {}

for i in range(26):
    letter = chr(65 + i)  # 65 is the ASCII code for 'A'
    value = 26 - i
    alphabet_dict[letter] = value


#conflict_sets = [{'A':alphabet_dict['A'],'I':alphabet_dict['I'],'M':alphabet_dict['M']},{'I':alphabet_dict['I']},{'E':alphabet_dict['E'],'A':alphabet_dict['A']}]
#conflict_sets= {0:[{'A','I','M'},{'I'},{'E','A'}],1:[{'B','C'},{'B','D'}]}
weights = {'0x71000': 136, '0x6e000': 29, '0x6c000': 9, '0x50000': 8, '0x0': 8, '0x6d000': 1, '0x6f000': 3, '0x72000': 3}
conflict_sets = {0:[{'0x6c000', '0x6e000'}, {'0x6c000', '0x6e000', '0x50000'}, {'0x6c000', '0x6e000'}, {'0x0', '0x6c000', '0x72000', '0x50000', '0x6e000'}],1:[{'0x71000'}, {'0x71000'}, {'0x71000'}, {'0x71000'}, {'0x71000', '0x6d000', '0x6f000'}]}


print(conflict_sets)
selected_nodes, selected_weights = maximize_weight_with_conflict_sets(conflict_sets,weights)
print("Selected Nodes:", selected_nodes)
print("Selected Weights:", selected_weights)

