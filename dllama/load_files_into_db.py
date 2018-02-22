import sqlite3
import os

conn = sqlite3.connect('benchmark_data.db')

c = conn.cursor()

# Create table
c.execute('''CREATE TABLE IF NOT EXISTS data
	(benchmark text, num_machines integer, num_vertices integer, time_taken real)''')


#f = open('slurm-702000.out', 'r')
output_files = [filename for filename in os.listdir('.') if filename.startswith("slurm-")]

for filename in output_files:
	f = open(filename, 'r')
	skipped_header = false
	for line in f:
		if skipped_header:
			if line == '':
				break
			words = line.split(' ')
			benchmark = words[0]
			num_machines = words[1]
			num_vertices = words[2]
			for i in range(3, len(words)):
				time_taken = words[3 + i]
				c.execute("INSERT INTO data VALUES (?,?,?,?)", (benchmark, num_machines, num_vertices, time_taken))
		elif line == 'started benchmark':
			skipped_header = true
	f.close()

# Save (commit) the changes
conn.commit()

for row in c.execute('SELECT * FROM data ORDER BY benchmark'):
	print row

# We can also close the connection if we are done with it.
# Just be sure any changes have been committed or they will be lost.
conn.close()