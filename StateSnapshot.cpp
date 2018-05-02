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
	nodes.emplace_back(node);
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


void StateSnapshot::read_node(MemoryBuffer& message, Node* parent)
{
	auto node_id = message.ReadUInt();
	auto node = parent->GetScene()->GetNode(node_id);

	// Create the node if it doesn't exist
	bool new_node = false;
	if (!node)
	{
		new_node = true;
		// Add initially to the root level. May be moved as we receive the parent attribute
		node = parent->CreateChild(node_id, LOCAL);
		// Create smoothed transform component
		//node->CreateComponent<SmoothedTransform>();
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

	// read child nodes
	read_child_nodes(message, node);
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
		if (component) {
			component->Remove();
		}
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

void StateSnapshot::read_child_nodes(MemoryBuffer & message, Node * parent)
{
	// Read number of nodes
	auto num_nodes = message.ReadVLE();

	// Read nodes
	for (; num_nodes-- > 0;)
		read_node(message, parent);
}


void StateSnapshot::write_state(VectorBuffer& message, Scene* scene)
{
	// Write number of nodes
	message.WriteVLE(nodes.size());

	// remove expired nodes
	nodes.erase(std::remove_if(nodes.begin(), nodes.end(), [](WeakPtr<Node> const& node) {
		return node.Expired();
	}), nodes.end());

	// Write nodes
	for (auto node : nodes) {
		auto sp_node = node.Lock();
		write_node(message, *node);
	}
}


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

	// write child nodes
	write_child_nodes(message, node);
}


void StateSnapshot::write_component(VectorBuffer& message, Component& component)
{
	// Write ID
	message.WriteUInt(component.GetID());
	// Write type
	message.WriteStringHash(component.GetType());
	// Write attributes
	write_network_attributes(component, message);
}

void StateSnapshot::write_child_nodes(VectorBuffer & message, Node & parent)
{
	const auto& child_nodes = parent.GetChildren();

	// write number of children
	message.WriteVLE(child_nodes.Size());

	// write nodes
	for (auto node : child_nodes) {
		write_node(message, *node);
	}
}


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
		if (attr.name_ == "Network Parent Node") // avoid overriding the actual parent
			continue;

		value.Clear();
		object.OnGetAttribute(attr, value);
		dest.WriteVariantData(value);
	}
}


void StateSnapshot::read_network_attributes(Serializable& object, Deserializer& source)
{
	const auto attributes = object.GetNetworkAttributes();
	if (!attributes)
		return;

	const auto numAttributes = attributes->Size();

	for (unsigned i = 0; i < numAttributes && !source.IsEof(); ++i)
	{
		const auto& attr = attributes->At(i);
		if (attr.name_ == "Network Parent Node") // avoid overriding the actual parent
			continue;

		object.OnSetAttribute(attr, source.ReadVariant(attr.type_));
	}
}
