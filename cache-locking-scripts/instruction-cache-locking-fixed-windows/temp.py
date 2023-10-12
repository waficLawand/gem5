page_start,page_end = calculate_page_boundary(int(address,16),page_size)

                        #print("THIS IS WHERE THE PAGE STARTS: ",hex(page_start))
            
                        translated_page_number = translation_dict[int(page_start)>>shift_value]
                        mask = (1 << shift_value)-1
                        translated_line = translated_page | int(address,16) & mask

                        #print("Page is:",hex(page_start),"TRANSLATED: ",hex(translated_page)," Page Number is: ",hex(translated_page_number))

                        if hex(translated_page) not in page_weights_dict:
                            page_weights_dict[hex(translated_page)] = 1
                        else:
                            page_weights_dict[hex(translated_page)] = page_weights_dict[hex(translated_page)]+1

                        # Constructing the conflict Graph
                        accessed_set =  calculate_set(cache_size, number_of_ways, 64, translated_line)
                        
                        conflict_graph[accessed_set][-1].add(hex(translated_page))


                        #is_conflict = check_conflict(translated_line,conflict_graph[accessed_way][-1])

                        #is_conflict = check_conflict_page(hex(translated_line),conflict_graph[accessed_set][-1])

                        #if hex(translated_page) not in conflict_graph[accessed_way][-1] and is_conflict or len(conflict_graph[accessed_way][-1]) == 0 :
                        #    conflict_graph[accessed_way][-1].add(hex(translated_page))

                        #print(hex(translated_page))
                        if hex(translated_page) not in locked_pages_durations:
                            #locked_pages_durations[hex(translated_page)] = 0
                            locked_pages_durations[hex(translated_page)] = [1,1]

                        if hex(translated_page) not in temp_window_counter:
                            temp_window_counter[hex(translated_page)] = 1
                        else:
                            temp_window_counter[hex(translated_page)] = temp_window_counter[hex(translated_page)] +1
                        
                        # We finihsed going over a full window
                        if current_insn % window_size == 0:
                            for page in temp_window_counter.keys():
                                if temp_window_counter[page] != 0:
                                    locked_pages_durations[page][0] = temp_window_counter[page]+locked_pages_durations[page][0]
                                    locked_pages_durations[page][1] = locked_pages_durations[page][1] + 1 
                            #print(temp_window_counter)
                            # Find the maximum across all windows
                            #update_duration(locked_pages_durations,temp_window_counter)

                            # Clear the temp dictionary for the next window
                            temp_window_counter = {}

                            for index in range(0,number_of_sets):
                                conflict_graph[index].append(set())
                        
                        current_insn = current_insn + 1


                    

'''for j in range(0,number_of_ways):
    for i in conflict_graph[j]:
        for node in i:
            print(node)
        print("NEW SET")'''





'''for key in insns_dict:
        # Calculate page boundaries
        #print(key)
        address = insns_dict[key][8].split(' ')[1].split("A=")[1]
        page_start,page_end = calculate_page_boundary(int(address,16),4096)
        
        translated_page_number = translation_dict[int(page_start)>>12]
        mask = (1 << 12)-1
        translated_page = translated_page_number<<12

        print("Page is:",hex(page_start),"TRANSLATED: ",hex(translated_page)," Page Number is: ",hex(translated_page_number))

        if hex(translated_page) not in page_weights_dict:
            page_weights_dict[hex(translated_page)] = 1
        else:
            page_weights_dict[hex(translated_page)] = page_weights_dict[hex(translated_page)]+1'''


    

print("DONE PROCESSING! ILP INITIATED!")
#def construct_conflict_graph(window_size,page_weights_dict,):

selected_nodes, selected_weights = maximize_weight_with_conflict_sets(conflict_graph,page_weights_dict)

#print(locked_pages_durations)

for node in selected_nodes.keys():
    print("lockingDurationsTable[" + node + "]=" + str(locked_pages_durations[node][0]//locked_pages_durations[node][1]) + ";\n")
    #print("lockingDurationsTable[" + node + "]=" + str(locked_pages_durations[node]) + ";\n")
#print(conflict_graph[0])
#print(conflict_graph[1])
#print(page_weights_dict)
#print(conflict_graph)
#print(locked_pages_durations)

print("Selected Nodes:", selected_nodes)
print("Selected Weights:", selected_weights)

#print(len(page_weights_dict))
#print(page_weights_dict)