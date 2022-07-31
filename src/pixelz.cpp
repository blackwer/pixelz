// MIT License
//
// Copyright (c) 2022 Robert Blackwell
// Initial ECS implementation from Austin Morlan as a jumping off point
// https://code.austinmorlan.com/austin/ecs
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is furnished
// to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice (including the next
// paragraph) shall be included in all copies or substantial portions of the
// Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS
// OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
// OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include <raylib-cpp.hpp>

#include <array>
#include <bitset>
#include <chrono>
#include <iostream>
#include <memory>
#include <queue>
#include <random>
#include <set>
#include <unordered_map>

namespace pixelz {
raylib::Window window(1920, 1080, "pixelz");

using Entity = std::uint32_t;
constexpr Entity MAX_ENTITIES = 5000;

using ComponentType = std::uint8_t;
constexpr ComponentType MAX_COMPONENTS = 32;

using Signature = std::bitset<MAX_COMPONENTS>;

struct Transform {
    raylib::Vector2 position{0.0, 0.0};
    float rotation = 0.0;
    float scale = 0.0;
};

struct Gravity {
    raylib::Vector2 force{0.0, 0.0};
};

struct RigidBody {
    raylib::Vector2 velocity;
    raylib::Vector2 acceleration;
};

struct Pixel {
    raylib::Color color;
};

class EntityManager {
  public:
    EntityManager() {
        for (Entity entity = 0; entity < MAX_ENTITIES; ++entity)
            available_entities_.push(entity);
    }

    Entity create_entity() {
        Entity id = available_entities_.front();
        available_entities_.pop();
        ++living_entity_count_;

        return id;
    }

    void destroy_entity(Entity entity) {
        signatures_[entity].reset();
        available_entities_.push(entity);
        --living_entity_count_;
    }

    void set_signature(Entity entity, Signature signature) { signatures_[entity] = signature; }

    Signature get_signature(Entity entity) { return signatures_[entity]; }

  private:
    std::queue<Entity> available_entities_{};
    std::array<Signature, MAX_ENTITIES> signatures_{};
    std::uint32_t living_entity_count_{};
};

// The one instance of virtual inheritance in the entire implementation.
// An interface is needed so that the ComponentManager (seen later)
// can tell a generic ComponentArray that an entity has been destroyed
// and that it needs to update its array mappings.
class IComponentArray {
  public:
    virtual ~IComponentArray() = default;
    virtual void entity_destroyed(Entity entity) = 0;
};

template <typename T>
class ComponentArray : public IComponentArray {
  public:
    void insert_data(Entity entity, T component) {
        // Put new entry at end and update the maps
        size_t new_index = size_;
        entity_to_index_map_[entity] = new_index;
        index_to_entity_map_[new_index] = entity;
        component_array_[new_index] = component;
        ++size_;
    }

    void remove_data(Entity entity) {
        // Copy element at end into deleted element's place to maintain density
        size_t indexOfRemovedEntity = entity_to_index_map_[entity];
        size_t indexOfLastElement = size_ - 1;
        component_array_[indexOfRemovedEntity] = component_array_[indexOfLastElement];

        // Update map to point to moved spot
        Entity entityOfLastElement = index_to_entity_map_[indexOfLastElement];
        entity_to_index_map_[entityOfLastElement] = indexOfRemovedEntity;
        index_to_entity_map_[indexOfRemovedEntity] = entityOfLastElement;

        entity_to_index_map_.erase(entity);
        index_to_entity_map_.erase(indexOfLastElement);

        --size_;
    }

    T &get_data(Entity entity) {
        // Return a reference to the entity's component
        return component_array_[entity_to_index_map_[entity]];
    }

    void entity_destroyed(Entity entity) override {
        if (entity_to_index_map_.find(entity) != entity_to_index_map_.end()) {
            // Remove the entity's component if it existed
            remove_data(entity);
        }
    }

  private:
    // The packed array of components (of generic type T),
    // set to a specified maximum amount, matching the maximum number
    // of entities allowed to exist simultaneously, so that each entity
    // has a unique spot.
    std::array<T, MAX_ENTITIES> component_array_;

    // Map from an entity ID to an array index.
    std::unordered_map<Entity, size_t> entity_to_index_map_;

    // Map from an array index to an entity ID.
    std::unordered_map<size_t, Entity> index_to_entity_map_;

    // Total size of valid entries in the array.
    size_t size_;
};

class ComponentManager {
  public:
    template <typename T>
    void register_component() {
        const char *type_name = typeid(T).name();

        // Add this component type to the component type map
        component_types_.insert({type_name, next_component_type});

        // Create a ComponentArray pointer and add it to the component arrays map
        component_arrays_.insert({type_name, std::make_shared<ComponentArray<T>>()});

        // Increment the value so that the next component registered will be different
        ++next_component_type;
    }

    template <typename T>
    ComponentType get_component_type() {
        const char *type_name = typeid(T).name();

        // Return this component's type - used for creating signatures
        return component_types_[type_name];
    }

    template <typename T>
    void add_component(Entity entity, T component) {
        // Add a component to the array for an entity
        get_component_array<T>()->insert_data(entity, component);
    }

    template <typename T>
    void remove_component(Entity entity) {
        // Remove a component from the array for an entity
        get_component_array<T>()->RemoveData(entity);
    }

    template <typename T>
    T &get_component(Entity entity) {
        // Get a reference to a component from the array for an entity
        return get_component_array<T>()->get_data(entity);
    }

    void entity_destroyed(Entity entity) {
        // Notify each component array that an entity has been destroyed
        // If it has a component for that entity, it will remove it
        for (auto const &pair : component_arrays_) {
            auto const &component = pair.second;

            component->entity_destroyed(entity);
        }
    }

  private:
    // Map from type string pointer to a component type
    std::unordered_map<const char *, ComponentType> component_types_{};

    // Map from type string pointer to a component array
    std::unordered_map<const char *, std::shared_ptr<IComponentArray>> component_arrays_{};

    // The component type to be assigned to the next registered component - starting at 0
    ComponentType next_component_type{};

    // Convenience function to get the statically casted pointer to the ComponentArray of type T.
    template <typename T>
    std::shared_ptr<ComponentArray<T>> get_component_array() {
        const char *type_name = typeid(T).name();

        return std::static_pointer_cast<ComponentArray<T>>(component_arrays_[type_name]);
    }
};

class System {
  public:
    std::set<Entity> entities_;
};

class SystemManager {
  public:
    template <typename T>
    std::shared_ptr<T> register_system() {
        const char *type_name = typeid(T).name();

        // Create a pointer to the system and return it so it can be used externally
        auto system = std::make_shared<T>();
        systems_.insert({type_name, system});
        return system;
    }

    template <typename T>
    void set_signature(Signature signature) {
        const char *type_name = typeid(T).name();

        // Set the signature for this system
        signatures_.insert({type_name, signature});
    }

    void entity_destroyed(Entity entity) {
        // Erase a destroyed entity from all system lists
        // mEntities is a set so no check needed
        for (auto const &pair : systems_) {
            auto const &system = pair.second;

            system->entities_.erase(entity);
        }
    }

    void entity_signature_changed(Entity entity, Signature entity_signature) {
        // Notify each system that an entity's signature changed
        for (auto const &pair : systems_) {
            auto const &type = pair.first;
            auto const &system = pair.second;
            auto const &system_signature = signatures_[type];

            // Entity signature matches system signature - insert into set
            if ((entity_signature & system_signature) == system_signature) {
                system->entities_.insert(entity);
            }
            // Entity signature does not match system signature - erase from set
            else {
                system->entities_.erase(entity);
            }
        }
    }

  private:
    // Map from system type string pointer to a signature
    std::unordered_map<const char *, Signature> signatures_{};

    // Map from system type string pointer to a system pointer
    std::unordered_map<const char *, std::shared_ptr<System>> systems_{};
};

class Coordinator {
  public:
    void init() {
        // Create pointers to each manager
        component_manager_ = std::make_unique<ComponentManager>();
        entity_manager_ = std::make_unique<EntityManager>();
        system_manager_ = std::make_unique<SystemManager>();
    }

    // Entity methods
    Entity create_entity() { return entity_manager_->create_entity(); }

    void destroy_entity(Entity entity) {
        entity_manager_->destroy_entity(entity);

        component_manager_->entity_destroyed(entity);

        system_manager_->entity_destroyed(entity);
    }

    // Component methods
    template <typename T>
    void register_component() {
        component_manager_->register_component<T>();
    }

    template <typename T>
    void add_component(Entity entity, T component) {
        component_manager_->add_component<T>(entity, component);

        auto signature = entity_manager_->get_signature(entity);
        signature.set(component_manager_->get_component_type<T>(), true);
        entity_manager_->set_signature(entity, signature);

        system_manager_->entity_signature_changed(entity, signature);
    }

    template <typename T>
    void remove_component(Entity entity) {
        component_manager_->remove_component<T>(entity);

        auto signature = entity_manager_->get_signature(entity);
        signature.set(component_manager_->get_component_type<T>(), false);
        entity_manager_->set_signature(entity, signature);

        system_manager_->entity_signature_changed(entity, signature);
    }

    template <typename T>
    T &get_component(Entity entity) {
        return component_manager_->get_component<T>(entity);
    }

    template <typename T>
    ComponentType get_component_type() {
        return component_manager_->get_component_type<T>();
    }

    // System methods
    template <typename T>
    std::shared_ptr<T> register_system() {
        return system_manager_->register_system<T>();
    }

    template <typename T>
    void set_system_signature(Signature signature) {
        system_manager_->set_signature<T>(signature);
    }

  private:
    std::unique_ptr<ComponentManager> component_manager_;
    std::unique_ptr<EntityManager> entity_manager_;
    std::unique_ptr<SystemManager> system_manager_;
};

Coordinator gCoordinator;

class PhysicsSystem : public System {
  public:
    void init(){};
    void update(float dt) {
        for (auto const &entity : entities_) {
            auto &rigidBody = gCoordinator.get_component<RigidBody>(entity);
            auto &transform = gCoordinator.get_component<Transform>(entity);
            auto const &gravity = gCoordinator.get_component<Gravity>(entity);

            transform.position += rigidBody.velocity * dt;

            rigidBody.velocity += gravity.force * dt;
        }
    };
};

class RenderSystem : public System {
  public:
    void init();
    void update(float dt) {
        for (auto const &entity : entities_) {
            auto &transform = gCoordinator.get_component<Transform>(entity);
            auto &pixel = gCoordinator.get_component<Pixel>(entity);
            pixel.color.DrawRectangle(transform.position.x, window.GetHeight() - transform.position.y, 4, 4);
        }
    };

  private:
    // void WindowSizeListener(Event &event);
    // std::unique_ptr<Shader> shader;
    Entity camera_;
};

} // namespace pixelz

int main() {
    SetTargetFPS(60);

    using namespace pixelz;
    gCoordinator.init();

    gCoordinator.register_component<Gravity>();
    gCoordinator.register_component<RigidBody>();
    gCoordinator.register_component<pixelz::Transform>();
    gCoordinator.register_component<pixelz::Pixel>();

    auto physicsSystem = gCoordinator.register_system<PhysicsSystem>();
    auto renderSystem = gCoordinator.register_system<RenderSystem>();
    {
        Signature signature;
        signature.set(gCoordinator.get_component_type<Gravity>());
        signature.set(gCoordinator.get_component_type<RigidBody>());
        signature.set(gCoordinator.get_component_type<pixelz::Transform>());
        gCoordinator.set_system_signature<PhysicsSystem>(signature);
    }
    {
        Signature signature;
        signature.set(gCoordinator.get_component_type<pixelz::Transform>());
        signature.set(gCoordinator.get_component_type<pixelz::Pixel>());
        gCoordinator.set_system_signature<RenderSystem>(signature);
    }

    std::vector<Entity> entities(MAX_ENTITIES);

    std::default_random_engine generator;
    std::uniform_real_distribution<float> randX(0.0f, window.GetWidth());
    std::uniform_real_distribution<float> randY(0.0f, window.GetHeight());
    std::uniform_real_distribution<float> randRotation(0.0f, 3.1415926f);
    std::uniform_real_distribution<float> randScale(1.0f, 2.0f);
    std::uniform_real_distribution<float> randGravity(-10.0f, -1.0f);
    std::uniform_int_distribution<uint8_t> randColor(0, 255);

    for (auto &entity : entities) {
        float scale = randScale(generator);

        entity = gCoordinator.create_entity();

        gCoordinator.add_component(entity, Gravity{.force = {0.0f, randGravity(generator)}});
        gCoordinator.add_component(entity, RigidBody{.velocity = {0.0f, 0.0f}, .acceleration = {0.0f, 0.0f}});
        gCoordinator.add_component(entity, pixelz::Transform{.position = {randX(generator), randY(generator)},
                                                             .rotation = randRotation(generator),
                                                             .scale = scale});
        gCoordinator.add_component(entity,
                                   pixelz::Pixel{.color = raylib::Color(randColor(generator), randColor(generator),
                                                                        randColor(generator), 255)});
    }
    physicsSystem->init();

    float dt = 0.0f;
    while (!window.ShouldClose()) {
        window.BeginDrawing();
        {
            auto startTime = std::chrono::high_resolution_clock::now();
            window.ClearBackground(BLACK);

            physicsSystem->update(dt);
            renderSystem->update(dt);

            auto stopTime = std::chrono::high_resolution_clock::now();

            dt = std::chrono::duration<float, std::chrono::seconds::period>(stopTime - startTime).count() * 20;
        }
        window.EndDrawing();
    }
}
