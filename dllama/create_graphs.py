import sqlite3
import matplotlib.pyplot as plt
import numpy as np

conn = sqlite3.connect('benchmark_data.db')

c = conn.cursor()

mean_times = []
for num_machines in range (1, 11):
	args = ('0add_edges', num_machines)
	mean_time = 0.0
	num_trials = 0
	for row in c.execute('SELECT * FROM data WHERE benchmark = ? AND num_machines = ? ORDER BY benchmark', args):
		mean_time += row[3]
		num_trials += 1
	mean_time /= num_trials
	mean_times.append(mean_time)

conn.close()

t = np.arange(1, 11, 1)
plt.plot(t, mean_times)

plt.xlabel('number of machines')
plt.ylabel('time (microseconds)')
plt.title('Time taken to add 50000 nodes')
plt.grid(True)
plt.savefig("test.png")
plt.show()