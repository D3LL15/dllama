package com.dllama;

import org.neo4j.graphdb.*;
import org.neo4j.graphdb.factory.GraphDatabaseFactory;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;
import java.util.ArrayDeque;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Queue;

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
			tx1.success();
		} finally {
			tx1.close();
		}
	}

	private void addNodes(int numIterations, int numNodes) {
		System.out.format("neo4j_add_nodes 1 %d ", numNodes * 10);
		for (int j = 0; j < numIterations; j++) {
			long t1 = System.nanoTime();
			org.neo4j.graphdb.Transaction tx = graphDb.beginTx();
			try {
				for (int num = 0; num < numNodes*10; num++) {
					Node newNode = graphDb.createNode();
					newNode.setProperty("num", num);
				}
				tx.success();
			} finally {
				tx.close();
			}
			long t2 = System.nanoTime();
			long duration = (t2 - t1)/1000;
			System.out.format("%d ", duration);
			deleteDB();
		}
		System.out.println("");
	}

	private void addEdges(int numIterations, int numNodes) {
		System.out.format("neo4j_add_edges 1 %d ", numNodes);
		//int numNodes = 10000;
		for (int j = 0; j < numIterations; j++) {
			long t1 = 0;
			org.neo4j.graphdb.Transaction tx = graphDb.beginTx();
			try {
				Node[] nodes = new Node[numNodes];
				for (int num = 0; num < numNodes; num++) {
					nodes[num] = graphDb.createNode();
					nodes[num].setProperty("num", num);
				}
				t1 = System.nanoTime();
				for (int num = 0; num < numNodes; num++) {
					for (int i = 0; i < 100; i++) {
						Relationship relationship = nodes[num].createRelationshipTo(nodes[i], RelTypes.EDGE);
					}
				}
				tx.success();
			}
				finally {
				tx.close();
			}
			long t2 = System.nanoTime();
			long duration = (t2 - t1)/1000;
			System.out.format("%d ", duration);
			deleteDB();
		}
		System.out.println("");
	}

	private void readEdges(int numIterations, int numNodes) {
		System.out.format("neo4j_read_edges 1 %d ", numNodes);
		//int numNodes = 10000;

		for (int j = 0; j < numIterations; j++) {
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
				tx.success();
			}
			finally {
				tx.close();
			}

			tx = graphDb.beginTx();
			try {

				long t1 = System.nanoTime();

				ResourceIterator<Node> readNodes = graphDb.getAllNodes().iterator();
				while (readNodes.hasNext()) {
					Node n = readNodes.next();
					Iterator<Relationship> edges = n.getRelationships().iterator();
					while (edges.hasNext()) {
						edges.next();
					}
				}

				long t2 = System.nanoTime();
				long duration = (t2 - t1) / 1000;
				System.out.format("%d ", duration);
				//deleteDB();
				tx.success();
			}
			finally{
				tx.close();
			}
			deleteDB();
		}
		System.out.println("");
		//deleteDB();
	}

	private void addAndReadGraph(int numberOfNodes, String fileName, int numIterations) {
		for (int j = 0; j < numIterations; j++) {

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

				tx.success();
			}
			catch (IOException e) {
				e.printStackTrace();
			}
			finally {
				tx.close();
			}

			long t1 = System.nanoTime();
			tx = graphDb.beginTx();
			try {
				ResourceIterator<Node> readNodes = graphDb.getAllNodes().iterator();
				while (readNodes.hasNext()) {
					Node n = readNodes.next();
					Iterator<Relationship> edges = n.getRelationships().iterator();
					while (edges.hasNext()) {
						edges.next();
					}
				}
				tx.success();
			}
			finally {
				tx.close();
			}
			long t2 = System.nanoTime();
			long duration = (t2 - t1)/1000;
			System.out.format("%d ", duration);
			deleteDB();
		}
		System.out.println("");
	}

	private void addAndReadPowerGraph(String directory, int numIterations) {
		System.out.print("neo4j_power 1 1000 ");
		addAndReadGraph(1000, directory + "/powerlaw.net", numIterations);
	}

	private void addAndReadKroneckerGraph(String directory, int numIterations) {
		System.out.print("neo4j_kronecker 1 1024 ");
		addAndReadGraph(1024, directory + "/kronecker_graph.net", numIterations);
	}

	private void addAndReadPowerGraph2(String directory, int numIterations) {
		System.out.print("neo4j_large_power 1 50000 ");
		addAndReadGraph(50000, directory + "/powerlaw2.net", numIterations);
	}

	private void addAndReadKroneckerGraph2(String directory, int numIterations) {
		System.out.print("neo4j_large_kronecker 1 131072 ");
		addAndReadGraph(131072, directory + "/krongraph2.net", numIterations);
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
			tx1.success();
		} finally {
			tx1.close();
		}
	}

	private static enum RelTypes implements RelationshipType
	{
		EDGE
	}

	private void breadthFirstSearch(String directory, int numIterations) {
		System.out.print("neo4j_breadth_first 1 50000 ");
		int numberOfNodes = 50000;
		long startID = 0;
		long endID = 0;

		long[] nodeIds = new long[numberOfNodes];

		org.neo4j.graphdb.Transaction tx = graphDb.beginTx();
		try {
			BufferedReader reader;
			//reader = new BufferedReader(new FileReader(directory + "/kronecker_graph.net"));
			reader = new BufferedReader(new FileReader(directory + "/powerlaw2.net"));
			String line = reader.readLine();
			String[] fileNodes;

			Node[] nodes = new Node[numberOfNodes];
			for (int num = 0; num < numberOfNodes; num++) {
				nodes[num] = graphDb.createNode();
				nodes[num].setProperty("num", num);
				nodeIds[num] = nodes[num].getId();
				/*if (nodes[num].getId() == 724) {
					System.out.format("end node actual number = %d\n", num);
				}*/
			}
			startID = nodes[0].getId();
			endID = nodes[692].getId();

			while (line != null) {
				fileNodes = line.split("	");
				//System.out.format("from: %d to: %d\n", Integer.parseInt(fileNodes[0]), Integer.parseInt(fileNodes[1]));
				Relationship relationship = nodes[Integer.parseInt(fileNodes[0])].createRelationshipTo(nodes[Integer.parseInt(fileNodes[1])], RelTypes.EDGE);
				line = reader.readLine();
			}
			reader.close();
			tx.success();
		}
		catch (IOException e) {
			e.printStackTrace();
		}
		finally {
			tx.close();
		}

		for (int j = 0; j < numIterations*5; j++) {

			long t1 = System.nanoTime();
			tx = graphDb.beginTx();
			try {
				Queue<Node> nextNodes = new ArrayDeque<Node>();
				HashSet<Node> seenNodes = new HashSet<Node>();

				Node n = graphDb.getNodeById(startID);
				//System.out.format("started at node %d\n", n.getId());
				seenNodes.add(n);

				boolean foundNode = false;
				while (!foundNode) {
					Iterator<Relationship> neighbours = n.getRelationships().iterator();

					while (neighbours.hasNext()) {
						Relationship r = neighbours.next();
						Node endNode = r.getEndNode();
						//if (endNode.getId() == endID) {
						if (endNode.getId() == nodeIds[692 + j]) {
							//System.out.println("found node");
							foundNode = true;
							break;
						}
						if (!seenNodes.contains(endNode)) {
							nextNodes.add(endNode);
							seenNodes.add(endNode);
						}
					}

					if (nextNodes.isEmpty()) {
						break;
					}

					n = nextNodes.remove();
				}

				/*for (int i = 0; i < 10000; i++) {
					Iterator<Relationship> neighbours = n.getRelationships().iterator();
					//visitedNodes.add(n);

					while (neighbours.hasNext()) {
						Relationship r = neighbours.next();
						Node endNode = r.getEndNode();
						if (!seenNodes.contains(endNode)) {
							nextNodes.add(endNode);
							seenNodes.add(endNode);
						}
					}

					if (nextNodes.isEmpty()) {
						System.out.format("reached %d nodes\n", i);
						break;
					}

					n = nextNodes.remove();
				}
				System.out.format("last node = %d\n", n.getId());*/


				tx.success();
			}
			finally {
				tx.close();
			}
			long t2 = System.nanoTime();
			long duration = (t2 - t1)/1000;
			System.out.format("%d ", duration);

		}
		deleteDB();
		System.out.println("");
	}

	public static void main(String... args)
	{
		//run clean compile then assembly:single

		if (args.length == 5) {
			Benchmark benchmark = new Benchmark(args[1]);
			int numIterations = Integer.parseInt(args[2]);
			int numNodes = Integer.parseInt(args[3]);
			switch (args[4].charAt(0)) {
				case '1':
					benchmark.addNodes(numIterations, numNodes);
					benchmark.addEdges(numIterations, numNodes);
					benchmark.readEdges(numIterations, numNodes);
					benchmark.addAndReadPowerGraph(args[0], numIterations);
					benchmark.addAndReadKroneckerGraph(args[0], numIterations);
					benchmark.breadthFirstSearch(args[0], numIterations);
					break;
				case '2':
					benchmark.addNodes(numIterations, numNodes);
					break;
				case '3':
					benchmark.addEdges(numIterations, numNodes);
					break;
				case '4':
					benchmark.readEdges(numIterations, numNodes);
					break;
				case '5':
					benchmark.addAndReadPowerGraph(args[0], numIterations);
					break;
				case '6':
					benchmark.addAndReadKroneckerGraph(args[0], numIterations);
					break;
				case '7':
					benchmark.breadthFirstSearch(args[0], numIterations);
					break;
				default:
					break;
			}

			benchmark.graphDb.shutdown();

		} else {
			System.out.println("Please provide the directory containing the example graph files then the directory" +
					" in which you would like to store the graph database then num nodes, then the benchmark you want");
		}
	}
}