import pydot

# Create the first component
subgraph1 = pydot.Subgraph('cluster1')
node1 = pydot.Node('Node 1')
node2 = pydot.Node('Node 2')
subgraph1.add_node(node1)
subgraph1.add_node(node2)

# Create the second component
subgraph2 = pydot.Subgraph('cluster2')
node3 = pydot.Node('Node 3')
node4 = pydot.Node('Node 4')
subgraph2.add_node(node3)
subgraph2.add_node(node4)

# Combine the two components into a single graph
graph = pydot.Dot()
graph.add_subgraph(subgraph1)
graph.add_subgraph(subgraph2)

# Add edges to the graph
edge1 = pydot.Edge(node1, node2)
edge2 = pydot.Edge(node3, node4)
graph.add_edge(edge1)
graph.add_edge(edge2)

# Save the graph to a file
graph.write_png('disjointed_graph.png')