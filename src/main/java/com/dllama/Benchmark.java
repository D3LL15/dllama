package com.dllama;

import org.neo4j.graphdb.*;
import org.neo4j.graphdb.factory.GraphDatabaseFactory;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;

public class Benchmark
{
	GraphDatabaseService graphDb;

	public Benchmark(String directory) {
		File dbDir = new File(directory + "/myNeo4jDatabase");
		graphDb = new GraphDatabaseFactory().newEmbeddedDatabase(dbDir);
		registerShutdownHook(graphDb);

		org.neo4j.graphdb.Transaction tx1 = graphDb.beginTx();
		try {
			graphDb.execute("MATCH (n) DETACH DELETE n");

		} finally {
			tx1.close();
		}
	}

	private void addNodes() {
		System.out.println("microseconds to add 10000 nodes");
		for (int j = 0; j < 10; j++) {
			long t1 = System.nanoTime();
			org.neo4j.graphdb.Transaction tx = graphDb.beginTx();
			try {
				for (int num = 0; num < 10000; num++) {
					Node newNode = graphDb.createNode();
					newNode.setProperty("num", num);
				}

			} finally {
				tx.close();
			}
			long t2 = System.nanoTime();
			long duration = (t2 - t1)/1000;
			System.out.print(duration + " ");
			deleteDB();
		}
		System.out.println("");
	}

	private void addEdges() {
		System.out.println("microseconds to add 100 edges to 10000 nodes");
		int numNodes = 10000;
		for (int j = 0; j < 10; j++) {
			long t1 = System.nanoTime();
			org.neo4j.graphdb.Transaction tx = graphDb.beginTx();
			try {
				Node[] nodes = new Node[numNodes];
				for (int num = 0; num < numNodes; num++) {
					nodes[num] = graphDb.createNode();
					nodes[num].setProperty("num", num);
				}

				for (int num = 0; num < numNodes; num++) {
					for (int i = 0; i < 100; i++) {
						Relationship relationship = nodes[num].createRelationshipTo(nodes[i], RelTypes.EDGE);
					}
				}
			}
			finally {
				tx.close();
			}
			long t2 = System.nanoTime();
			long duration = (t2 - t1)/1000;
			System.out.print(duration + " ");
			deleteDB();
		}
		System.out.println("");
	}

	private void readEdges() {
		System.out.println("microseconds to read 100 edges from each of 10000 nodes");
		int numNodes = 10000;

		org.neo4j.graphdb.Transaction tx = graphDb.beginTx();
		try {
			Node[] nodes = new Node[numNodes];
			for (int num = 0; num < numNodes; num++) {
				nodes[num] = graphDb.createNode();
				nodes[num].setProperty("num", num);
			}

			for (int num = 0; num < numNodes; num++) {
				for (int i = 0; i < 100; i++) {
					Relationship relationship = nodes[num].createRelationshipTo(nodes[i], RelTypes.EDGE);
				}
			}
		}
		finally {
			tx.close();
		}

		tx = graphDb.beginTx();
		try {
			for (int j = 0; j < 10; j++) {
				long t1 = System.nanoTime();

				ResourceIterator<Node> readNodes = graphDb.getAllNodes().iterator();
				while (readNodes.hasNext()) {
					Node n = readNodes.next();
					n.getRelationships().iterator();
				}

				long t2 = System.nanoTime();
				long duration = (t2 - t1)/1000;
				System.out.print(duration + " ");
			}
		}
		finally {
			tx.close();
		}
		System.out.println("");
		deleteDB();
	}

	private void addAndReadGraph(int numberOfNodes, String fileName) {
		for (int j = 0; j < 10; j++) {
			long t1 = System.nanoTime();

			org.neo4j.graphdb.Transaction tx = graphDb.beginTx();
			try {
				BufferedReader reader;
				reader = new BufferedReader(new FileReader(fileName));
				String line = reader.readLine();
				String[] fileNodes;

				Node[] nodes = new Node[numberOfNodes];
				for (int num = 0; num < numberOfNodes; num++) {
					nodes[num] = graphDb.createNode();
					nodes[num].setProperty("num", num);
				}

				while (line != null) {
					fileNodes = line.split("	");
					Relationship relationship = nodes[Integer.parseInt(fileNodes[0])].createRelationshipTo(nodes[Integer.parseInt(fileNodes[1])], RelTypes.EDGE);
					line = reader.readLine();
				}
				reader.close();
			}
			catch (IOException e) {
				e.printStackTrace();
			}
			finally {
				tx.close();
			}

			tx = graphDb.beginTx();
			try {
				ResourceIterator<Node> readNodes = graphDb.getAllNodes().iterator();
				while (readNodes.hasNext()) {
					Node n = readNodes.next();
					n.getRelationships().iterator();
				}
			}
			finally {
				tx.close();
			}
			long t2 = System.nanoTime();
			long duration = (t2 - t1)/1000;
			System.out.print(duration + " ");
			deleteDB();
		}
		System.out.println("");
	}

	private void addAndReadPowerGraph(String directory) {
		System.out.println("microseconds to add power law graph then read all edges from each node");
		addAndReadGraph(1024, directory + "/kronecker_graph.net");
	}

	private void addAndReadKroneckerGraph(String directory) {
		System.out.println("microseconds to add kronecker graph then read all edges from each node");
		addAndReadGraph(1000, directory + "/powerlaw.net");
	}

	private static void registerShutdownHook( final GraphDatabaseService graphDb )
	{
		// Registers a shutdown hook for the Neo4j instance so that it
		// shuts down nicely when the VM exits (even if you "Ctrl-C" the
		// running application).
		Runtime.getRuntime().addShutdownHook( new Thread()
		{
			@Override
			public void run()
			{
				graphDb.shutdown();
			}
		} );
	}

	public void deleteDB() {
		org.neo4j.graphdb.Transaction tx1 = graphDb.beginTx();
		try {
			graphDb.execute("MATCH (n) DETACH DELETE n");
		} finally {
			tx1.close();
		}
	}

	private static enum RelTypes implements RelationshipType
	{
		EDGE
	}

	public static void main(String... args)
	{
		//run clean compile then assembly:single

		if (args.length == 1) {
			Benchmark benchmark = new Benchmark(args[0]);
			benchmark.addNodes();
			benchmark.addEdges();
			benchmark.readEdges();
			benchmark.addAndReadPowerGraph(args[0]);
			benchmark.addAndReadKroneckerGraph(args[0]);
			benchmark.graphDb.shutdown();

		} else {
			System.out.println("Please provide the directory containing the example graph files");
		}
	}
}