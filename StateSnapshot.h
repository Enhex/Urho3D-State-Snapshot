#pragma once

#include <Urho3D/Container/HashSet.h>
#include <Urho3D/Core/Object.h>
#include <Urho3D/IO/VectorBuffer.h>
#include <functional>
#include <unordered_set>
#include <vector>

namespace Urho3D
{
	class Context;
	class Serializable;
	class Component;
	class Scene;
	class Node;
	class Connection;
	class MemoryBuffer;
}

using namespace Urho3D;

/*
Used to create full state snapshots for its nodes.

TODO:
creating child nodes
*/
struct StateSnapshot
{
	// snapshot nodes
	std::vector<WeakPtr<Node>> nodes;

	void add_node(Node* node);

	/*
	state serialization structure:
	- Last input ID
	(1):
	- num of nodes
	- for each node
		- ID (unsigned to include local nodes)
		- attributes
		- User variables
		- num of components
		- for each component
			- ID (unsigned)
			- type
			- attributes
		- child nodes: recursive, go back to (1)
	*/
	// Process scene update received from the server
	void read_state(MemoryBuffer& message, Scene* scene);
	void read_node(MemoryBuffer& message, Node* parent);
	void read_component(MemoryBuffer& message, Node* node);
	void read_child_nodes(MemoryBuffer& message, Node* parent);

	// Get a complete network state snapshot
	void write_state(VectorBuffer& message, Scene* scene);
	void write_node(VectorBuffer& message, Node& node);
	void write_component(VectorBuffer& message, Component& component);
	void write_child_nodes(VectorBuffer& message, Node& parent);

	// Write all the network attributes
	void write_network_attributes(Serializable& object, Serializer& dest);
	// Read all the network attributes
	void read_network_attributes(Serializable& object, Deserializer& source);


protected:
	// Reuseable hash set for tracking unused nodes
	std::unordered_set<Node*> unused_nodes;
};
