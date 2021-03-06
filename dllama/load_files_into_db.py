import sqlite3
import os

conn = sqlite3.connect('benchmark_data.db')

c = conn.cursor()

# Create table
c.execute('''CREATE TABLE IF NOT EXISTS data
	(benchmark text, num_machines integer, num_vertices integer, time_taken real)''')


def load_data_from_dir(dir):
	#f = open('slurm-702000.out', 'r')
	output_files = [filename for filename in os.listdir(dir) if filename.startswith("slurm-")]
	#output_files = [filename for filename in os.listdir('.') if filename.startswith("test_slurm")]

	for filename in output_files:
		print(filename)
		f = open(dir + '/' + filename, 'r')
		#f = open('neo4j data/' + filename, 'r')
		skipped_header = False
		for line in f:
			#print(line)
			if skipped_header:
				if line == '\n':
					break
				words = line.split(' ')
				benchmark = words[0]
				num_machines = words[1]
				num_vertices = words[2]
				for i in range(3, len(words) - 1):
					time_taken = words[i]
					c.execute("INSERT INTO data VALUES (?,?,?,?)", (benchmark, num_machines, num_vertices, time_taken))
			elif line == 'started benchmark\n':
				skipped_header = True
		f.close()

load_data_from_dir('output4/with prev')
c.execute("DELETE FROM data WHERE benchmark = ?", ('0add_nodes',))
load_data_from_dir('neo4j data')
load_data_from_dir('output5/add nodes')
load_data_from_dir('output5/with prev')
load_data_from_dir('output6')

# Save (commit) the changes
conn.commit()

for row in c.execute('SELECT * FROM data ORDER BY benchmark'):
	print row

# We can also close the connection if we are done with it.
# Just be sure any changes have been committed or they will be lost.
conn.close()