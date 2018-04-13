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
Client side prediction subsystem.
Works alongside the Network subsystem.
Usage:
Add LOCAL nodes which you want to be predicted.
Note: Uses the PhysicsWorld Fps as a fixed timestep.
*/
struct StateSnapshot
{
	// Server: Add a node to the client side prediction
	void add_node(Node* node);

	/*
	state serialization structure:
	- Last input ID
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
	*/
	// Process scene update received from the server
	void read_state(MemoryBuffer& message, Scene* scene);
	void read_node(MemoryBuffer& message, Scene* scene);
	void read_component(MemoryBuffer& message, Node* node);

	// Get a complete network state snapshot
	void write_state(VectorBuffer& message, Scene* scene);
	void write_node(VectorBuffer& message, Node& node);
	void write_component(VectorBuffer& message, Component& component);

	// Write all the network attributes
	void write_network_attributes(Serializable& object, Serializer& dest);
	// Read all the network attributes
	void read_network_attributes(Serializable& object, Deserializer& source);

	// Intercept network attributes to avoid replication overriding prediction
	void set_intercept_network_attributes(Serializable& object);


protected:
	// snapshot nodes
	std::vector<Node*> nodes;
	// Reuseable hash set for tracking unused nodes
	std::unordered_set<Node*> unused_nodes;
};
