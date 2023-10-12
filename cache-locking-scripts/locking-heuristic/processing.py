fixed_dict={}
with open('fixed-window.txt') as f:
    lines = f.readlines()
    for line in lines:
        address = line.split('lockingDurationsTable[')[1].split(']=')[0]
        access = line.split('lockingDurationsTable[')[1].split(']=')[1].split(';')[0]
        fixed_dict[address] = access

loop_dict={}
with open('loop-window.txt') as f:
    lines = f.readlines()
    for line in lines:
        address = line.split('lockingDurationsTable[')[1].split(']=')[0]
        access = line.split('lockingDurationsTable[')[1].split(']=')[1].split(';')[0]
        loop_dict[address] = access

keys_union = set(fixed_dict.keys()) - set(loop_dict.keys())

print(len(keys_union))
print(len(loop_dict.keys()))
print(len(fixed_dict.keys()))

for node in keys_union:
    print("lockingDurationsTable[" + node + "]=" + str(fixed_dict[node]) + ";")
