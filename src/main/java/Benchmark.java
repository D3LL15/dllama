import org.neo4j.driver.v1.*;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;
import java.util.List;

import static org.neo4j.driver.v1.Values.parameters;

public class Benchmark
{
	// Driver objects are thread-safe and are typically made available application-wide.
	Driver driver;

	public Benchmark(String uri, String user, String password)
	{
		driver = GraphDatabase.driver(uri, AuthTokens.basic(user, password));
	}

	private void addNodes() {
		System.out.println("nanoseconds to add 1000 nodes");
		Session session = driver.session();
		try {
			for (int j = 0; j < 1; j++) {

				long t1 = System.nanoTime();
				for (int num = 0; num < 1000; num++) {
					session.run("CREATE (a:Node {num: {x}})", parameters("x", num));
				}
				long t2 = System.nanoTime();
				long duration = t2 - t1;
				System.out.print(duration + " ");
				session.run("MATCH (n:Node) DETACH DELETE n");
			}
			System.out.println("");
		} finally {
			session.close();
		}
	}

	private void addEdges() {
		System.out.println("nanoseconds to add 100 edges to 100 nodes");
		Session session = driver.session();
		try {
			for (int j = 0; j < 1; j++) {
				for (int num = 0; num < 100; num++) {
					session.run("CREATE (a:Node {num: {x}})", parameters("x", num));
				}
				long t1 = System.nanoTime();
				for (int num = 0; num < 100; num++) {
					for (int i = 0; i < 100; i++) {
						session.run("MATCH (a:Node), (b:node) WHERE a.num = {x} AND b.num = {y} CREATE (a)-[r:Edge]->(b)", parameters("x", num, "y", i));
					}
				}
				long t2 = System.nanoTime();
				long duration = t2 - t1;
				System.out.print(duration + " ");
				session.run("MATCH (n:Node) DETACH DELETE n");
			}
			System.out.println("");
		} finally {
			session.close();
		}
	}

	private void readEdges() {
		System.out.println("nanoseconds to read 100 edges each from 100 nodes");
		Session session = driver.session();
		try {
			for (int num = 0; num < 100; num++) {
				session.run("CREATE (a:Node {num: {x}})", parameters("x", num));
			}
			for (int num = 0; num < 100; num++) {
				for (int i = 0; i < 100; i++) {
					session.run("MATCH (a:Node), (b:Node) WHERE a.num = {x} AND b.num = {y} CREATE (a)-[r:Edge]->(b)", parameters("x", num, "y", i));
				}
			}
			for (int j = 0; j < 1; j++) {
				long t1 = System.nanoTime();

				for (int num = 0; num < 100; num++) {
					StatementResult result = session.run(
							"MATCH (a:Node)-[r:Edge]->(b:Node) WHERE a.num = {x} RETURN b.num AS number",
							parameters("x", num));
					List<Record> results = result.list();
				}

				long t2 = System.nanoTime();
				long duration = t2 - t1;
				System.out.print(duration + " ");
				session.run("MATCH (n) DETACH DELETE n");
			}
			System.out.println("");
		} finally {
			session.close();
		}
	}

	private void addAndReadGraph(int numberOfNodes, String fileName) {
		Session session = driver.session();
		try {
			for (int j = 0; j < 1; j++) {

				BufferedReader reader;
				reader = new BufferedReader(new FileReader(fileName));


				long t1 = System.nanoTime();
				/*for (int num = 0; num < numberOfNodes; num++) {
					session.run("CREATE (a:Node {num: {x}})", parameters("x", num));
				}*/

				String line = reader.readLine();
				String[] nodes;
				while (line != null) {
					nodes = line.split("	");
					session.run("MATCH (a:Node), (b:node) WHERE a.num = " + nodes[0] + " AND b.num = " + nodes[1] + " CREATE (a)-[r:Edge]->(b)");
					line = reader.readLine();
				}

				for (int num = 0; num < numberOfNodes; num++) {
					StatementResult result = session.run(
							"MATCH (a:Node)-[r:Edge]->(b:Node) WHERE a.num = {x} RETURN b.num AS number",
							parameters("x", num));
					List<Record> results = result.list();
				}

				long t2 = System.nanoTime();
				long duration = t2 - t1;
				System.out.print(duration + " ");
				session.run("MATCH (n:Node) DETACH DELETE n");
				reader.close();
			}
			System.out.println("");
		} catch (IOException e) {
			e.printStackTrace();
		} finally {
			session.close();
		}
	}

	private void addAndReadPowerGraph() {
		System.out.println("nanoseconds to add power law graph then read all edges from each node");
		addAndReadGraph(1024, "dllama/kronecker_graph.net");
	}

	private void addAndReadKroneckerGraph() {
		System.out.println("nanoseconds to add kronecker graph then read all edges from each node");
		addAndReadGraph(1000, "dllama/powerlaw.net");
	}

	public void close()
	{
		// Closing a driver immediately shuts down all open connections.
		driver.close();
	}

	public void deleteDatabase() {
		Session session = driver.session();
		session.run("MATCH (n:Node) DETACH DELETE n");
	}

	public void addIndex() {
		Session session = driver.session();
		session.run("CREATE INDEX ON :Node(num)");
	}

	public static void main(String... args)
	{
		Benchmark example = new Benchmark("bolt://localhost:7687", "neo4j", "password");
		example.addIndex();

		//example.deleteDatabase();

		example.addNodes();
		example.addEdges();
		example.readEdges();
		example.addAndReadKroneckerGraph();
		example.addAndReadPowerGraph();

		example.close();
	}
}