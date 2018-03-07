import sqlite3
import matplotlib.pyplot as plt
import numpy as np

conn = sqlite3.connect('benchmark_data.db')

c = conn.cursor()

graph_dir = '/Users/bionicbug/Documents/Cambridge/3rdYear/Project/writeup/figs/'

graph_parameters = [('0add_nodes', 'Time taken to add 50000 nodes', graph_dir + 'add_nodes.png', '1add_nodes', 'neo4j_add_nodes', '2add_nodes'),
					('0add_edges', 'Time taken to add 100 edges to 5000 nodes', graph_dir + 'add_edges.png', '1add_edges', 'neo4j_add_edges', '2add_edges')#,
					#('0power', 'Time taken to read all 7196 edges from a power-law graph with 1000 nodes', 'power.png'),
					#('0kronecker', 'Time taken to read all 2655 edges from a kronecker graph with 1024 nodes', 'kronecker.png'),
					]

for parameters in graph_parameters:
	dllama_mean_times = []
	dllama_standard_deviations = []
	simple_dllama_mean_times = []
	simple_dllama_standard_deviations = []

	for num_machines in range (1, 11):
		args = (parameters[0], num_machines)
		times = []
		for row in c.execute('SELECT * FROM data WHERE benchmark = ? AND num_machines = ? ORDER BY benchmark', args):
			times.append(row[3] / 1000.0)
		mean_time = np.mean(times)
		dllama_mean_times.append(mean_time)
		dllama_standard_deviations.append(np.std(times))

		if parameters[0] == '0add_edges':
			args = (parameters[3], num_machines)
			times = []
			for row in c.execute('SELECT * FROM data WHERE benchmark = ? AND num_machines = ? ORDER BY benchmark', args):
				times.append(row[3] / 1000.0)
			mean_time = np.mean(times)
			simple_dllama_mean_times.append(mean_time)
			simple_dllama_standard_deviations.append(np.std(times))

	t = np.arange(1, 11, 1)
	plt.figure()
	plt.errorbar(t, dllama_mean_times, yerr=dllama_standard_deviations)
	if parameters[0] == '0add_edges':
		plt.errorbar(t, simple_dllama_mean_times, yerr=simple_dllama_standard_deviations)

	times = []
	for row in c.execute('SELECT * FROM data WHERE benchmark = ? ORDER BY benchmark', (parameters[4],)):
		times.append(row[3] / 1000.0)
	neo4j_mean_time = [np.mean(times)] * 10
	neo4j_std = [0.0] * 10
	neo4j_std[0] = np.std(times)
	plt.errorbar(t, neo4j_mean_time, yerr=neo4j_std)

	times = []
	for row in c.execute('SELECT * FROM data WHERE benchmark = ? ORDER BY benchmark', (parameters[5],)):
		times.append(row[3] / 1000.0)
	llama_mean_time = [np.mean(times)] * 10
	llama_std = [0.0] * 10
	llama_std[0] = np.std(times)
	plt.errorbar(t, llama_mean_time, yerr=llama_std)

	if parameters[0] == '0add_edges':
		plt.legend(['DLLAMA', 'Simple DLLAMA', 'Embedded Neo4j', 'LLAMA'])
	else:
		plt.legend(['DLLAMA', 'Embedded Neo4j', 'LLAMA'])

	plt.xlabel('number of machines')
	plt.ylabel('time (milliseconds)')
	#plt.title(parameters[1])
	plt.grid(True)
	plt.savefig(parameters[2])
	#plt.show()




merge_mean_times = []
merge_standard_deviations = []
num_nodes = [500, 1000, 1500, 2000, 2500, 3000, 4000, 5000]

for nn in num_nodes:
	args = ('0merge_benchmark', nn)
	times = []
	for row in c.execute('SELECT * FROM data WHERE benchmark = ? AND num_vertices = ? ORDER BY num_vertices', args):
		times.append(row[3] / 1000.0)
	mean_time = np.mean(times)
	merge_mean_times.append(mean_time)
	merge_standard_deviations.append(np.std(times))

plt.figure()
plt.errorbar(num_nodes, merge_mean_times, yerr=merge_standard_deviations)

plt.xlabel('number of nodes')
plt.ylabel('time (milliseconds)')
#plt.title('Time taken to merge 10 snapshots, each with n nodes, each with 100 edges')
plt.grid(True)
xmin, xmax = plt.xlim()
plt.xlim(0.0, xmax)
plt.savefig(graph_dir + 'merge_times.png')
#plt.show()






parameters = [('0read_edges', 'Time taken to read all 100 edges from 5000 nodes', graph_dir + 'read_edges.png', 'neo4j_read_edges', '2read_edges', 5000), 
				#('0merge_benchmark', 'Time taken to merge snapshots', 'merge.png', 'neo4j_merge'), 
				('0breadth_first', 'Time taken to complete breadth first search on power-law graph with 50000 nodes', graph_dir + 'breadth.png', 'neo4j_breadth_first', '2breadth_first', 50000), 
				('0power', 'Time taken to read all 7196 edges from a power-law graph with 1000 nodes', graph_dir + 'power.png', 'neo4j_power', '2power', 1000), 
				('0kronecker', 'Time taken to read all 2655 edges from a kronecker graph with 1024 nodes', graph_dir + 'kronecker.png', 'neo4j_kronecker', '2kronecker', 1024),
				('0power', 'Time taken to read all edges from a power-law graph with 50000 nodes', graph_dir + 'power2.png', 'neo4j_power', '2power', 50000)
				]

for param in parameters:
	means_dllama = []
	std_dllama = []
	times = []
	args = (param[0], 1, param[5])
	for row in c.execute('SELECT * FROM data WHERE benchmark = ? AND num_machines = ? AND num_vertices = ? ORDER BY benchmark', args):
		times.append(row[3])
	mean_time = np.mean(times)
	means_dllama.append(mean_time)
	std_dllama.append(np.std(times))

	if param[3] == 'neo4j_merge':
		means_neo4j = [0.0]
		std_neo4j = [0.0]
		n_groups = 2
	else:
		means_neo4j = []
		std_neo4j = []
		times = []
		args = (param[3], 1, param[5])
		for row in c.execute('SELECT * FROM data WHERE benchmark = ? AND num_machines = ? AND num_vertices = ? ORDER BY benchmark', args):
			times.append(row[3])
		mean_time = np.mean(times)
		means_neo4j.append(mean_time)
		std_neo4j.append(np.std(times))
		n_groups = 3

	means_llama = []
	std_llama = []
	times = []
	args = (param[4], 1, param[5])
	for row in c.execute('SELECT * FROM data WHERE benchmark = ? AND num_machines = ? AND num_vertices = ? ORDER BY benchmark', args):
		times.append(row[3])
	mean_time = np.mean(times)
	means_llama.append(mean_time)
	std_llama.append(np.std(times))

	#means_dllama = (20, 35, 30, 35, 27)
	#std_dllama = (2, 3, 4, 1, 2)

	means = [means_dllama[0], means_neo4j[0], means_llama[0]]
	stds = [std_dllama[0], std_neo4j[0], std_llama[0]]

	fig, ax = plt.subplots()
	index = np.arange(n_groups)
	bar_width = 0.35
	opacity = 0.4
	error_config = {'ecolor': '0.3'}

	rects1 = ax.barh(index, means, bar_width,
	                alpha=opacity, color='b',
	                xerr=stds, error_kw=error_config)

	#rects2 = ax.bar(index + bar_width, means_neo4j, bar_width,
	#                alpha=opacity, color='r',
	#                yerr=std_neo4j, error_kw=error_config,
	#                label='Neo4j')

	ax.set_ylabel('Database')
	ax.set_xlabel('Time (microseconds)')
	#ax.set_title('Time taken for each benchmark')
	ax.set_yticks(index)
	ax.set_yticklabels(('DLLAMA', 'Neo4j', 'LLAMA'))
	#ax.set_xticklabels(('Read edges', 'Breadth first', 'Power law', 'Kronecker'))
	#ax.legend()

	fig.tight_layout()
	#plt.title(param[1])
	xmin, xmax = plt.xlim()
	plt.xlim(0.0, xmax)

	plt.savefig(param[2])




conn.close()