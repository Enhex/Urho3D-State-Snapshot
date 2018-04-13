#include "StateSnapshot.h"

#include <Urho3D/Core/CoreEvents.h>
#include <Urho3D/IO/Log.h>
#include <Urho3D/IO/MemoryBuffer.h>
#include <Urho3D/IO/VectorBuffer.h>
#include <Urho3D/Network/Network.h>
#include <Urho3D/Network/NetworkEvents.h>
#include <Urho3D/Scene/LogicComponent.h>
#include <Urho3D/Scene/Scene.h>
#include <Urho3D/Scene/SmoothedTransform.h>


void StateSnapshot::add_node(Node* node)
{
	nodes.push_back(node);
}


void StateSnapshot::read_state(MemoryBuffer& message, Scene* scene)
{
	// Reset the unused nodes set
	unused_nodes.clear();
	for (auto node : nodes)
		unused_nodes.insert(node);

	// Read number of nodes
	auto num_nodes = message.ReadVLE();

	// Read nodes
	for (; num_nodes-- > 0;)
		read_node(message, scene);

	// Remove unsued nodes
	for (auto node : unused_nodes)
		node->Remove();
}


void StateSnapshot::read_node(MemoryBuffer& message, Scene* scene)
{
	auto node_id = message.ReadUInt();
	auto node = scene->GetNode(node_id);
	bool new_node = false;

	// Create the node if it doesn't exist
	if (!node)
	{
		new_node = true;
		// Add initially to the root level. May be moved as we receive the parent attribute
		node = scene->CreateChild(node_id, LOCAL);
		// Create smoothed transform component
		node->CreateComponent<SmoothedTransform>(LOCAL);
	}
	else
	{
		// Remove the node from the unused nodes list
		unused_nodes.erase(node);
	}

	// Read attributes
	read_network_attributes(*node, message);
	// ApplyAttributes() is deliberately skipped, as Node has no attributes that require late applying.
	// Furthermore it would propagate to components and child nodes, which is not desired in this case

	if (new_node)
	{
		// Snap the motion smoothing immediately to the end
		auto transform = node->GetComponent<SmoothedTransform>();
		if (transform)
			transform->Update(1.0f, 0.0f);

		// intercept updates
		//set_intercept_network_attributes(*node);
	}

	// Read user variables
	unsigned num_vars = message.ReadVLE();
	for (; num_vars > 0; --num_vars)
	{
		auto key = message.ReadStringHash();
		node->SetVar(key, message.ReadVariant());
	}

	// Read components
	unsigned num_components = message.ReadVLE();
	for (; num_components > 0; --num_components)
		read_component(message, node);
}

void StateSnapshot::read_component(MemoryBuffer& message, Node* node)
{
	// Read component ID
	auto componentID = message.ReadUInt();
	// Read component type
	auto type = message.ReadStringHash();

	// Check if the component by this ID and type already exists in this node
	auto component = node->GetScene()->GetComponent(componentID);
	if (!component || component->GetType() != type || component->GetNode() != node)
	{
		if (component)
			component->Remove();
		component = node->CreateComponent(type, LOCAL, componentID);
	}

	// If was unable to create the component, would desync the message and therefore have to abort
	if (!component)
	{
		URHO3D_LOGERROR("CreateNode message parsing aborted due to unknown component");
		return;
	}

	// Read attributes and apply
	read_network_attributes(*component, message);
	component->ApplyAttributes();
}


//
// write_scene_state
//
void StateSnapshot::write_state(VectorBuffer& message, Scene* scene)
{
	// Write number of nodes
	message.WriteVLE(nodes.size());

	// Write nodes
	for (auto node : nodes)
		write_node(message, *node);
}


//
// write_node
//
void StateSnapshot::write_node(VectorBuffer& message, Node& node)
{
	// Write node ID
	message.WriteUInt(node.GetID());

	// Write attributes
	write_network_attributes(node, message);

	// Write user variables
	const auto& vars = node.GetVars();
	message.WriteVLE(vars.Size());
	for (auto i = vars.Begin(); i != vars.End(); ++i)
	{
		message.WriteStringHash(i->first_);
		message.WriteVariant(i->second_);
	}

	// Write number of components
	message.WriteVLE(node.GetNumComponents());

	// Write components
	const auto& components = node.GetComponents();
	for (unsigned i = 0; i < components.Size(); ++i)
	{
		auto component = components[i];
		write_component(message, *component);
	}
}


//
// write_component
//
void StateSnapshot::write_component(VectorBuffer& message, Component& component)
{
	// Write ID
	message.WriteUInt(component.GetID());
	// Write type
	message.WriteStringHash(component.GetType());
	// Write attributes
	write_network_attributes(component, message);
}


//
// write_network_attributes
//
void StateSnapshot::write_network_attributes(Serializable& object, Serializer& dest)
{
	const auto attributes = object.GetNetworkAttributes();
	if (!attributes)
		return;

	const auto numAttributes = attributes->Size();
	Variant value;

	for (unsigned i = 0; i < numAttributes; ++i)
	{
		const auto& attr = attributes->At(i);
		value.Clear();
		object.OnGetAttribute(attr, value);
		dest.WriteVariantData(value);
	}
}


//
// read_network_attributes
//
void StateSnapshot::read_network_attributes(Serializable& object, Deserializer& source)
{
	const auto attributes = object.GetNetworkAttributes();
	if (!attributes)
		return;

	const auto numAttributes = attributes->Size();

	for (unsigned i = 0; i < numAttributes && !source.IsEof(); ++i)
	{
		const auto& attr = attributes->At(i);
		object.OnSetAttribute(attr, source.ReadVariant(attr.type_));
	}
}

void StateSnapshot::set_intercept_network_attributes(Serializable & object)
{
	const auto attributes = object.GetNetworkAttributes();
	if (!attributes)
		return;

	const auto numAttributes = attributes->Size();

	for (unsigned i = 0; i < numAttributes; ++i) {
		const auto& attr = attributes->At(i);
		object.SetInterceptNetworkUpdate(attr.name_, true);
	}
}