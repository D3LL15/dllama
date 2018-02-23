import sqlite3
import matplotlib.pyplot as plt
import numpy as np

conn = sqlite3.connect('benchmark_data.db')

c = conn.cursor()

graph_parameters = [('0add_nodes', 'Time taken to add 50000 nodes', 'add_nodes.png'),
					('0add_edges', 'Time taken to add 100 edges to 5000 nodes', 'add_edges.png'),
					('0power', 'Time taken to read all 7196 edges from a power-law graph with 1000 nodes', 'power.png'),
					('0kronecker', 'Time taken to read all 2655 edges from a kronecker graph with 1024 nodes', 'kronecker.png'),
					]

for parameters in graph_parameters:
	mean_times = []
	standard_deviations = []
	for num_machines in range (1, 11):
		args = (parameters[0], num_machines)
		mean_time = 0.0
		num_trials = 0
		times = []
		for row in c.execute('SELECT * FROM data WHERE benchmark = ? AND num_machines = ? ORDER BY benchmark', args):
			times.append(row[3])
			mean_time += row[3]
			num_trials += 1
		mean_time /= num_trials
		mean_times.append(mean_time)
		standard_deviations.append(np.std(times))

	t = np.arange(1, 11, 1)
	plt.figure()
	plt.errorbar(t, mean_times, yerr=standard_deviations)

	plt.xlabel('number of machines')
	plt.ylabel('time (microseconds)')
	plt.title(parameters[1])
	plt.grid(True)
	plt.savefig(parameters[2])
	#plt.show()



n_groups = 5

means_men = (20, 35, 30, 35, 27)
std_men = (2, 3, 4, 1, 2)

means_women = (25, 32, 34, 20, 25)
std_women = (3, 5, 2, 3, 3)

fig, ax = plt.subplots()

index = np.arange(n_groups)
bar_width = 0.35

opacity = 0.4
error_config = {'ecolor': '0.3'}

rects1 = ax.bar(index, means_men, bar_width,
                alpha=opacity, color='b',
                yerr=std_men, error_kw=error_config,
                label='Men')

rects2 = ax.bar(index + bar_width, means_women, bar_width,
                alpha=opacity, color='r',
                yerr=std_women, error_kw=error_config,
                label='Women')

ax.set_xlabel('Group')
ax.set_ylabel('Scores')
ax.set_title('Scores by group and gender')
ax.set_xticks(index + bar_width / 2)
ax.set_xticklabels(('A', 'B', 'C', 'D', 'E'))
ax.legend()

fig.tight_layout()
plt.show()

conn.close()